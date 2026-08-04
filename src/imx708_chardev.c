// SPDX-License-Identifier: GPL-2.0-only
/*
 * imx708_chardev.c - Character device interface for IMX708 sensor
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * This file implements the /dev/imx708* character device interface,
 * providing ioctl-based access to sensor configuration beyond what
 * the V4L2 subdev interface exposes. It owns the cdev lifecycle,
 * file_operations, and ioctl dispatch.
 *
 * Locking: All ioctl handlers take sensor->lock before accessing
 * sensor state. The lock is not held across copy_to_user().
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/capability.h>

#include "imx708_platform.h"
#include "imx708_uapi.h"
#include "imx708_regs.h"

#define DRV_NAME	"imx708"

/* Module-level globals for char device (one class, one region for all instances) */
dev_t		imx708_devt;
struct class	*imx708_class;
#define IMX708_MAX_DEVICES	4

/* ------------------------------------------------------------------ */
/* File operations                                                     */
/* ------------------------------------------------------------------ */

static int imx708_chardev_open(struct inode *inode, struct file *filp)
{
	struct imx708_dev *sensor;

	/* Get the sensor device from the cdev pointer */
	sensor = container_of(inode->i_cdev, struct imx708_dev, cdev);
	if (!sensor)
		return -ENODEV;

	filp->private_data = sensor;
	return 0;
}

static int imx708_chardev_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

static long imx708_chardev_ioctl(struct file *filp, unsigned int cmd,
				  unsigned long arg)
{
	struct imx708_dev *sensor = filp->private_data;
	void __user *argp = (void __user *)arg;
	int ret = 0;

	/* Validate ioctl magic and size */
	if (_IOC_TYPE(cmd) != IMX708_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > IMX708_IOC_MAXNR)
		return -ENOTTY;

	switch (cmd) {
	case IMX708_GET_NUM_MODES: {
		u32 num_modes = sensor->soc->num_modes;
		if (copy_to_user(argp, &num_modes, sizeof(num_modes)))
			return -EFAULT;
		return 0;
	}

	case IMX708_GET_MODE_INFO: {
		struct imx708_mode_info minfo;
		u32 index;

		if (copy_from_user(&index, argp, sizeof(index)))
			return -EFAULT;

		if (index >= sensor->soc->num_modes)
			return -EINVAL;

		mutex_lock(&sensor->lock);
		minfo.width = sensor->soc->modes[index].width;
		minfo.height = sensor->soc->modes[index].height;
		minfo.code = sensor->soc->modes[index].code;
		minfo.fps = sensor->soc->modes[index].fps;
		minfo.hblank = sensor->soc->modes[index].hblank;
		minfo.vblank = sensor->soc->modes[index].vblank;
		minfo.bit_depth = 10;	/* 10-bit raw */
		minfo.__pad = 0;
		mutex_unlock(&sensor->lock);

		if (copy_to_user(argp, &minfo, sizeof(minfo)))
			return -EFAULT;
		return 0;
	}

	case IMX708_GET_STATUS: {
		struct imx708_sensor_status status;
		u32 temp, frame_count, status_reg;

		mutex_lock(&sensor->lock);

		regmap_read(sensor->regmap, IMX708_REG_TEMPERATURE, &temp);
		regmap_read(sensor->regmap, IMX708_REG_STATUS, &status_reg);
		frame_count = 0;	/* frame count not directly readable */

		status.temperature = (s32)temp;
		status.frame_count = frame_count;
		status.pll_locked = !!(status_reg & IMX708_STATUS_PLL_LOCKED);
		status.streaming = sensor->streaming;
		status.error = !!(status_reg & IMX708_STATUS_ERROR);
		memset(status.__pad, 0, sizeof(status.__pad));

		mutex_unlock(&sensor->lock);

		if (copy_to_user(argp, &status, sizeof(status)))
			return -EFAULT;
		return 0;
	}

	case IMX708_SET_GAIN: {
		struct imx708_gain_config cfg;
		if (copy_from_user(&cfg, argp, sizeof(cfg)))
			return -EFAULT;

		mutex_lock(&sensor->lock);
		ret = sensor->soc->ops->set_gain(sensor, cfg.analog_gain);
		if (!ret)
			ret = sensor->soc->ops->set_digital_gain(sensor,
								  cfg.digital_gain);
		mutex_unlock(&sensor->lock);
		return ret;
	}

	case IMX708_GET_GAIN: {
		struct imx708_gain_config cfg;
		u32 val;

		mutex_lock(&sensor->lock);
		regmap_read(sensor->regmap, IMX708_REG_ANALOG_GAIN, &val);
		cfg.analog_gain = val;
		regmap_read(sensor->regmap, IMX708_REG_DIGITAL_GAIN, &val);
		cfg.digital_gain = val;
		cfg.analog_gain_r = 0;
		cfg.analog_gain_gr = 0;
		cfg.analog_gain_gb = 0;
		cfg.analog_gain_b = 0;
		mutex_unlock(&sensor->lock);

		if (copy_to_user(argp, &cfg, sizeof(cfg)))
			return -EFAULT;
		return 0;
	}

	case IMX708_SET_EXPOSURE: {
		struct imx708_exposure_config cfg;
		if (copy_from_user(&cfg, argp, sizeof(cfg)))
			return -EFAULT;

		mutex_lock(&sensor->lock);
		ret = sensor->soc->ops->set_exposure(sensor, cfg.exposure);
		mutex_unlock(&sensor->lock);
		return ret;
	}

	case IMX708_GET_EXPOSURE: {
		struct imx708_exposure_config cfg;
		u32 val;

		mutex_lock(&sensor->lock);
		regmap_read(sensor->regmap, IMX708_REG_EXPOSURE, &val);
		cfg.exposure = val;
		cfg.exposure_r = 0;
		cfg.exposure_gr = 0;
		cfg.exposure_gb = 0;
		cfg.exposure_b = 0;
		mutex_unlock(&sensor->lock);

		if (copy_to_user(argp, &cfg, sizeof(cfg)))
			return -EFAULT;
		return 0;
	}

	case IMX708_SET_HDR: {
		struct imx708_hdr_config cfg;
		if (copy_from_user(&cfg, argp, sizeof(cfg)))
			return -EFAULT;

		/* TODO(HW): Implement HDR mode configuration */
		dev_dbg(sensor->dev, "HDR mode=%u ratio=%u (not yet implemented)\n",
			cfg.mode, cfg.ratio);
		return 0;
	}

	case IMX708_GET_HDR: {
		struct imx708_hdr_config cfg = { 0 };
		if (copy_to_user(argp, &cfg, sizeof(cfg)))
			return -EFAULT;
		return 0;
	}

	case IMX708_SET_TEST_PATTERN: {
		struct imx708_test_pattern_config cfg;

		if (copy_from_user(&cfg, argp, sizeof(cfg)))
			return -EFAULT;

		mutex_lock(&sensor->lock);
		ret = regmap_write(sensor->regmap, IMX708_REG_TEST_PATTERN,
				   cfg.pattern & 0x0F);
		mutex_unlock(&sensor->lock);
		return ret;
	}

	case IMX708_GET_TEST_PATTERN: {
		struct imx708_test_pattern_config cfg;
		u32 val;

		mutex_lock(&sensor->lock);
		regmap_read(sensor->regmap, IMX708_REG_TEST_PATTERN, &val);
		cfg.pattern = val & 0x0F;
		cfg.color = 0;
		cfg.brightness = 0;
		mutex_unlock(&sensor->lock);

		if (copy_to_user(argp, &cfg, sizeof(cfg)))
			return -EFAULT;
		return 0;
	}

	case IMX708_START_STREAM: {
		mutex_lock(&sensor->lock);
		if (!sensor->streaming) {
			ret = sensor->soc->ops->power_on(sensor);
			if (!ret)
				sensor->streaming = true;
		}
		mutex_unlock(&sensor->lock);
		return ret;
	}

	case IMX708_STOP_STREAM: {
		mutex_lock(&sensor->lock);
		if (sensor->streaming) {
			sensor->soc->ops->power_off(sensor);
			sensor->streaming = false;
		}
		mutex_unlock(&sensor->lock);
		return 0;
	}

	case IMX708_SOFT_RESET: {
		/* Software reset: put in standby then back to streaming */
		mutex_lock(&sensor->lock);
		ret = regmap_write(sensor->regmap, IMX708_REG_MODE_SELECT,
				   IMX708_MODE_STANDBY);
		if (!ret) {
			usleep_range(10000, 15000);
			ret = regmap_write(sensor->regmap,
					   IMX708_REG_MODE_SELECT,
					   IMX708_MODE_STREAMING);
		}
		mutex_unlock(&sensor->lock);
		return ret;
	}

	case IMX708_READ_REG: {
		struct imx708_reg_access ra;

		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

		if (copy_from_user(&ra, argp, sizeof(ra)))
			return -EFAULT;

		mutex_lock(&sensor->lock);
		ret = regmap_read(sensor->regmap, ra.reg, &ra.val);
		mutex_unlock(&sensor->lock);

		if (ret)
			return ret;

		if (copy_to_user(argp, &ra, sizeof(ra)))
			return -EFAULT;
		return 0;
	}

	case IMX708_WRITE_REG: {
		struct imx708_reg_access ra;

		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

		if (copy_from_user(&ra, argp, sizeof(ra)))
			return -EFAULT;

		mutex_lock(&sensor->lock);
		ret = regmap_write(sensor->regmap, ra.reg, ra.val);
		mutex_unlock(&sensor->lock);
		return ret;
	}

	default:
		return -ENOTTY;
	}
}

#ifdef CONFIG_COMPAT
static long imx708_chardev_compat_ioctl(struct file *filp,
					  unsigned int cmd, unsigned long arg)
{
	return imx708_chardev_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static const struct file_operations imx708_fops = {
	.owner		= THIS_MODULE,
	.open		= imx708_chardev_open,
	.release	= imx708_chardev_release,
	.unlocked_ioctl	= imx708_chardev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= imx708_chardev_compat_ioctl,
#endif
	.llseek		= noop_llseek,
};

/* ------------------------------------------------------------------ */
/* Char device registration / unregistration                           */
/* ------------------------------------------------------------------ */

int imx708_chardev_register(struct imx708_dev *sensor, unsigned int id)
{
	struct device *dev;
	int ret;

	/* Initialize cdev */
	cdev_init(&sensor->cdev, &imx708_fops);
	sensor->cdev.owner = THIS_MODULE;

	ret = cdev_add(&sensor->cdev, MKDEV(MAJOR(imx708_devt), id), 1);
	if (ret) {
		dev_err(sensor->dev, "failed to add cdev: %d\n", ret);
		return ret;
	}

	/* Create device node */
	dev = device_create(imx708_class, sensor->dev,
			    MKDEV(MAJOR(imx708_devt), id), sensor,
			    DRV_NAME "%u", id);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		dev_err(sensor->dev, "failed to create device: %d\n", ret);
		cdev_del(&sensor->cdev);
		return ret;
	}

	sensor->chardev_id = id;
	dev_dbg(sensor->dev, "char device registered as /dev/%s%u\n",
		DRV_NAME, id);

	return 0;
}

void imx708_chardev_unregister(struct imx708_dev *sensor)
{
	device_destroy(imx708_class, MKDEV(MAJOR(imx708_devt),
					   sensor->chardev_id));
	cdev_del(&sensor->cdev);
}
