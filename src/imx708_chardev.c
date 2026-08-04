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
 * file_operations, and ioctl dispatch. This is the ABI consumed by
 * libimx708, the gRPC server and the GUI client.
 *
 * Locking:
 *   All ioctl handlers take sensor->lock before accessing sensor state.
 *   The lock is not held across copy_to_user()/copy_from_user().
 *
 * Power:
 *   The sensor is runtime-suspended (supplies off) when idle, so every
 *   handler that talks to the hardware takes a runtime-PM reference. The
 *   reference must be taken *before* sensor->lock, because the resume
 *   callback re-applies controls and takes the same mutex.
 *
 * Lifetime:
 *   An open file descriptor can outlive driver unbind. open() takes a
 *   reference on struct imx708_dev and release() drops it, and every
 *   handler checks sensor->removed before touching hardware that has
 *   already been torn down.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/capability.h>

#include "imx708_platform.h"
#include "imx708_uapi.h"
#include "imx708_regs.h"

#define DRV_NAME "imx708"

/* Module-level globals for char device (one class, one region for all instances) */
dev_t imx708_devt;
struct class *imx708_class;
#define IMX708_MAX_DEVICES 4

void imx708_dev_release(struct kref *kref)
{
	struct imx708_dev *sensor = container_of(kref, struct imx708_dev,
											 refcount);

	mutex_destroy(&sensor->lock);
	kfree(sensor);
}

/* ------------------------------------------------------------------ */
/* File operations                                                     */
/* ------------------------------------------------------------------ */

static int imx708_chardev_open(struct inode *inode, struct file *filp)
{
	struct imx708_dev *sensor;

	/* Get the sensor device from the cdev pointer */
	sensor = container_of(inode->i_cdev, struct imx708_dev, cdev);

	/* Keep the structure alive for as long as this fd is open. */
	imx708_dev_get(sensor);

	filp->private_data = sensor;
	return 0;
}

static int imx708_chardev_release(struct inode *inode, struct file *filp)
{
	struct imx708_dev *sensor = filp->private_data;

	filp->private_data = NULL;
	if (sensor)
		imx708_dev_put(sensor);

	return 0;
}

/*
 * Resume the sensor and take a runtime-PM reference, rejecting the call if
 * the device has been unbound in the meantime. Must be called without
 * sensor->lock held.
 */
static int imx708_ioctl_get_hw(struct imx708_dev *sensor)
{
	int ret;

	if (READ_ONCE(sensor->removed))
		return -ENODEV;

	ret = pm_runtime_resume_and_get(sensor->dev);
	if (ret < 0)
		return ret;

	if (READ_ONCE(sensor->removed))
	{
		pm_runtime_put(sensor->dev);
		return -ENODEV;
	}

	return 0;
}

static void imx708_ioctl_put_hw(struct imx708_dev *sensor)
{
	pm_runtime_mark_last_busy(sensor->dev);
	pm_runtime_put_autosuspend(sensor->dev);
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

	switch (cmd)
	{
	case IMX708_GET_NUM_MODES:
	{
		u32 num_modes = sensor->soc->num_modes;

		if (copy_to_user(argp, &num_modes, sizeof(num_modes)))
			return -EFAULT;
		return 0;
	}

	case IMX708_GET_MODE_INFO:
	{
		struct imx708_mode_info minfo;
		u32 index;

		/*
		 * The caller passes the mode index in the first field of the
		 * (larger) imx708_mode_info buffer, so only that word is read
		 * back here; the whole struct is returned below.
		 */
		if (copy_from_user(&index, argp, sizeof(index)))
			return -EFAULT;

		if (index >= sensor->soc->num_modes)
			return -EINVAL;

		memset(&minfo, 0, sizeof(minfo));
		minfo.width = sensor->soc->modes[index].width;
		minfo.height = sensor->soc->modes[index].height;
		minfo.code = sensor->soc->modes[index].code;
		minfo.fps = sensor->soc->modes[index].fps;
		minfo.hblank = sensor->soc->modes[index].hblank;
		minfo.vblank = sensor->soc->modes[index].vblank;
		minfo.bit_depth = 10; /* 10-bit raw */

		if (copy_to_user(argp, &minfo, sizeof(minfo)))
			return -EFAULT;
		return 0;
	}

	case IMX708_GET_STATUS:
	{
		struct imx708_sensor_status status;
		u32 temp = 0, status_reg = 0;

		ret = imx708_ioctl_get_hw(sensor);
		if (ret)
			return ret;

		memset(&status, 0, sizeof(status));

		mutex_lock(&sensor->lock);
		regmap_read(sensor->regmap, IMX708_REG_TEMPERATURE, &temp);
		imx708_read_reg16(sensor, IMX708_REG_STATUS, &status_reg);

		status.temperature = (s8)(temp & 0xff);
		status.frame_count = sensor->irq_counters ? (u32)atomic_read(&sensor->irq_counters->frame_end) : 0;
		status.pll_locked = !!(status_reg & IMX708_STATUS_PLL_LOCKED);
		status.streaming = sensor->streaming;
		status.error = !!(status_reg & IMX708_STATUS_ERROR);
		mutex_unlock(&sensor->lock);

		imx708_ioctl_put_hw(sensor);

		if (copy_to_user(argp, &status, sizeof(status)))
			return -EFAULT;
		return 0;
	}

	case IMX708_SET_GAIN:
	{
		struct imx708_gain_config cfg;

		if (copy_from_user(&cfg, argp, sizeof(cfg)))
			return -EFAULT;

		ret = imx708_ioctl_get_hw(sensor);
		if (ret)
			return ret;

		mutex_lock(&sensor->lock);
		ret = sensor->soc->ops->set_gain(sensor, cfg.analog_gain);
		if (!ret)
			ret = sensor->soc->ops->set_digital_gain(sensor,
													 cfg.digital_gain);
		mutex_unlock(&sensor->lock);

		imx708_ioctl_put_hw(sensor);
		return ret;
	}

	case IMX708_GET_GAIN:
	{
		struct imx708_gain_config cfg;
		u32 val = 0;

		ret = imx708_ioctl_get_hw(sensor);
		if (ret)
			return ret;

		memset(&cfg, 0, sizeof(cfg));

		mutex_lock(&sensor->lock);
		imx708_read_reg16(sensor, IMX708_REG_ANALOG_GAIN, &val);
		cfg.analog_gain = val;
		imx708_read_reg16(sensor, IMX708_REG_DIGITAL_GAIN, &val);
		cfg.digital_gain = val;
		mutex_unlock(&sensor->lock);

		imx708_ioctl_put_hw(sensor);

		if (copy_to_user(argp, &cfg, sizeof(cfg)))
			return -EFAULT;
		return 0;
	}

	case IMX708_SET_EXPOSURE:
	{
		struct imx708_exposure_config cfg;

		if (copy_from_user(&cfg, argp, sizeof(cfg)))
			return -EFAULT;

		ret = imx708_ioctl_get_hw(sensor);
		if (ret)
			return ret;

		mutex_lock(&sensor->lock);
		ret = sensor->soc->ops->set_exposure(sensor, cfg.exposure);
		mutex_unlock(&sensor->lock);

		imx708_ioctl_put_hw(sensor);
		return ret;
	}

	case IMX708_GET_EXPOSURE:
	{
		struct imx708_exposure_config cfg;
		u32 val = 0;

		ret = imx708_ioctl_get_hw(sensor);
		if (ret)
			return ret;

		memset(&cfg, 0, sizeof(cfg));

		mutex_lock(&sensor->lock);
		imx708_read_reg16(sensor, IMX708_REG_EXPOSURE, &val);
		cfg.exposure = val;
		mutex_unlock(&sensor->lock);

		imx708_ioctl_put_hw(sensor);

		if (copy_to_user(argp, &cfg, sizeof(cfg)))
			return -EFAULT;
		return 0;
	}

	case IMX708_SET_HDR:
	{
		struct imx708_hdr_config cfg;

		if (copy_from_user(&cfg, argp, sizeof(cfg)))
			return -EFAULT;

		if (cfg.mode > IMX708_HDR_MODE_MAX)
			return -EINVAL;

		ret = imx708_ioctl_get_hw(sensor);
		if (ret)
			return ret;

		mutex_lock(&sensor->lock);
		/* The HDR table reprograms timing; not safe while streaming. */
		if (sensor->streaming)
			ret = -EBUSY;
		else
			ret = sensor->soc->ops->set_hdr(sensor, cfg.mode,
											cfg.ratio);
		mutex_unlock(&sensor->lock);

		imx708_ioctl_put_hw(sensor);
		return ret;
	}

	case IMX708_GET_HDR:
	{
		struct imx708_hdr_config cfg;

		memset(&cfg, 0, sizeof(cfg));

		mutex_lock(&sensor->lock);
		cfg.mode = sensor->hdr_enabled ? 1 : 0;
		cfg.ratio = sensor->hdr_ratio;
		mutex_unlock(&sensor->lock);

		if (copy_to_user(argp, &cfg, sizeof(cfg)))
			return -EFAULT;
		return 0;
	}

	case IMX708_SET_TEST_PATTERN:
	{
		struct imx708_test_pattern_config cfg;

		if (copy_from_user(&cfg, argp, sizeof(cfg)))
			return -EFAULT;

		if (cfg.pattern > IMX708_TEST_PATTERN_MAX)
			return -EINVAL;

		ret = imx708_ioctl_get_hw(sensor);
		if (ret)
			return ret;

		mutex_lock(&sensor->lock);
		ret = imx708_write_reg16(sensor, IMX708_REG_TEST_PATTERN,
								 cfg.pattern);
		if (!ret)
			sensor->test_pattern = cfg.pattern;
		mutex_unlock(&sensor->lock);

		imx708_ioctl_put_hw(sensor);
		return ret;
	}

	case IMX708_GET_TEST_PATTERN:
	{
		struct imx708_test_pattern_config cfg;
		u32 val = 0;

		ret = imx708_ioctl_get_hw(sensor);
		if (ret)
			return ret;

		memset(&cfg, 0, sizeof(cfg));

		mutex_lock(&sensor->lock);
		imx708_read_reg16(sensor, IMX708_REG_TEST_PATTERN, &val);
		cfg.pattern = val & IMX708_TEST_PATTERN_MASK;
		mutex_unlock(&sensor->lock);

		imx708_ioctl_put_hw(sensor);

		if (copy_to_user(argp, &cfg, sizeof(cfg)))
			return -EFAULT;
		return 0;
	}

	case IMX708_START_STREAM:
	{
		/*
		 * Mirrors imx708_s_stream(): the runtime-PM reference is taken
		 * here and held for the whole streaming session, and the mode
		 * registers are programmed before leaving standby.
		 */
		ret = imx708_ioctl_get_hw(sensor);
		if (ret)
			return ret;

		mutex_lock(&sensor->lock);

		if (sensor->streaming)
		{
			mutex_unlock(&sensor->lock);
			imx708_ioctl_put_hw(sensor);
			return 0;
		}

		ret = sensor->soc->ops->set_mode(sensor, &sensor->fmt);
		if (!ret)
			ret = sensor->soc->ops->power_on(sensor);

		if (ret)
		{
			mutex_unlock(&sensor->lock);
			imx708_ioctl_put_hw(sensor);
			return ret;
		}

		sensor->streaming = true;
		mutex_unlock(&sensor->lock);

		/* Reference intentionally retained until STOP_STREAM. */
		return 0;
	}

	case IMX708_STOP_STREAM:
	{
		mutex_lock(&sensor->lock);

		if (!sensor->streaming)
		{
			mutex_unlock(&sensor->lock);
			return 0;
		}

		sensor->soc->ops->power_off(sensor);
		sensor->streaming = false;
		mutex_unlock(&sensor->lock);

		/* Release the reference taken by START_STREAM. */
		imx708_ioctl_put_hw(sensor);
		return 0;
	}

	case IMX708_SOFT_RESET:
	{
		bool was_streaming;

		ret = imx708_ioctl_get_hw(sensor);
		if (ret)
			return ret;

		mutex_lock(&sensor->lock);
		was_streaming = sensor->streaming;

		/* Standby, re-run init and the mode registers, then restore. */
		ret = regmap_write(sensor->regmap, IMX708_REG_MODE_SELECT,
						   IMX708_MODE_STANDBY);
		if (!ret)
		{
			usleep_range(10000, 15000);
			ret = sensor->soc->ops->init(sensor);
		}
		if (!ret)
			ret = sensor->soc->ops->set_mode(sensor, &sensor->fmt);
		if (!ret && was_streaming)
			ret = sensor->soc->ops->power_on(sensor);

		if (ret)
			sensor->streaming = false;
		mutex_unlock(&sensor->lock);

		imx708_ioctl_put_hw(sensor);

		/*
		 * If the stream could not be restarted, drop the reference
		 * START_STREAM was holding on its behalf.
		 */
		if (ret && was_streaming)
			imx708_ioctl_put_hw(sensor);

		return ret;
	}

	case IMX708_READ_REG:
	{
		struct imx708_reg_access ra;

		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

		if (copy_from_user(&ra, argp, sizeof(ra)))
			return -EFAULT;

		if (ra.reg > IMX708_REG_ADDR_MAX)
			return -EINVAL;

		ret = imx708_ioctl_get_hw(sensor);
		if (ret)
			return ret;

		mutex_lock(&sensor->lock);
		ret = regmap_read(sensor->regmap, ra.reg, &ra.val);
		mutex_unlock(&sensor->lock);

		imx708_ioctl_put_hw(sensor);

		if (ret)
			return ret;

		if (copy_to_user(argp, &ra, sizeof(ra)))
			return -EFAULT;
		return 0;
	}

	case IMX708_WRITE_REG:
	{
		struct imx708_reg_access ra;

		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

		if (copy_from_user(&ra, argp, sizeof(ra)))
			return -EFAULT;

		/* Registers are 16-bit addressed with 8-bit data. */
		if (ra.reg > IMX708_REG_ADDR_MAX || ra.val > 0xff)
			return -EINVAL;

		ret = imx708_ioctl_get_hw(sensor);
		if (ret)
			return ret;

		mutex_lock(&sensor->lock);
		ret = regmap_write(sensor->regmap, ra.reg, ra.val);
		mutex_unlock(&sensor->lock);

		imx708_ioctl_put_hw(sensor);
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
	.owner = THIS_MODULE,
	.open = imx708_chardev_open,
	.release = imx708_chardev_release,
	.unlocked_ioctl = imx708_chardev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = imx708_chardev_compat_ioctl,
#endif
	.llseek = noop_llseek,
};

/* ------------------------------------------------------------------ */
/* Char device registration / unregistration                           */
/* ------------------------------------------------------------------ */

int imx708_chardev_register(struct imx708_dev *sensor, unsigned int id)
{
	struct device *dev;
	int ret;

	if (id >= IMX708_MAX_DEVICES)
		return -EINVAL;

	/* Initialize cdev */
	cdev_init(&sensor->cdev, &imx708_fops);
	sensor->cdev.owner = THIS_MODULE;

	ret = cdev_add(&sensor->cdev, MKDEV(MAJOR(imx708_devt), id), 1);
	if (ret)
	{
		dev_err(sensor->dev, "failed to add cdev: %d\n", ret);
		return ret;
	}

	/* Create device node */
	dev = device_create(imx708_class, sensor->dev,
						MKDEV(MAJOR(imx708_devt), id), sensor,
						DRV_NAME "%u", id);
	if (IS_ERR(dev))
	{
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
	/*
	 * Stop new hardware access first. Handlers already in flight are
	 * serialised by sensor->lock, which the caller does not hold.
	 */
	WRITE_ONCE(sensor->removed, true);

	device_destroy(imx708_class, MKDEV(MAJOR(imx708_devt),
									   sensor->chardev_id));
	cdev_del(&sensor->cdev);
}
