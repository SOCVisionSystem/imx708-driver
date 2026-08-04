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
 * Locking: All show/store handlers take sensor->lock.
 */

#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/kernel.h>

#include "imx708_platform.h"
#include "imx708_regs.h"

/* ------------------------------------------------------------------ */
/* Attribute show/store functions                                      */
/* ------------------------------------------------------------------ */

static ssize_t chip_id_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 chip_id;
	int ret;

	mutex_lock(&sensor->lock);
	ret = regmap_read(sensor->regmap, IMX708_REG_CHIP_ID, &chip_id);
	mutex_unlock(&sensor->lock);

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

	mutex_lock(&sensor->lock);
	ret = regmap_read(sensor->regmap, IMX708_REG_MODULE_ID, &version);
	mutex_unlock(&sensor->lock);

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

	mutex_lock(&sensor->lock);
	ret = regmap_read(sensor->regmap, IMX708_REG_TEMPERATURE, &temp);
	mutex_unlock(&sensor->lock);

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

	mutex_lock(&sensor->lock);
	if (enable && !sensor->streaming) {
		ret = sensor->soc->ops->power_on(sensor);
		if (!ret)
			sensor->streaming = true;
	} else if (!enable && sensor->streaming) {
		sensor->soc->ops->power_off(sensor);
		sensor->streaming = false;
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

	mutex_lock(&sensor->lock);
	mode = sensor->mode;
	if (mode)
		sysfs_emit(buf, "%ux%u @ %u fps\n",
			   mode->width, mode->height, mode->fps);
	else
		sysfs_emit(buf, "none\n");
	mutex_unlock(&sensor->lock);

	return 0;
}
static DEVICE_ATTR_RO(mode);

static ssize_t gain_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 gain;
	int ret;

	mutex_lock(&sensor->lock);
	ret = regmap_read(sensor->regmap, IMX708_REG_ANALOG_GAIN, &gain);
	mutex_unlock(&sensor->lock);

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

	mutex_lock(&sensor->lock);
	ret = sensor->soc->ops->set_gain(sensor, gain);
	mutex_unlock(&sensor->lock);

	return ret ? ret : count;
}
static DEVICE_ATTR_RW(gain);

static ssize_t exposure_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 exposure;
	int ret;

	mutex_lock(&sensor->lock);
	ret = regmap_read(sensor->regmap, IMX708_REG_EXPOSURE, &exposure);
	mutex_unlock(&sensor->lock);

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

	mutex_lock(&sensor->lock);
	ret = sensor->soc->ops->set_exposure(sensor, exposure);
	mutex_unlock(&sensor->lock);

	return ret ? ret : count;
}
static DEVICE_ATTR_RW(exposure);

static ssize_t test_pattern_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 pattern;
	int ret;

	mutex_lock(&sensor->lock);
	ret = regmap_read(sensor->regmap, IMX708_REG_TEST_PATTERN, &pattern);
	mutex_unlock(&sensor->lock);

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

	if (pattern > 4)
		return -EINVAL;

	mutex_lock(&sensor->lock);
	ret = regmap_write(sensor->regmap, IMX708_REG_TEST_PATTERN, pattern);
	mutex_unlock(&sensor->lock);

	return ret ? ret : count;
}
static DEVICE_ATTR_RW(test_pattern);

static ssize_t pll_locked_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 status;
	int ret;

	mutex_lock(&sensor->lock);
	ret = regmap_read(sensor->regmap, IMX708_REG_STATUS, &status);
	mutex_unlock(&sensor->lock);

	if (ret)
		return ret;

	return sysfs_emit(buf, "%d\n",
			  !!(status & IMX708_STATUS_PLL_LOCKED));
}
static DEVICE_ATTR_RO(pll_locked);

static ssize_t frame_count_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 frame_count;
	int ret;

	mutex_lock(&sensor->lock);
	ret = regmap_read(sensor->regmap,
			  IMX708_REG_FRAME_LENGTH, &frame_count);
	mutex_unlock(&sensor->lock);

	if (ret)
		return ret;

	return sysfs_emit(buf, "%u\n", frame_count);
}
static DEVICE_ATTR_RO(frame_count);

static ssize_t hdr_mode_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct imx708_dev *sensor = dev_get_drvdata(dev);
	u32 hdr = 0;

	mutex_lock(&sensor->lock);
	/* TODO(HW): Read actual HDR mode register */
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

	if (mode > 2)
		return -EINVAL;

	/* TODO(HW): Implement HDR mode switching */
	dev_dbg(sensor->dev, "HDR mode %u (not yet implemented)\n", mode);

	return count;
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
