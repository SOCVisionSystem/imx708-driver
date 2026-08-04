/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * imx708_uapi.h - Userspace API definitions for IMX708 sensor driver
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * This header defines the ioctl interface shared between kernel and userspace.
 * It uses the proper _IOR/_IOW/_IOWR encoding with a dedicated magic number,
 * fixed-width types, and explicit padding so 32-bit userspace on a 64-bit
 * kernel works without a translation layer.
 *
 * This file is part of the Linux kernel and is licensed under GPL-2.0 WITH
 * Linux-syscall-note exception for inclusion in userspace headers.
 */

#ifndef _IMX708_UAPI_H_
#define _IMX708_UAPI_H_

#include <linux/types.h>
#include <linux/ioctl.h>

/*
 * Magic number for IMX708 ioctls
 *
 * Chosen to avoid collision with other media drivers. The V4L2 subsystem
 * uses 'V' (0x56). We use 'I' (0x49) for IMX708-specific extensions.
 */
#define IMX708_IOC_MAGIC	0x49

/*
 * IOCTL numbers and their structs
 *
 * All structs use fixed-width types (__u32, __s32, __u64) and explicit
 * padding to ensure the same layout on 32-bit and 64-bit systems.
 */

/**
 * struct imx708_mode_info - Sensor mode description
 * @width:        Active pixel width
 * @height:       Active pixel height
 * @code:         Media bus code (MEDIA_BUS_FMT_*)
 * @fps:          Frames per second
 * @hblank:       Horizontal blanking in pixels
 * @vblank:       Vertical blanking in lines
 * @bit_depth:    Bits per pixel
 * @__pad:        Padding to 64-bit alignment
 */
struct imx708_mode_info {
	__u32 width;
	__u32 height;
	__u32 code;
	__u32 fps;
	__u32 hblank;
	__u32 vblank;
	__u32 bit_depth;
	__u32 __pad;
};

/**
 * struct imx708_sensor_status - Live sensor status
 * @temperature:  Die temperature in degrees Celsius (signed)
 * @frame_count:  Frame counter since stream start
 * @pll_locked:   PLL lock status (1=locked, 0=unlocked)
 * @streaming:    Streaming state (1=active, 0=standby)
 * @error:        Error flag
 * @__pad:        Padding to 64-bit alignment
 */
struct imx708_sensor_status {
	__s32 temperature;
	__u32 frame_count;
	__u8  pll_locked;
	__u8  streaming;
	__u8  error;
	__u8  __pad[5];
};

/**
 * struct imx708_gain_config - Gain configuration
 * @analog_gain:     Analog gain in sensor-specific units
 * @digital_gain:    Digital gain in sensor-specific units
 * @analog_gain_r:   Red channel analog gain (0 = auto)
 * @analog_gain_gr:  Green-R channel analog gain (0 = auto)
 * @analog_gain_gb:  Green-B channel analog gain (0 = auto)
 * @analog_gain_b:   Blue channel analog gain (0 = auto)
 */
struct imx708_gain_config {
	__u32 analog_gain;
	__u32 digital_gain;
	__u32 analog_gain_r;
	__u32 analog_gain_gr;
	__u32 analog_gain_gb;
	__u32 analog_gain_b;
};

/**
 * struct imx708_exposure_config - Exposure configuration
 * @exposure:     Exposure time in line units
 * @exposure_r:   Red channel exposure (0 = same as exposure)
 * @exposure_gr:  Green-R channel exposure (0 = same)
 * @exposure_gb:  Green-B channel exposure (0 = same)
 * @exposure_b:   Blue channel exposure (0 = same)
 */
struct imx708_exposure_config {
	__u32 exposure;
	__u32 exposure_r;
	__u32 exposure_gr;
	__u32 exposure_gb;
	__u32 exposure_b;
};

/**
 * struct imx708_hdr_config - HDR mode configuration
 * @mode:         HDR mode (0=off, 1=2-exposure line-interleaved)
 * @ratio:        HDR ratio (long/short exposure ratio, 0 = keep current)
 * @exposure_s:   Short exposure time (0 = auto-calculated)
 * @gain_s:       Short exposure gain (0 = auto-calculated)
 *
 * DOL HDR is not supported; IMX708_SET_HDR returns -EINVAL for mode > 1.
 * HDR cannot be reconfigured while streaming (-EBUSY).
 */
struct imx708_hdr_config {
	__u32 mode;
	__u32 ratio;
	__u32 exposure_s;
	__u32 gain_s;
};

/**
 * struct imx708_test_pattern_config - Test pattern configuration
 * @pattern:      Test pattern type (0=off, 1=color bars, 2=solid, 3=checker)
 * @color:        Solid color value (when pattern=2)
 * @brightness:   Pattern brightness (0-255)
 */
struct imx708_test_pattern_config {
	__u32 pattern;
	__u32 color;
	__u32 brightness;
};

/*
 * IOCTL definitions
 *
 * Direction key:
 *   _IOR  - read from sensor (device → userspace)
 *   _IOW  - write to sensor (userspace → device)
 *   _IOWR - read-modify-write
 */

/* Query number of available sensor modes */
#define IMX708_GET_NUM_MODES	_IOR(IMX708_IOC_MAGIC, 1, __u32)

/* Get mode info for a given index (index in argp) */
#define IMX708_GET_MODE_INFO	_IOWR(IMX708_IOC_MAGIC, 2, struct imx708_mode_info)

/* Get live sensor status */
#define IMX708_GET_STATUS	_IOR(IMX708_IOC_MAGIC, 3, struct imx708_sensor_status)

/* Set gain (analog + digital, per-channel) */
#define IMX708_SET_GAIN		_IOW(IMX708_IOC_MAGIC, 4, struct imx708_gain_config)

/* Get current gain settings */
#define IMX708_GET_GAIN		_IOR(IMX708_IOC_MAGIC, 5, struct imx708_gain_config)

/* Set exposure time */
#define IMX708_SET_EXPOSURE	_IOW(IMX708_IOC_MAGIC, 6, struct imx708_exposure_config)

/* Get current exposure settings */
#define IMX708_GET_EXPOSURE	_IOR(IMX708_IOC_MAGIC, 7, struct imx708_exposure_config)

/* Set HDR mode */
#define IMX708_SET_HDR		_IOW(IMX708_IOC_MAGIC, 8, struct imx708_hdr_config)

/* Get current HDR configuration */
#define IMX708_GET_HDR		_IOR(IMX708_IOC_MAGIC, 9, struct imx708_hdr_config)

/* Set test pattern */
#define IMX708_SET_TEST_PATTERN	_IOW(IMX708_IOC_MAGIC, 10, struct imx708_test_pattern_config)

/* Get current test pattern */
#define IMX708_GET_TEST_PATTERN	_IOR(IMX708_IOC_MAGIC, 11, struct imx708_test_pattern_config)

/* Start streaming */
#define IMX708_START_STREAM	_IO(IMX708_IOC_MAGIC, 12)

/* Stop streaming */
#define IMX708_STOP_STREAM	_IO(IMX708_IOC_MAGIC, 13)

/* Software reset the sensor */
#define IMX708_SOFT_RESET	_IO(IMX708_IOC_MAGIC, 14)

/* Read raw register (debug, requires CAP_SYS_ADMIN) */
#define IMX708_READ_REG		_IOWR(IMX708_IOC_MAGIC, 15, struct imx708_reg_access)

/* Write raw register (debug, requires CAP_SYS_ADMIN) */
#define IMX708_WRITE_REG	_IOW(IMX708_IOC_MAGIC, 16, struct imx708_reg_access)

/**
 * struct imx708_reg_access - Raw register read/write (debug only)
 * @reg:          Register address
 * @val:          Register value (read returns, write sets)
 */
struct imx708_reg_access {
	__u32 reg;
	__u32 val;
};

/* Maximum number of ioctls */
#define IMX708_IOC_MAXNR		16

#endif /* _IMX708_UAPI_H_ */
