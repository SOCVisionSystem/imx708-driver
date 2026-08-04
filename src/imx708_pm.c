// SPDX-License-Identifier: GPL-2.0-only
/*
 * imx708_pm.c - Power management for IMX708 sensor driver
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * The IMX708 has four supplies (vana1 2.8V, vana2 1.8V, vdig 1.1V and the
 * optional vddl 1.8V) plus a reset GPIO. Cutting those is the only state
 * that actually saves power, so the regulator/GPIO sequence is what the
 * runtime-PM callbacks drive; imx708_hw_power_up()/imx708_hw_power_down()
 * in imx708_main.c implement it.
 *
 * The sensor loses all register state when the supplies drop, so there is
 * no useful register context to save. Instead, resume re-runs the sensor
 * init sequence, re-applies the current mode and replays the V4L2 control
 * values through the control handler. This is the standard camera-sensor
 * pattern and cannot drift out of sync with the register map the way a
 * hand-maintained save list does.
 *
 * Locking:
 *   The runtime-PM callbacks must NOT take sensor->lock. Callers such as
 *   imx708_s_stream() and the ioctl paths invoke pm_runtime_resume_and_get()
 *   while holding it and would deadlock against themselves. The PM core
 *   already serialises the callbacks for a given device, and
 *   v4l2_ctrl_handler_setup() takes sensor->lock itself via
 *   imx708_set_ctrl().
 */

#include <linux/pm_runtime.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-ctrls.h>

#include "imx708_platform.h"
#include "imx708_regs.h"

/* Idle this long before dropping the supplies. */
#define IMX708_AUTOSUSPEND_DELAY_MS	1000

/* ------------------------------------------------------------------ */
/* Runtime PM handlers                                                 */
/* ------------------------------------------------------------------ */

int imx708_runtime_suspend(struct device *dev)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);

	dev_dbg(dev, "runtime suspend\n");

	if (sensor->streaming)
		sensor->soc->ops->power_off(sensor);

	imx708_hw_power_down(dev);

	return 0;
}

int imx708_runtime_resume(struct device *dev)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "runtime resume\n");

	/* Bring up supplies, release reset and run the init sequence. */
	ret = imx708_hw_power_up(dev);
	if (ret)
		return ret;

	/* Re-apply the mode; the sensor came up with register defaults. */
	if (sensor->mode) {
		ret = sensor->soc->ops->set_mode(sensor, &sensor->fmt);
		if (ret) {
			dev_err(dev, "failed to restore mode: %d\n", ret);
			goto err_power_down;
		}
	}

	/*
	 * Replay every control value. This restores gain, exposure, digital
	 * gain and the test pattern without a separate register save list.
	 */
	ret = v4l2_ctrl_handler_setup(&sensor->ctrl_handler);
	if (ret) {
		dev_err(dev, "failed to restore controls: %d\n", ret);
		goto err_power_down;
	}

	/* Restore HDR configuration if it was enabled. */
	if (sensor->hdr_enabled && sensor->soc->ops->set_hdr) {
		ret = sensor->soc->ops->set_hdr(sensor, 1, sensor->hdr_ratio);
		if (ret)
			dev_warn(dev, "failed to restore HDR mode: %d\n", ret);
	}

	return 0;

err_power_down:
	imx708_hw_power_down(dev);
	return ret;
}

/* ------------------------------------------------------------------ */
/* System sleep handlers                                                */
/* ------------------------------------------------------------------ */

int imx708_suspend(struct device *dev)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);

	/*
	 * Stop the stream first so the sensor is quiesced before the supplies
	 * go away. sensor->streaming stays set so imx708_resume() knows to
	 * restart it.
	 */
	mutex_lock(&sensor->lock);
	if (sensor->streaming)
		sensor->soc->ops->power_off(sensor);
	mutex_unlock(&sensor->lock);

	return pm_runtime_force_suspend(dev);
}

int imx708_resume(struct device *dev)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_force_resume(dev);
	if (ret)
		return ret;

	mutex_lock(&sensor->lock);
	if (sensor->streaming) {
		ret = sensor->soc->ops->power_on(sensor);
		if (ret) {
			dev_err(dev, "failed to restart streaming: %d\n", ret);
			sensor->streaming = false;
		}
	}
	mutex_unlock(&sensor->lock);

	return ret;
}

/* ------------------------------------------------------------------ */
/* Runtime PM setup                                                    */
/* ------------------------------------------------------------------ */

static void imx708_pm_disable(void *data)
{
	struct device *dev = data;

	pm_runtime_dont_use_autosuspend(dev);
	pm_runtime_disable(dev);
	pm_runtime_set_suspended(dev);
}

/**
 * imx708_pm_init - Enable runtime PM for the sensor
 * @sensor: Driver state
 *
 * Called from probe with the hardware already powered up (probe reads the
 * chip ID over I2C), so the device starts in the RPM_ACTIVE state and the
 * reference taken here is released via autosuspend. Runtime PM is torn
 * down by the devm action registered here, which runs before the managed
 * regulators and GPIOs are released.
 */
int imx708_pm_init(struct imx708_dev *sensor)
{
	struct device *dev = sensor->dev;
	int ret;

	pm_runtime_set_active(dev);
	pm_runtime_get_noresume(dev);
	pm_runtime_enable(dev);

	ret = devm_add_action_or_reset(dev, imx708_pm_disable, dev);
	if (ret) {
		pm_runtime_put_noidle(dev);
		return ret;
	}

	pm_runtime_set_autosuspend_delay(dev, IMX708_AUTOSUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;
}
