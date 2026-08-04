// SPDX-License-Identifier: GPL-2.0-only
/*
 * imx708_sysfs.c - Sysfs attributes for IMX708 sensor driver
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * This file implements the sysfs attribute interface for the IMX708 sensor.
 * One value per file, human-readable, documented units. All attributes are
 * attached via .dev_groups on the driver so creation and removal are race-free.
 *
 * Locking:
 *   All show/store handlers take sensor->lock around sensor state and
 *   register access.
 *
 * Power:
 *   The sensor is runtime-suspended (supplies off) whenever nothing is
 *   streaming, and I2C transfers to an unpowered sensor fail. Every handler
 *   that touches hardware therefore takes a runtime-PM reference first via
 *   the imx708_sysfs_read8() and imx708_sysfs_read16() helpers below.
 *   Attributes that only report driver state do not.
 */

#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/kernel.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>

#include "imx708_platform.h"
#include "imx708_regs.h"

/* ------------------------------------------------------------------ */
/* Power-managed register access helpers                               */
/* ------------------------------------------------------------------ */

static int imx708_sysfs_read8(struct imx708_dev *sensor, u32 reg, u32 *val)
{
	int ret;

	ret = pm_runtime_resume_and_get(sensor->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&sensor->lock);
	ret = regmap_read(sensor->regmap, reg, val);
	mutex_unlock(&sensor->lock);

	pm_runtime_mark_last_busy(sensor->dev);
	pm_runtime_put_autosuspend(sensor->dev);

	return ret;
}

static int imx708_sysfs_read16(struct imx708_dev *sensor, u32 reg, u32 *val)
{
	int ret;

	ret = pm_runtime_resume_and_get(sensor->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&sensor->lock);
	ret = imx708_read_reg16(sensor, reg, val);
	mutex_unlock(&sensor->lock);

	pm_runtime_mark_last_busy(sensor->dev);
	pm_runtime_put_autosuspend(sensor->dev);

	return ret;
}

/* ------------------------------------------------------------------ */
/* Attribute show/store functions                                      */
/* ------------------------------------------------------------------ */

static ssize_t chip_id_show(struct device *dev,
							struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 chip_id;
	int ret;

	ret = imx708_sysfs_read16(sensor, IMX708_REG_CHIP_ID, &chip_id);
	if (ret)
		return ret;

	return sysfs_emit(buf, "0x%04x\n", chip_id);
}
static DEVICE_ATTR_RO(chip_id);

static ssize_t chip_version_show(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 version;
	int ret;

	ret = imx708_sysfs_read16(sensor, IMX708_REG_MODULE_ID, &version);
	if (ret)
		return ret;

	return sysfs_emit(buf, "0x%04x\n", version);
}
static DEVICE_ATTR_RO(chip_version);

static ssize_t temperature_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 temp;
	int ret;

	ret = imx708_sysfs_read8(sensor, IMX708_REG_TEMPERATURE, &temp);
	if (ret)
		return ret;

	/* Temperature in degrees Celsius (signed 8-bit) */
	return sysfs_emit(buf, "%d\n", (s8)(temp & 0xFF));
}
static DEVICE_ATTR_RO(temperature);

static ssize_t streaming_show(struct device *dev,
							  struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	bool streaming;

	mutex_lock(&sensor->lock);
	streaming = sensor->streaming;
	mutex_unlock(&sensor->lock);

	return sysfs_emit(buf, "%d\n", streaming);
}

static ssize_t streaming_store(struct device *dev,
							   struct device_attribute *attr,
							   const char *buf, size_t count)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	bool enable;
	int ret;

	ret = kstrtobool(buf, &enable);
	if (ret)
		return ret;

	/*
	 * Take the runtime-PM reference before sensor->lock: the resume
	 * callback re-applies the mode and control values and would deadlock
	 * against this mutex otherwise. The reference is held for as long as
	 * the stream is running.
	 */
	if (enable)
	{
		ret = pm_runtime_resume_and_get(sensor->dev);
		if (ret < 0)
			return ret;
	}

	mutex_lock(&sensor->lock);

	if (enable == sensor->streaming)
	{
		mutex_unlock(&sensor->lock);
		if (enable)
			pm_runtime_put(sensor->dev);
		return count;
	}

	if (enable)
	{
		ret = sensor->soc->ops->set_mode(sensor, &sensor->fmt);
		if (!ret)
			ret = sensor->soc->ops->power_on(sensor);
		if (ret)
		{
			mutex_unlock(&sensor->lock);
			pm_runtime_put(sensor->dev);
			return ret;
		}
		sensor->streaming = true;
	}
	else
	{
		ret = sensor->soc->ops->power_off(sensor);
		sensor->streaming = false;
		pm_runtime_mark_last_busy(sensor->dev);
		pm_runtime_put_autosuspend(sensor->dev);
	}

	mutex_unlock(&sensor->lock);

	return ret ? ret : count;
}
static DEVICE_ATTR_RW(streaming);

static ssize_t mode_show(struct device *dev,
						 struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	const struct imx708_mode *mode;
	int len;

	mutex_lock(&sensor->lock);
	mode = sensor->mode;
	if (mode)
		len = sysfs_emit(buf, "%ux%u @ %u fps\n",
						 mode->width, mode->height, mode->fps);
	else
		len = sysfs_emit(buf, "none\n");
	mutex_unlock(&sensor->lock);

	return len;
}
static DEVICE_ATTR_RO(mode);

static ssize_t gain_show(struct device *dev,
						 struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 gain;
	int ret;

	ret = imx708_sysfs_read16(sensor, IMX708_REG_ANALOG_GAIN, &gain);
	if (ret)
		return ret;

	return sysfs_emit(buf, "0x%04x\n", gain);
}

static ssize_t gain_store(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t count)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 gain;
	int ret;

	ret = kstrtou32(buf, 0, &gain);
	if (ret)
		return ret;

	if (gain > IMX708_ANA_GAIN_MAX)
		return -EINVAL;

	ret = pm_runtime_resume_and_get(sensor->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&sensor->lock);
	ret = sensor->soc->ops->set_gain(sensor, gain);
	mutex_unlock(&sensor->lock);

	pm_runtime_mark_last_busy(sensor->dev);
	pm_runtime_put_autosuspend(sensor->dev);

	return ret ? ret : count;
}
static DEVICE_ATTR_RW(gain);

static ssize_t exposure_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 exposure;
	int ret;

	ret = imx708_sysfs_read16(sensor, IMX708_REG_EXPOSURE, &exposure);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%u\n", exposure);
}

static ssize_t exposure_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 exposure;
	int ret;

	ret = kstrtou32(buf, 0, &exposure);
	if (ret)
		return ret;

	if (exposure < IMX708_EXPOSURE_MIN || exposure > IMX708_EXPOSURE_MAX)
		return -EINVAL;

	ret = pm_runtime_resume_and_get(sensor->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&sensor->lock);
	ret = sensor->soc->ops->set_exposure(sensor, exposure);
	mutex_unlock(&sensor->lock);

	pm_runtime_mark_last_busy(sensor->dev);
	pm_runtime_put_autosuspend(sensor->dev);

	return ret ? ret : count;
}
static DEVICE_ATTR_RW(exposure);

static ssize_t test_pattern_show(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 pattern;
	int ret;

	ret = imx708_sysfs_read16(sensor, IMX708_REG_TEST_PATTERN, &pattern);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%u\n", pattern & 0x0F);
}

static ssize_t test_pattern_store(struct device *dev,
								  struct device_attribute *attr,
								  const char *buf, size_t count)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 pattern;
	int ret;

	ret = kstrtou32(buf, 0, &pattern);
	if (ret)
		return ret;

	if (pattern > IMX708_TEST_PATTERN_MAX)
		return -EINVAL;

	ret = pm_runtime_resume_and_get(sensor->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&sensor->lock);
	ret = imx708_write_reg16(sensor, IMX708_REG_TEST_PATTERN, pattern);
	if (!ret)
		sensor->test_pattern = pattern;
	mutex_unlock(&sensor->lock);

	pm_runtime_mark_last_busy(sensor->dev);
	pm_runtime_put_autosuspend(sensor->dev);

	return ret ? ret : count;
}
static DEVICE_ATTR_RW(test_pattern);

static ssize_t pll_locked_show(struct device *dev,
							   struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 status;
	int ret;

	ret = imx708_sysfs_read16(sensor, IMX708_REG_STATUS, &status);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%d\n",
					  !!(status & IMX708_STATUS_PLL_LOCKED));
}
static DEVICE_ATTR_RO(pll_locked);

/*
 * The IMX708 has no host-readable frame counter. Report the count the
 * driver maintains from the frame-end interrupt (0 on boards without an
 * interrupt line) rather than the FRAME_LENGTH timing register, which is
 * a completely different quantity.
 */
static ssize_t frame_count_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	unsigned int count = 0;

	if (sensor->irq_counters)
		count = (unsigned int)atomic_read(&sensor->irq_counters->frame_end);

	return sysfs_emit(buf, "%u\n", count);
}
static DEVICE_ATTR_RO(frame_count);

static ssize_t hdr_mode_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 hdr;

	mutex_lock(&sensor->lock);
	hdr = sensor->hdr_enabled ? 1 : 0;
	mutex_unlock(&sensor->lock);

	return sysfs_emit(buf, "%u\n", hdr);
}

static ssize_t hdr_mode_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 mode;
	int ret;

	ret = kstrtou32(buf, 0, &mode);
	if (ret)
		return ret;

	if (mode > IMX708_HDR_MODE_MAX)
		return -EINVAL;

	ret = pm_runtime_resume_and_get(sensor->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&sensor->lock);
	/* The HDR table reprograms timing/binning; not safe while streaming. */
	if (sensor->streaming)
		ret = -EBUSY;
	else
		ret = sensor->soc->ops->set_hdr(sensor, mode,
										IMX708_HDR_EXPOSURE_RATIO);
	mutex_unlock(&sensor->lock);

	pm_runtime_mark_last_busy(sensor->dev);
	pm_runtime_put_autosuspend(sensor->dev);

	return ret ? ret : count;
}
static DEVICE_ATTR_RW(hdr_mode);

/* ------------------------------------------------------------------ */
/* Attribute groups                                                     */
/* ------------------------------------------------------------------ */

static struct attribute *imx708_attrs[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_chip_version.attr,
	&dev_attr_temperature.attr,
	&dev_attr_streaming.attr,
	&dev_attr_mode.attr,
	&dev_attr_gain.attr,
	&dev_attr_exposure.attr,
	&dev_attr_test_pattern.attr,
	&dev_attr_pll_locked.attr,
	&dev_attr_frame_count.attr,
	&dev_attr_hdr_mode.attr,
	NULL,
};

static const struct attribute_group imx708_attr_group = {
	.attrs = imx708_attrs,
};

const struct attribute_group *imx708_attr_groups[] = {
	&imx708_attr_group,
	NULL,
};
