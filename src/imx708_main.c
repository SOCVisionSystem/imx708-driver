// SPDX-License-Identifier: GPL-2.0-only
/*
 * imx708_main.c - Sony IMX708 camera sensor V4L2 sub-device driver
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * This file implements the I2C driver probe/remove, V4L2 sub-device
 * registration, media entity setup, and the module init/exit. It owns
 * the driver lifecycle and the top-level device registration. It does
 * NOT contain any SoC-specific register offsets or hardware operations
 * — those live in imx708_platform.c behind the imx708_hw_ops interface.
 *
 * Locking:
 *   @lock (mutex) protects all sensor state: mode, format, streaming flag,
 *   power_count, and control values. It is taken in every ioctl, every
 *   subdev operation, and every sysfs show/store. The hardirq handler
 *   does not take @lock — it only latches events into pending_events
 *   (atomic). The threaded handler takes @lock before processing events.
 *
 *   Acquisition order: @lock first, then any regmap internal lock.
 *   No other locks are taken.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/idr.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-common.h>
#include <media/media-entity.h>

#include "imx708_platform.h"
#include "imx708_regs.h"
#include "imx708_uapi.h"
#include "imx708_trace.h"

#define DRV_NAME "imx708"

/* DRV_VERSION is passed via EXTRA_CFLAGS from the Makefile */

/* External symbols from imx708_chardev.c */
/*
 * Sub-module entry points. Declared in imx708_platform.h; the char device
 * region and class are owned by imx708_chardev.c.
 */
extern dev_t imx708_devt;
extern struct class *imx708_class;
#define IMX708_MAX_DEVICES 4

/* SoC descriptions from imx708_platform.c */
extern const struct imx708_soc_data imx708_soc_rpi;
extern const struct imx708_soc_data imx708_soc_rpi_wide;

/* Number of I2C retry attempts */
#define IMX708_I2C_RETRIES 3

/* Allocates the per-instance minor / debugfs / sysfs directory index. */
static DEFINE_IDA(imx708_ida);

/* ------------------------------------------------------------------ */
/* V4L2 sub-device operations                                         */
/* ------------------------------------------------------------------ */

static int imx708_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx708_dev *sensor = to_imx708_dev(sd);
	int ret = 0;

	/*
	 * Resume the device *before* taking @lock: the runtime-PM callbacks
	 * run with the PM core's own serialisation and must not contend for
	 * @lock, otherwise pm_runtime_get_sync() would deadlock against the
	 * mutex this function already holds.
	 */
	if (enable)
	{
		ret = pm_runtime_resume_and_get(sensor->dev);
		if (ret < 0)
		{
			dev_err(sensor->dev, "failed to power on: %d\n", ret);
			return ret;
		}
	}

	mutex_lock(&sensor->lock);

	if (sensor->streaming == !!enable)
	{
		mutex_unlock(&sensor->lock);
		if (enable)
			pm_runtime_put(sensor->dev);
		return 0;
	}

	if (enable)
	{
		ret = sensor->soc->ops->set_mode(sensor, &sensor->fmt);
		if (ret)
		{
			dev_err(sensor->dev, "failed to set mode: %d\n", ret);
			pm_runtime_put(sensor->dev);
			goto out_unlock;
		}

		/* Start streaming — take sensor out of standby */
		ret = sensor->soc->ops->power_on(sensor);
		if (ret)
		{
			dev_err(sensor->dev, "failed to start streaming: %d\n", ret);
			pm_runtime_put(sensor->dev);
			goto out_unlock;
		}

		sensor->streaming = true;
		trace_imx708_stream(sensor->client->addr, true);
		dev_dbg(sensor->dev, "streaming started\n");
	}
	else
	{
		ret = sensor->soc->ops->power_off(sensor);
		if (ret)
			dev_warn(sensor->dev, "power_off failed: %d\n", ret);

		/*
		 * The stream is torn down regardless: report success so the
		 * media pipeline can still be stopped cleanly.
		 */
		ret = 0;
		sensor->streaming = false;
		pm_runtime_put(sensor->dev);
		trace_imx708_stream(sensor->client->addr, false);
		dev_dbg(sensor->dev, "streaming stopped\n");
	}

out_unlock:
	mutex_unlock(&sensor->lock);
	return ret;
}

static int imx708_g_frame_interval(struct v4l2_subdev *sd,
								   struct v4l2_subdev_state *state,
								   struct v4l2_subdev_frame_interval *fi)
{
	struct imx708_dev *sensor = to_imx708_dev(sd);

	mutex_lock(&sensor->lock);
	fi->interval.numerator = 1;
	fi->interval.denominator = sensor->mode ? sensor->mode->fps : 30;
	mutex_unlock(&sensor->lock);

	return 0;
}

static int imx708_enum_mbus_code(struct v4l2_subdev *sd,
								 struct v4l2_subdev_state *state,
								 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx708_dev *sensor = to_imx708_dev(sd);

	if (code->index >= sensor->soc->num_modes)
		return -EINVAL;

	code->code = sensor->soc->modes[code->index].code;
	return 0;
}

static int imx708_enum_frame_size(struct v4l2_subdev *sd,
								  struct v4l2_subdev_state *state,
								  struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx708_dev *sensor = to_imx708_dev(sd);
	int i, count = 0;

	for (i = 0; i < sensor->soc->num_modes; i++)
	{
		if (sensor->soc->modes[i].code != fse->code)
			continue;

		if (count == fse->index)
		{
			fse->min_width = sensor->soc->modes[i].width;
			fse->max_width = sensor->soc->modes[i].width;
			fse->min_height = sensor->soc->modes[i].height;
			fse->max_height = sensor->soc->modes[i].height;
			return 0;
		}
		count++;
	}

	return -EINVAL;
}

static int imx708_get_fmt(struct v4l2_subdev *sd,
						  struct v4l2_subdev_state *state,
						  struct v4l2_subdev_format *format)
{
	struct imx708_dev *sensor = to_imx708_dev(sd);

	mutex_lock(&sensor->lock);
	format->format = sensor->fmt;
	mutex_unlock(&sensor->lock);

	return 0;
}

static int imx708_set_fmt(struct v4l2_subdev *sd,
						  struct v4l2_subdev_state *state,
						  struct v4l2_subdev_format *format)
{
	struct imx708_dev *sensor = to_imx708_dev(sd);
	const struct imx708_mode *best_mode = NULL;
	int i;

	mutex_lock(&sensor->lock);

	/* Find the best matching mode */
	for (i = 0; i < sensor->soc->num_modes; i++)
	{
		const struct imx708_mode *m = &sensor->soc->modes[i];

		if (format->format.code != m->code)
			continue;

		if (format->format.width == m->width &&
			format->format.height == m->height)
		{
			best_mode = m;
			break;
		}

		/* Accept closest match if exact not found */
		if (!best_mode ||
			(m->width <= format->format.width &&
			 m->height <= format->format.height &&
			 m->width > best_mode->width))
			best_mode = m;
	}

	if (!best_mode)
		best_mode = &sensor->soc->modes[0];

	/* If streaming, only allow try — can't change while active */
	if (sensor->streaming)
	{
		format->format.width = best_mode->width;
		format->format.height = best_mode->height;
		format->format.code = best_mode->code;
		format->format.field = V4L2_FIELD_NONE;
		format->format.colorspace = V4L2_COLORSPACE_SRGB;
		mutex_unlock(&sensor->lock);
		return 0;
	}

	/* Apply the format */
	sensor->fmt.width = best_mode->width;
	sensor->fmt.height = best_mode->height;
	sensor->fmt.code = best_mode->code;
	sensor->fmt.field = V4L2_FIELD_NONE;
	sensor->fmt.colorspace = V4L2_COLORSPACE_SRGB;
	sensor->mode = best_mode;

	format->format = sensor->fmt;

	mutex_unlock(&sensor->lock);
	return 0;
}

static int imx708_init_cfg(struct v4l2_subdev *sd,
						   struct v4l2_subdev_state *state)
{
	struct imx708_dev *sensor = to_imx708_dev(sd);
	struct v4l2_mbus_framefmt *fmt;

	fmt = v4l2_subdev_state_get_format(state, 0);
	if (!fmt)
		return -EINVAL;

	fmt->width = sensor->soc->modes[0].width;
	fmt->height = sensor->soc->modes[0].height;
	fmt->code = sensor->soc->modes[0].code;
	fmt->field = V4L2_FIELD_NONE;
	fmt->colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

static const struct v4l2_subdev_video_ops imx708_video_ops = {
	.s_stream = imx708_s_stream,
};

static const struct v4l2_subdev_pad_ops imx708_pad_ops = {
	.enum_mbus_code = imx708_enum_mbus_code,
	.enum_frame_size = imx708_enum_frame_size,
	.get_fmt = imx708_get_fmt,
	.set_fmt = imx708_set_fmt,
	.get_frame_interval = imx708_g_frame_interval,
};

static const struct v4l2_subdev_ops imx708_subdev_ops = {
	.video = &imx708_video_ops,
	.pad = &imx708_pad_ops,
};

static const struct v4l2_subdev_internal_ops imx708_internal_ops = {
	.init_state = imx708_init_cfg,
};

/* ------------------------------------------------------------------ */
/* V4L2 control handlers                                               */
/* ------------------------------------------------------------------ */

static int imx708_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx708_dev *sensor =
		container_of(ctrl->handler, struct imx708_dev, ctrl_handler);
	int ret = 0;

	mutex_lock(&sensor->lock);

	switch (ctrl->id)
	{
	case V4L2_CID_ANALOGUE_GAIN:
		ret = sensor->soc->ops->set_gain(sensor, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret = sensor->soc->ops->set_exposure(sensor, ctrl->val);
		break;
	case V4L2_CID_DIGITAL_GAIN:
		ret = sensor->soc->ops->set_digital_gain(sensor, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		/* Test pattern is applied on next stream start */
		sensor->test_pattern = ctrl->val;
		break;
	case V4L2_CID_HFLIP:
		/* TODO(HW): Implement horizontal flip register write */
		sensor->hflip = ctrl->val;
		break;
	case V4L2_CID_VFLIP:
		/* TODO(HW): Implement vertical flip register write */
		sensor->vflip = ctrl->val;
		break;
	case V4L2_CID_BRIGHTNESS:
		/* Digital offset / black level */
		sensor->brightness = ctrl->val;
		break;
	case V4L2_CID_CONTRAST:
		sensor->contrast = ctrl->val;
		break;
	case V4L2_CID_SATURATION:
		sensor->saturation = ctrl->val;
		break;
	case V4L2_CID_HUE:
		sensor->hue = ctrl->val;
		break;
	case V4L2_CID_GAMMA:
		sensor->gamma = ctrl->val;
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		sensor->auto_wb = ctrl->val;
		break;
	case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
		if (!sensor->auto_wb)
			sensor->wb_temp = ctrl->val;
		break;
	case V4L2_CID_SHARPNESS:
		sensor->sharpness = ctrl->val;
		break;
	case V4L2_CID_BACKLIGHT_COMPENSATION:
		sensor->backlight_comp = ctrl->val;
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		sensor->power_line_freq = ctrl->val;
		break;
	case V4L2_CID_AUTO_EXPOSURE_BIAS:
		sensor->exp_bias = ctrl->val;
		break;
	case V4L2_CID_3A_LOCK:
		sensor->a3a_lock = ctrl->val;
		break;
	case V4L2_CID_SCENE_MODE:
		sensor->scene_mode = ctrl->val;
		break;
	case V4L2_CID_ISO_SENSITIVITY:
		sensor->iso = ctrl->val;
		break;
	case V4L2_CID_ISO_SENSITIVITY_AUTO:
		sensor->iso_auto = ctrl->val;
		break;
	case V4L2_CID_COLORFX:
		sensor->colorfx = ctrl->val;
		break;
	case V4L2_CID_ZOOM_ABSOLUTE:
		sensor->zoom = ctrl->val;
		break;
	case V4L2_CID_PAN_ABSOLUTE:
		sensor->pan = ctrl->val;
		break;
	case V4L2_CID_TILT_ABSOLUTE:
		sensor->tilt = ctrl->val;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&sensor->lock);
	return ret;
}

static const struct v4l2_ctrl_ops imx708_ctrl_ops = {
	.s_ctrl = imx708_set_ctrl,
};

static const char *const imx708_test_pattern_menu[] = {
	"Disabled",
	"Color Bars",
	"Solid Color",
	"Checkerboard",
	"Walking 1s",
	"Vertical Color Bars",
	"Horizontal Color Bars",
	"Alternate Pattern",
};

static const char *const imx708_power_line_freq_menu[] = {
	"Disabled",
	"50 Hz",
	"60 Hz",
	"Auto",
};

static const char *const imx708_scene_mode_menu[] = {
	"Auto",
	"Night",
	"Sport",
	"Landscape",
	"Portrait",
	"Macro",
	"Fireworks",
	"Sunsets",
	"Candlelight",
};

static const char *const imx708_colorfx_menu[] = {
	"None",
	"B&W",
	"Sepia",
	"Negative",
	"Emboss",
	"Sketch",
	"Sky Blue",
	"Grass Green",
	"Skin Whiten",
	"Vivid",
};

/*
 * V4L2_CID_AUTO_EXPOSURE_BIAS and V4L2_CID_ISO_SENSITIVITY are
 * V4L2_CTRL_TYPE_INTEGER_MENU controls. They must be created with
 * v4l2_ctrl_new_int_menu(); v4l2_ctrl_new_std() rejects them and latches
 * -EINVAL into hdl->error, which aborts probe.
 *
 * Exposure bias is expressed in 1/1000 EV and ISO as the standard ISO
 * value multiplied by 1000, as required by the V4L2 control documentation.
 */
static const s64 imx708_exposure_bias_menu[] = {
	-3000,
	-2750,
	-2500,
	-2250,
	-2000,
	-1750,
	-1500,
	-1250,
	-1000,
	-750,
	-500,
	-250,
	0,
	250,
	500,
	750,
	1000,
	1250,
	1500,
	1750,
	2000,
	2250,
	2500,
	2750,
	3000,
};
#define IMX708_EXPOSURE_BIAS_DEF_IDX 12 /* 0 EV */

static const s64 imx708_iso_menu[] = {
	100000,
	200000,
	400000,
	800000,
	1600000,
	3200000,
};

static int imx708_init_controls(struct imx708_dev *sensor)
{
	struct v4l2_ctrl_handler *hdl = &sensor->ctrl_handler;
	int ret;

	ret = v4l2_ctrl_handler_init(hdl, 32);
	if (ret)
		return ret;

	/* === Exposure / Gain === */

	/* Analog gain, in sensor-specific units (dB * 1000) */
	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_ANALOGUE_GAIN,
					  IMX708_GAIN_MIN, IMX708_GAIN_MAX,
					  IMX708_GAIN_STEP, IMX708_GAIN_DEFAULT);

	/* Digital gain */
	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_DIGITAL_GAIN,
					  IMX708_DGTL_GAIN_MIN, IMX708_DGTL_GAIN_MAX,
					  IMX708_DGTL_GAIN_STEP, IMX708_DGTL_GAIN_DEFAULT);

	/* Exposure in line units */
	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_EXPOSURE,
					  IMX708_EXPOSURE_MIN, IMX708_EXPOSURE_MAX,
					  1, IMX708_EXPOSURE_DEFAULT);

	/* Auto exposure bias (+/- 3 EV in 0.25 steps) */
	v4l2_ctrl_new_int_menu(hdl, &imx708_ctrl_ops,
						   V4L2_CID_AUTO_EXPOSURE_BIAS,
						   ARRAY_SIZE(imx708_exposure_bias_menu) - 1,
						   IMX708_EXPOSURE_BIAS_DEF_IDX,
						   imx708_exposure_bias_menu);

	/* ISO sensitivity */
	v4l2_ctrl_new_int_menu(hdl, &imx708_ctrl_ops,
						   V4L2_CID_ISO_SENSITIVITY,
						   ARRAY_SIZE(imx708_iso_menu) - 1, 0,
						   imx708_iso_menu);

	v4l2_ctrl_new_std_menu(hdl, &imx708_ctrl_ops,
						   V4L2_CID_ISO_SENSITIVITY_AUTO,
						   1, 0, V4L2_ISO_SENSITIVITY_AUTO);

	/* 3A lock (auto exposure / auto white balance lock) */
	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_3A_LOCK,
					  0, (V4L2_LOCK_EXPOSURE | V4L2_LOCK_WHITE_BALANCE | V4L2_LOCK_FOCUS),
					  0, 0);

	/* === Image Processing === */

	/* Brightness (black level offset) */
	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_BRIGHTNESS,
					  -255, 255, 1, 0);

	/* Contrast */
	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_CONTRAST,
					  0, 255, 1, 128);

	/* Saturation */
	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_SATURATION,
					  0, 255, 1, 128);

	/* Hue */
	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_HUE,
					  -180, 180, 1, 0);

	/* Gamma */
	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_GAMMA,
					  0, 255, 1, 128);

	/* Sharpness */
	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_SHARPNESS,
					  0, 15, 1, 0);

	/* === White Balance === */

	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_AUTO_WHITE_BALANCE,
					  0, 1, 1, 1);

	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_WHITE_BALANCE_TEMPERATURE,
					  2000, 8000, 100, 5000);

	/* === Test Pattern === */

	v4l2_ctrl_new_std_menu_items(hdl, &imx708_ctrl_ops,
								 V4L2_CID_TEST_PATTERN,
								 ARRAY_SIZE(imx708_test_pattern_menu) - 1,
								 0, 0, imx708_test_pattern_menu);

	/* === Flip / Mirror === */

	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_HFLIP, 0, 1, 1, 0);

	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_VFLIP, 0, 1, 1, 0);

	/* === Power Line Frequency (anti-flicker) === */

	v4l2_ctrl_new_std_menu_items(hdl, &imx708_ctrl_ops,
								 V4L2_CID_POWER_LINE_FREQUENCY,
								 ARRAY_SIZE(imx708_power_line_freq_menu) - 1,
								 0, 0, imx708_power_line_freq_menu);

	/* === Backlight Compensation === */

	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_BACKLIGHT_COMPENSATION,
					  0, 2, 1, 0);

	/* === Scene Mode === */

	v4l2_ctrl_new_std_menu_items(hdl, &imx708_ctrl_ops,
								 V4L2_CID_SCENE_MODE,
								 ARRAY_SIZE(imx708_scene_mode_menu) - 1,
								 0, 0, imx708_scene_mode_menu);

	/* === Color Effects === */

	v4l2_ctrl_new_std_menu_items(hdl, &imx708_ctrl_ops,
								 V4L2_CID_COLORFX,
								 ARRAY_SIZE(imx708_colorfx_menu) - 1,
								 0, 0, imx708_colorfx_menu);

	/* === Zoom / Pan / Tilt (digital) === */

	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_ZOOM_ABSOLUTE,
					  0, 100, 1, 0);

	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_PAN_ABSOLUTE,
					  -100, 100, 1, 0);

	v4l2_ctrl_new_std(hdl, &imx708_ctrl_ops,
					  V4L2_CID_TILT_ABSOLUTE,
					  -100, 100, 1, 0);

	if (hdl->error)
	{
		ret = hdl->error;
		v4l2_ctrl_handler_free(hdl);
		return ret;
	}

	sensor->sd.ctrl_handler = hdl;
	return 0;
}

/* ------------------------------------------------------------------ */
/* Power management                                                    */
/* ------------------------------------------------------------------ */

static int imx708_hw_power_up_locked(struct device *dev)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "power on\n");

	/* Enable regulators (IMX708: vana1=2.8V, vana2=1.8V, vdig=1.1V, vddl=1.8V) */
	ret = regulator_enable(sensor->reg_dovdd); /* vana1 */
	if (ret)
		return ret;

	ret = regulator_enable(sensor->reg_avdd); /* vana2 */
	if (ret)
		goto err_dovdd;

	ret = regulator_enable(sensor->reg_dvdd); /* vdig */
	if (ret)
		goto err_avdd;

	if (sensor->reg_vddl)
	{ /* vddl (optional) */
		ret = regulator_enable(sensor->reg_vddl);
		if (ret)
			goto err_dvdd;
	}

	/* Wait for power supply to stabilize */
	usleep_range(3000, 5000);

	/* Toggle reset GPIO */
	if (sensor->gpio_reset)
	{
		gpiod_set_value_cansleep(sensor->gpio_reset, 1);
		usleep_range(1000, 2000);
		gpiod_set_value_cansleep(sensor->gpio_reset, 0);
		usleep_range(10000, 15000);
	}

	/* Enable power GPIO */
	if (sensor->gpio_power)
		gpiod_set_value_cansleep(sensor->gpio_power, 1);

	/* Wait for sensor to initialize */
	usleep_range(10000, 15000);

	ret = sensor->soc->ops->init(sensor);
	if (ret)
		goto err_reset;

	trace_imx708_power(sensor->client->addr, true);
	return 0;

err_reset:
	if (sensor->gpio_power)
		gpiod_set_value_cansleep(sensor->gpio_power, 0);
	if (sensor->gpio_reset)
		gpiod_set_value_cansleep(sensor->gpio_reset, 1);
	if (sensor->reg_vddl)
		regulator_disable(sensor->reg_vddl);
err_dvdd:
	regulator_disable(sensor->reg_dvdd);
err_avdd:
	regulator_disable(sensor->reg_avdd);
err_dovdd:
	regulator_disable(sensor->reg_dovdd);
	return ret;
}

static int imx708_hw_power_down_locked(struct device *dev)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);

	dev_dbg(dev, "power off\n");

	sensor->soc->ops->deinit(sensor);

	if (sensor->gpio_power)
		gpiod_set_value_cansleep(sensor->gpio_power, 0);
	if (sensor->gpio_reset)
		gpiod_set_value_cansleep(sensor->gpio_reset, 1);

	if (sensor->reg_vddl)
		regulator_disable(sensor->reg_vddl);
	regulator_disable(sensor->reg_dvdd);
	regulator_disable(sensor->reg_avdd);
	regulator_disable(sensor->reg_dovdd);

	trace_imx708_power(sensor->client->addr, false);
	return 0;
}

/*
 * Public wrappers used by the runtime-PM callbacks in imx708_pm.c. They
 * are separate from the *_locked() helpers only so that probe can drive
 * the same sequence before runtime PM is enabled.
 */
int imx708_hw_power_up(struct device *dev)
{
	return imx708_hw_power_up_locked(dev);
}

int imx708_hw_power_down(struct device *dev)
{
	return imx708_hw_power_down_locked(dev);
}

static const struct dev_pm_ops imx708_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(imx708_suspend, imx708_resume)
		SET_RUNTIME_PM_OPS(imx708_runtime_suspend, imx708_runtime_resume,
						   NULL)};

/* ------------------------------------------------------------------ */
/* I2C driver probe / remove                                           */
/* ------------------------------------------------------------------ */

static int imx708_check_chip_id(struct imx708_dev *sensor)
{
	u32 chip_id;
	int ret;

	/*
	 * The chip ID lives in two consecutive 8-bit registers (0x0016 and
	 * 0x0017). A single regmap_read() only returns the high byte.
	 */
	ret = imx708_read_reg16(sensor, IMX708_REG_CHIP_ID, &chip_id);
	if (ret)
	{
		dev_err(sensor->dev, "failed to read chip ID: %d\n", ret);
		return ret;
	}

	dev_dbg(sensor->dev, "chip ID: 0x%04x\n", chip_id);

	/* IMX708 chip ID is 0x0708 */
	if (chip_id != IMX708_CHIP_ID)
	{
		dev_err(sensor->dev,
				"unexpected chip ID 0x%04x (expected 0x%04x)\n",
				chip_id, IMX708_CHIP_ID);
		return -ENODEV;
	}

	return 0;
}

static int imx708_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct imx708_dev *sensor;
	int ret;

	/*
	 * Not devm: an open file descriptor on /dev/imx708* keeps a reference
	 * and can outlive driver unbind. imx708_dev_release() frees it.
	 */
	sensor = kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	kref_init(&sensor->refcount);
	sensor->dev = dev;
	sensor->client = client;
	mutex_init(&sensor->lock);
	dev_set_drvdata(dev, sensor);

	/* Get platform data (SoC match data) */
	sensor->soc = device_get_match_data(dev);
	if (!sensor->soc)
	{
		dev_err(dev, "no platform match data found\n");
		ret = -ENODEV;
		goto err_put;
	}

	dev_info(dev, "probing %s sensor\n", sensor->soc->name);

	/* Initialize regmap */
	sensor->regmap = devm_regmap_init_i2c(client, sensor->soc->regmap_cfg);
	if (IS_ERR(sensor->regmap))
	{
		ret = PTR_ERR(sensor->regmap);
		dev_err(dev, "failed to init regmap: %d\n", ret);
		goto err_put;
	}

	/* Get regulators (IMX708 has 4 supplies: vana1, vana2, vdig, vddl) */
	sensor->reg_dovdd = devm_regulator_get(dev, "vana1");
	if (IS_ERR(sensor->reg_dovdd))
	{
		ret = dev_err_probe(dev, PTR_ERR(sensor->reg_dovdd),
							"failed to get vana1 (2.8V analog) regulator\n");
		goto err_put;
	}

	sensor->reg_avdd = devm_regulator_get(dev, "vana2");
	if (IS_ERR(sensor->reg_avdd))
	{
		ret = dev_err_probe(dev, PTR_ERR(sensor->reg_avdd),
							"failed to get vana2 (1.8V analog) regulator\n");
		goto err_put;
	}

	sensor->reg_dvdd = devm_regulator_get(dev, "vdig");
	if (IS_ERR(sensor->reg_dvdd))
	{
		ret = dev_err_probe(dev, PTR_ERR(sensor->reg_dvdd),
							"failed to get vdig (1.1V digital) regulator\n");
		goto err_put;
	}

	/* vddl (1.8V I/O) is optional - often tied to vana2 */
	sensor->reg_vddl = devm_regulator_get_optional(dev, "vddl");
	if (IS_ERR(sensor->reg_vddl))
	{
		if (PTR_ERR(sensor->reg_vddl) == -EPROBE_DEFER)
		{
			ret = dev_err_probe(dev, -EPROBE_DEFER,
								"vddl regulator not ready\n");
			goto err_put;
		}
		sensor->reg_vddl = NULL;
	}

	/* Get GPIOs */
	sensor->gpio_reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(sensor->gpio_reset))
	{
		ret = dev_err_probe(dev, PTR_ERR(sensor->gpio_reset),
							"failed to get reset GPIO\n");
		goto err_put;
	}

	sensor->gpio_power = devm_gpiod_get_optional(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(sensor->gpio_power))
	{
		ret = dev_err_probe(dev, PTR_ERR(sensor->gpio_power),
							"failed to get power GPIO\n");
		goto err_put;
	}

	/* Power on and check chip ID */
	ret = imx708_hw_power_up(dev);
	if (ret)
	{
		dev_err(dev, "failed to power on sensor: %d\n", ret);
		goto err_put;
	}

	ret = imx708_check_chip_id(sensor);
	if (ret)
		goto err_power_off;

	/* Allocate the per-instance index used for /dev, sysfs and debugfs */
	ret = ida_alloc_max(&imx708_ida, IMX708_MAX_DEVICES - 1, GFP_KERNEL);
	if (ret < 0)
	{
		dev_err(dev, "no free device instance (max %d): %d\n",
				IMX708_MAX_DEVICES, ret);
		goto err_power_off;
	}
	sensor->chardev_id = (unsigned int)ret;

	/* Initialize V4L2 sub-device */
	v4l2_subdev_init(&sensor->sd, &imx708_subdev_ops);
	sensor->sd.internal_ops = &imx708_internal_ops;
	sensor->sd.owner = THIS_MODULE;
	sensor->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE |
					   V4L2_SUBDEV_FL_HAS_EVENTS;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	sensor->sd.dev = dev;

	snprintf(sensor->sd.name, sizeof(sensor->sd.name), "%s %s",
			 DRV_NAME, dev_name(dev));

	/* Initialize controls */
	ret = imx708_init_controls(sensor);
	if (ret)
	{
		dev_err(dev, "failed to init controls: %d\n", ret);
		goto err_free_ida;
	}

	/* Create media pad */
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);
	if (ret)
	{
		dev_err(dev, "failed to init media pads: %d\n", ret);
		goto err_free_ctrl;
	}

	/* Set default format */
	sensor->fmt.width = sensor->soc->modes[0].width;
	sensor->fmt.height = sensor->soc->modes[0].height;
	sensor->fmt.code = sensor->soc->modes[0].code;
	sensor->fmt.field = V4L2_FIELD_NONE;
	sensor->fmt.colorspace = V4L2_COLORSPACE_SRGB;
	sensor->mode = &sensor->soc->modes[0];

	/*
	 * Allocate the IRQ counters and, when the device tree supplies an
	 * interrupt, hook up the handler. Boards without an interrupt line
	 * (the stock Camera Module 3) still get the counters so that debugfs
	 * and the ioctl status path stay valid.
	 */
	ret = imx708_irq_init(sensor);
	if (ret)
	{
		dev_err(dev, "failed to init interrupts: %d\n", ret);
		goto err_media_cleanup;
	}

	/* Register sub-device */
	ret = v4l2_async_register_subdev_sensor(&sensor->sd);
	if (ret)
	{
		dev_err(dev, "failed to register subdev: %d\n", ret);
		goto err_media_cleanup;
	}

	/*
	 * Expose the ioctl ABI on /dev/imx708<id>. Without this the
	 * libimx708 userspace library, the gRPC server and the GUI have no
	 * way to reach the sensor at all.
	 */
	ret = imx708_chardev_register(sensor, sensor->chardev_id);
	if (ret)
	{
		dev_err(dev, "failed to register char device: %d\n", ret);
		goto err_unregister_subdev;
	}

	/* Per-instance debugfs directory (best effort, never fatal) */
	imx708_debugfs_register(sensor, sensor->chardev_id);

	/*
	 * Enable runtime PM last: the hardware is currently powered up, so
	 * imx708_pm_init() marks the device active and lets autosuspend
	 * drop the supplies once probe settles.
	 */
	ret = imx708_pm_init(sensor);
	if (ret)
	{
		dev_err(dev, "failed to enable runtime PM: %d\n", ret);
		goto err_unregister_chardev;
	}

	trace_imx708_probe(sensor->soc->name, client->addr);
	dev_info(dev, "%s probed as /dev/%s%u\n", sensor->soc->name,
			 DRV_NAME, sensor->chardev_id);

	return 0;

err_unregister_chardev:
	imx708_debugfs_unregister(sensor);
	imx708_chardev_unregister(sensor);
err_unregister_subdev:
	v4l2_async_unregister_subdev(&sensor->sd);
err_media_cleanup:
	media_entity_cleanup(&sensor->sd.entity);
err_free_ctrl:
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);
err_free_ida:
	ida_free(&imx708_ida, sensor->chardev_id);
err_power_off:
	imx708_hw_power_down(dev);
err_put:
	imx708_dev_put(sensor);
	return ret;
}

static void imx708_remove(struct i2c_client *client)
{
	struct imx708_dev *sensor = i2c_get_clientdata(client);

	trace_imx708_remove(sensor->soc->name, client->addr);

	imx708_debugfs_unregister(sensor);
	imx708_chardev_unregister(sensor);

	v4l2_async_unregister_subdev(&sensor->sd);
	media_entity_cleanup(&sensor->sd.entity);
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);

	/*
	 * Runtime PM is disabled by the devm action installed in
	 * imx708_pm_init(), which also powers the sensor down.
	 */
	ida_free(&imx708_ida, sensor->chardev_id);

	/*
	 * Drop the driver's reference. The structure is only freed once any
	 * still-open /dev/imx708* file descriptors are closed as well.
	 */
	dev_info(sensor->dev, "sensor removed\n");
	imx708_dev_put(sensor);
}

/* ------------------------------------------------------------------ */
/* I2C driver structure                                                */
/* ------------------------------------------------------------------ */

static const struct of_device_id imx708_of_match[] = {
	{
		.compatible = "sony,imx708",
		.data = &imx708_soc_rpi,
	},
	{
		.compatible = "sony,imx708-wide",
		.data = &imx708_soc_rpi_wide,
	},
	{/* sentinel */}};
MODULE_DEVICE_TABLE(of, imx708_of_match);

static const struct i2c_device_id imx708_id[] = {
	{"imx708", (kernel_ulong_t)&imx708_soc_rpi},
	{"imx708-wide", (kernel_ulong_t)&imx708_soc_rpi_wide},
	{}};
MODULE_DEVICE_TABLE(i2c, imx708_id);

static struct i2c_driver imx708_i2c_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = imx708_of_match,
		.pm = &imx708_pm_ops,
		/*
		 * Attaches the attributes from imx708_sysfs.c. Without this
		 * the documented sysfs ABI is never created.
		 */
		.dev_groups = imx708_attr_groups,
	},
	.probe = imx708_probe,
	.remove = imx708_remove,
	.id_table = imx708_id,
};

/* ------------------------------------------------------------------ */
/* Module init / exit                                                  */
/* ------------------------------------------------------------------ */

static int __init imx708_module_init(void)
{
	int ret;

	/* Register char device region for /dev/imx708* */
	ret = alloc_chrdev_region(&imx708_devt, 0, IMX708_MAX_DEVICES,
							  DRV_NAME);
	if (ret)
	{
		pr_err("imx708: failed to alloc chrdev region: %d\n", ret);
		return ret;
	}

	/* Create device class */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	imx708_class = class_create(DRV_NAME);
#else
	imx708_class = class_create(THIS_MODULE, DRV_NAME);
#endif
	if (IS_ERR(imx708_class))
	{
		ret = PTR_ERR(imx708_class);
		pr_err("imx708: failed to create class: %d\n", ret);
		unregister_chrdev_region(imx708_devt, IMX708_MAX_DEVICES);
		return ret;
	}

	/* debugfs is optional; a failure here must not stop the driver. */
	ret = imx708_debugfs_init();
	if (ret)
		pr_warn("imx708: debugfs unavailable: %d\n", ret);

	ret = i2c_add_driver(&imx708_i2c_driver);
	if (ret)
	{
		pr_err("imx708: failed to add i2c driver: %d\n", ret);
		imx708_debugfs_exit();
		class_destroy(imx708_class);
		unregister_chrdev_region(imx708_devt, IMX708_MAX_DEVICES);
		return ret;
	}

	pr_info("imx708: driver loaded (version %s)\n", DRV_VERSION);
	return 0;
}

static void __exit imx708_module_exit(void)
{
	i2c_del_driver(&imx708_i2c_driver);
	imx708_debugfs_exit();
	class_destroy(imx708_class);
	unregister_chrdev_region(imx708_devt, IMX708_MAX_DEVICES);
	ida_destroy(&imx708_ida);
	pr_info("imx708: driver unloaded\n");
}

module_init(imx708_module_init);
module_exit(imx708_module_exit);

MODULE_AUTHOR("Sandesh <sandesh@soccentric.com>");
MODULE_DESCRIPTION("Sony IMX708 12MP MIPI CSI-2 camera sensor driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS("i2c:" DRV_NAME);
