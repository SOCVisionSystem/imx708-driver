// SPDX-License-Identifier: GPL-2.0-only
/*
 * imx708_platform.c - Platform-specific data and operations for IMX708
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * This file contains every SoC-specific hardware fact: register offsets,
 * mode tables, clock names, GPIO names, and the OF match table binding
 * compatible strings to their imx708_soc_data instances.
 *
 * Register values verified against the upstream Raspberry Pi kernel driver
 * (rpi-6.6.y, drivers/media/i2c/imx708.c) and the Sony IMX708-AAJH5-C
 * datasheet.
 *
 * To add a new platform:
 *   1. Define a new imx708_soc_data instance below
 *   2. Add an entry to imx708_of_match[] in imx708_main.c
 *   3. Implement the hw_ops callbacks
 *   4. No changes to any other file are needed.
 */

#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/device.h>

#include "imx708_platform.h"
#include "imx708_regs.h"

/* PDAF correction gains (from upstream driver) */
const u8 imx708_pdaf_gains_left[9] = {
	0x4c, 0x4c, 0x4c, 0x46, 0x3e, 0x38, 0x35, 0x35, 0x35
};

const u8 imx708_pdaf_gains_right[9] = {
	0x35, 0x35, 0x35, 0x38, 0x3e, 0x46, 0x4c, 0x4c, 0x4c
};

/* ------------------------------------------------------------------ */
/* 16-bit register helpers                                             */
/* ------------------------------------------------------------------ */

/*
 * regmap is configured with 8-bit values because that is what the sensor
 * actually implements. Fields wider than one byte occupy consecutive
 * addresses, most significant byte first.
 */
int imx708_read_reg16(struct imx708_dev *sensor, u32 reg, u32 *val)
{
	u32 hi, lo;
	int ret;

	ret = regmap_read(sensor->regmap, reg, &hi);
	if (ret)
		return ret;

	ret = regmap_read(sensor->regmap, reg + 1, &lo);
	if (ret)
		return ret;

	*val = ((hi & 0xff) << 8) | (lo & 0xff);
	return 0;
}

int imx708_write_reg16(struct imx708_dev *sensor, u32 reg, u32 val)
{
	int ret;

	ret = regmap_write(sensor->regmap, reg, (val >> 8) & 0xff);
	if (ret)
		return ret;

	return regmap_write(sensor->regmap, reg + 1, val & 0xff);
}

/* ------------------------------------------------------------------ */
/* Common register sequence (written once at power-on)                 */
/* ------------------------------------------------------------------ */

static const struct reg_sequence imx708_common_regs[] = {
	{ 0x0100, 0x00 },
	{ 0x0136, 0x18 },
	{ 0x0137, 0x00 },
	{ 0x33f0, 0x02 },
	{ 0x33f1, 0x05 },
	{ 0x3062, 0x00 },
	{ 0x3063, 0x12 },
	{ 0x3068, 0x00 },
	{ 0x3069, 0x12 },
	{ 0x306a, 0x00 },
	{ 0x306b, 0x30 },
	{ 0x3076, 0x00 },
	{ 0x3077, 0x30 },
	{ 0x3078, 0x00 },
	{ 0x3079, 0x30 },
	{ 0x5e54, 0x0c },
	{ 0x6e44, 0x00 },
	{ 0xb0b6, 0x01 },
	{ 0xe829, 0x00 },
	{ 0xf001, 0x08 },
	{ 0xf003, 0x08 },
	{ 0xf00d, 0x10 },
	{ 0xf00f, 0x10 },
	{ 0xf031, 0x08 },
	{ 0xf033, 0x08 },
	{ 0xf03d, 0x10 },
	{ 0xf03f, 0x10 },
	{ 0x0112, 0x0a },
	{ 0x0113, 0x0a },
	{ 0x0114, 0x01 },
	{ 0x0b8e, 0x01 },
	{ 0x0b8f, 0x00 },
	{ 0x0b94, 0x01 },
	{ 0x0b95, 0x00 },
	{ 0x3400, 0x01 },
	{ 0x3478, 0x01 },
	{ 0x3479, 0x1c },
	{ 0x3091, 0x01 },
	{ 0x3092, 0x00 },
	{ 0x3419, 0x00 },
	{ 0xbcf1, 0x02 },
	{ 0x3094, 0x01 },
	{ 0x3095, 0x01 },
	{ 0x3362, 0x00 },
	{ 0x3363, 0x00 },
	{ 0x3364, 0x00 },
	{ 0x3365, 0x00 },
	{ 0x0138, 0x01 },
	/* PLL config */
	{ 0x0301, 0x05 },
	{ 0x0303, 0x02 },
	{ 0x0305, 0x02 },
	{ 0x0306, 0x00 }, { 0x0307, 0x7c },
	{ 0x030b, 0x02 },
	{ 0x030d, 0x04 },
	{ 0x0310, 0x01 },
	/* non-HDR defaults */
	{ 0x0220, 0x62 },
	{ 0x0222, 0x01 },
	{ 0x350c, 0x00 },
	{ 0x350d, 0x00 },
	{ 0x0224, 0x01 }, { 0x0225, 0xf4 },	/* SHT_EXPOSURE = 500 */
	{ 0x3116, 0x01 }, { 0x3117, 0xf4 },	/* MID_EXPOSURE = 500 */
	{ 0x0216, 0x00 }, { 0x0217, 0x00 },	/* SHT_ANALOG_GAIN = 0 */
	{ 0x0218, 0x01 }, { 0x0219, 0x00 },	/* SHT_DIGITAL_GAIN = 0x100 */
	{ 0x3118, 0x00 }, { 0x3119, 0x00 },	/* MID_ANALOG_GAIN = 0 */
	{ 0x311a, 0x01 }, { 0x311b, 0x00 },	/* MID_DIGITAL_GAIN = 0x100 */
	{ 0x3366, 0x00 }, { 0x3367, 0x00 },	/* AEHIST1_AREA_WIDTH = 0 */
	{ 0x3368, 0x00 }, { 0x3369, 0x00 },	/* AEHIST1_AREA_HEIGHT = 0 */
	/* Quad-Bayer Compensation */
	{ IMX708_REG_QBC_RMSC_EN, 0x01 },
	{ IMX708_REG_LPF_INTENSITY, IMX708_LPF_INTENSITY_DEFAULT },
	{ IMX708_REG_LPF_INTENSITY_EN, IMX708_LPF_INTENSITY_ENABLED },
	{ 0x32d6, 0x00 },
	{ 0x32db, 0x01 },
	/* Analogue crop disabled */
	{ IMX708_REG_ACROPLP_EN, 0x00 },
	/* Unknown registers (from upstream driver) */
	{ 0x3ca0, 0x00 }, { 0x3ca1, 0x64 },
	{ 0x3ca4, 0x00 }, { 0x3ca5, 0x00 },
	{ 0x3ca6, 0x00 }, { 0x3ca7, 0x00 },
	{ 0x3caa, 0x00 }, { 0x3cab, 0x00 },
	{ 0x3cb8, 0x00 }, { 0x3cb9, 0x08 },
	{ 0x3cba, 0x00 }, { 0x3cbb, 0x00 },
	{ 0x3cbc, 0x00 }, { 0x3cbd, 0x3c },
	{ 0x3cbe, 0x00 }, { 0x3cbf, 0x00 },
	{ 0x341a, 0x00 }, { 0x341b, 0x00 },
	{ 0x341c, 0x00 }, { 0x341d, 0x00 },
	{ 0x341e, 0x01 }, { 0x341f, 0x20 },
	{ 0x3420, 0x00 }, { 0x3421, 0xd8 },
};

/* ------------------------------------------------------------------ */
/* Mode register sequences (from upstream Raspberry Pi kernel driver)  */
/* ------------------------------------------------------------------ */

/* 4608 × 2592 full resolution, 10-bit, 2-lane MIPI */
static const struct reg_sequence imx708_mode_4608x2592_regs[] = {
	{ 0x0342, 0x3d }, { 0x0343, 0x20 },	/* LINE_LENGTH = 15648 */
	{ 0x0340, 0x0a }, { 0x0341, 0x59 },	/* FRAME_LENGTH = 2649 */
	{ 0x0344, 0x00 }, { 0x0345, 0x00 },	/* X_ADD_STA = 0 */
	{ 0x0346, 0x00 }, { 0x0347, 0x00 },	/* Y_ADD_STA = 0 */
	{ 0x0348, 0x11 }, { 0x0349, 0xff },	/* X_ADD_END = 4607 */
	{ 0x034a, 0x0a }, { 0x034b, 0x1f },	/* Y_ADD_END = 2591 */
	{ 0x0220, 0x62 }, { 0x0222, 0x01 },	/* HDR off */
	{ 0x0900, 0x00 }, { 0x0901, 0x11 },	/* No binning */
	{ 0x0902, 0x0a },
	{ 0x3200, 0x01 }, { 0x3201, 0x01 },	/* Binning priority */
	{ 0x32d5, 0x01 }, { 0x32d6, 0x00 },	/* QBC remosaic on */
	{ 0x32db, 0x01 }, { 0x32df, 0x00 },
	{ 0x350c, 0x00 }, { 0x350d, 0x00 },
	{ 0x0408, 0x00 }, { 0x0409, 0x00 },	/* Digital crop off */
	{ 0x040a, 0x00 }, { 0x040b, 0x00 },
	{ 0x040c, 0x12 }, { 0x040d, 0x00 },	/* DIG_CROP_W = 4608 */
	{ 0x040e, 0x0a }, { 0x040f, 0x20 },	/* DIG_CROP_H = 2592 */
	{ 0x034c, 0x12 }, { 0x034d, 0x00 },	/* X_OUTPUT = 4608 */
	{ 0x034e, 0x0a }, { 0x034f, 0x20 },	/* Y_OUTPUT = 2592 */
	{ 0x0301, 0x05 }, { 0x0303, 0x02 },	/* PLL */
	{ 0x0305, 0x02 }, { 0x0306, 0x00 },
	{ 0x0307, 0x7c }, { 0x030b, 0x02 },
	{ 0x030d, 0x04 }, { 0x030e, 0x01 },
	{ 0x030f, 0x2c }, { 0x0310, 0x01 },
	{ 0x3ca0, 0x00 }, { 0x3ca1, 0x64 },
	{ 0x3ca4, 0x00 }, { 0x3ca5, 0x00 },
	{ 0x3ca6, 0x00 }, { 0x3ca7, 0x00 },
	{ 0x3caa, 0x00 }, { 0x3cab, 0x00 },
	{ 0x3cb8, 0x00 }, { 0x3cb9, 0x08 },
	{ 0x3cba, 0x00 }, { 0x3cbb, 0x00 },
	{ 0x3cbc, 0x00 }, { 0x3cbd, 0x3c },
	{ 0x3cbe, 0x00 }, { 0x3cbf, 0x00 },
	{ 0x0202, 0x0a }, { 0x0203, 0x29 },	/* Exposure = 0xa29 */
	{ 0x0224, 0x01 }, { 0x0225, 0xf4 },	/* Short exposure */
	{ 0x3116, 0x01 }, { 0x3117, 0xf4 },	/* Mid exposure */
	{ 0x0204, 0x00 }, { 0x0205, 0x00 },	/* Analog gain = 0 */
	{ 0x0216, 0x00 }, { 0x0217, 0x00 },
	{ 0x0218, 0x01 }, { 0x0219, 0x00 },
	{ 0x020e, 0x01 }, { 0x020f, 0x00 },	/* Digital gain = 0x100 */
	{ 0x3118, 0x00 }, { 0x3119, 0x00 },
	{ 0x311a, 0x01 }, { 0x311b, 0x00 },
	{ 0x341a, 0x00 }, { 0x341b, 0x00 },
	{ 0x341c, 0x00 }, { 0x341d, 0x00 },
	{ 0x341e, 0x01 }, { 0x341f, 0x20 },
	{ 0x3420, 0x00 }, { 0x3421, 0xd8 },
	{ 0xc428, 0x00 }, { 0xc429, 0x04 },	/* LPF intensity */
	{ 0x3366, 0x00 }, { 0x3367, 0x00 },
	{ 0x3368, 0x00 }, { 0x3369, 0x00 },
};

/* 2304 × 1296, 2x2 binned, 10-bit */
static const struct reg_sequence imx708_mode_2x2binned_regs[] = {
	{ 0x0342, 0x1e }, { 0x0343, 0x90 },	/* LINE_LENGTH = 7824 */
	{ 0x0340, 0x05 }, { 0x0341, 0x38 },	/* FRAME_LENGTH = 1336 */
	{ 0x0344, 0x00 }, { 0x0345, 0x00 },
	{ 0x0346, 0x00 }, { 0x0347, 0x00 },
	{ 0x0348, 0x11 }, { 0x0349, 0xff },
	{ 0x034a, 0x0a }, { 0x034b, 0x1f },
	{ 0x0220, 0x62 }, { 0x0222, 0x01 },
	{ 0x0900, 0x01 }, { 0x0901, 0x22 },	/* 2x2 binning */
	{ 0x0902, 0x08 },
	{ 0x3200, 0x41 }, { 0x3201, 0x41 },
	{ 0x32d5, 0x00 }, { 0x32d6, 0x00 },
	{ 0x32db, 0x01 }, { 0x32df, 0x00 },
	{ 0x350c, 0x00 }, { 0x350d, 0x00 },
	{ 0x0408, 0x00 }, { 0x0409, 0x00 },
	{ 0x040a, 0x00 }, { 0x040b, 0x00 },
	{ 0x040c, 0x09 }, { 0x040d, 0x00 },	/* DIG_CROP_W = 2304 */
	{ 0x040e, 0x05 }, { 0x040f, 0x10 },	/* DIG_CROP_H = 1296 */
	{ 0x034c, 0x09 }, { 0x034d, 0x00 },	/* X_OUTPUT = 2304 */
	{ 0x034e, 0x05 }, { 0x034f, 0x10 },	/* Y_OUTPUT = 1296 */
	{ 0x0301, 0x05 }, { 0x0303, 0x02 },
	{ 0x0305, 0x02 }, { 0x0306, 0x00 },
	{ 0x0307, 0x7a }, { 0x030b, 0x02 },
	{ 0x030d, 0x04 }, { 0x030e, 0x01 },
	{ 0x030f, 0x2c }, { 0x0310, 0x01 },
	{ 0x3ca0, 0x00 }, { 0x3ca1, 0x3c },
	{ 0x3ca4, 0x00 }, { 0x3ca5, 0x3c },
	{ 0x3ca6, 0x00 }, { 0x3ca7, 0x00 },
	{ 0x3caa, 0x00 }, { 0x3cab, 0x00 },
	{ 0x3cb8, 0x00 }, { 0x3cb9, 0x1c },
	{ 0x3cba, 0x00 }, { 0x3cbb, 0x08 },
	{ 0x3cbc, 0x00 }, { 0x3cbd, 0x1e },
	{ 0x3cbe, 0x00 }, { 0x3cbf, 0x0a },
	{ 0x0202, 0x05 }, { 0x0203, 0x08 },
	{ 0x0224, 0x01 }, { 0x0225, 0xf4 },
	{ 0x3116, 0x01 }, { 0x3117, 0xf4 },
	{ 0x0204, 0x00 }, { 0x0205, 0x70 },
	{ 0x0216, 0x00 }, { 0x0217, 0x70 },
	{ 0x0218, 0x01 }, { 0x0219, 0x00 },
	{ 0x020e, 0x01 }, { 0x020f, 0x00 },
	{ 0x3118, 0x00 }, { 0x3119, 0x70 },
	{ 0x311a, 0x01 }, { 0x311b, 0x00 },
	{ 0x341a, 0x00 }, { 0x341b, 0x00 },
	{ 0x341c, 0x00 }, { 0x341d, 0x00 },
	{ 0x341e, 0x00 }, { 0x341f, 0x90 },
	{ 0x3420, 0x00 }, { 0x3421, 0x6c },
	{ 0x3366, 0x00 }, { 0x3367, 0x00 },
	{ 0x3368, 0x00 }, { 0x3369, 0x00 },
};

/* 1536 × 864, 2x2 binned + cropped (720p), 10-bit */
static const struct reg_sequence imx708_mode_2x2binned_720p_regs[] = {
	{ 0x0342, 0x14 }, { 0x0343, 0x60 },	/* LINE_LENGTH = 5216 */
	{ 0x0340, 0x04 }, { 0x0341, 0xb6 },	/* FRAME_LENGTH = 1206 */
	{ 0x0344, 0x03 }, { 0x0345, 0x00 },	/* X_ADD_STA = 768 */
	{ 0x0346, 0x01 }, { 0x0347, 0xb0 },	/* Y_ADD_STA = 432 */
	{ 0x0348, 0x0e }, { 0x0349, 0xff },	/* X_ADD_END = 3839 */
	{ 0x034a, 0x08 }, { 0x034b, 0x6f },	/* Y_ADD_END = 2159 */
	{ 0x0220, 0x62 }, { 0x0222, 0x01 },
	{ 0x0900, 0x01 }, { 0x0901, 0x22 },
	{ 0x0902, 0x08 },
	{ 0x3200, 0x41 }, { 0x3201, 0x41 },
	{ 0x32d5, 0x00 }, { 0x32d6, 0x00 },
	{ 0x32db, 0x01 }, { 0x32df, 0x01 },
	{ 0x350c, 0x00 }, { 0x350d, 0x00 },
	{ 0x0408, 0x00 }, { 0x0409, 0x00 },
	{ 0x040a, 0x00 }, { 0x040b, 0x00 },
	{ 0x040c, 0x06 }, { 0x040d, 0x00 },	/* DIG_CROP_W = 1536 */
	{ 0x040e, 0x03 }, { 0x040f, 0x60 },	/* DIG_CROP_H = 864 */
	{ 0x034c, 0x06 }, { 0x034d, 0x00 },	/* X_OUTPUT = 1536 */
	{ 0x034e, 0x03 }, { 0x034f, 0x60 },	/* Y_OUTPUT = 864 */
	{ 0x0301, 0x05 }, { 0x0303, 0x02 },
	{ 0x0305, 0x02 }, { 0x0306, 0x00 },
	{ 0x0307, 0x76 }, { 0x030b, 0x02 },
	{ 0x030d, 0x04 }, { 0x030e, 0x01 },
	{ 0x030f, 0x2c }, { 0x0310, 0x01 },
	{ 0x3ca0, 0x00 }, { 0x3ca1, 0x3c },
	{ 0x3ca4, 0x01 }, { 0x3ca5, 0x5e },
	{ 0x3ca6, 0x00 }, { 0x3ca7, 0x00 },
	{ 0x3caa, 0x00 }, { 0x3cab, 0x00 },
	{ 0x3cb8, 0x00 }, { 0x3cb9, 0x0c },
	{ 0x3cba, 0x00 }, { 0x3cbb, 0x04 },
	{ 0x3cbc, 0x00 }, { 0x3cbd, 0x1e },
	{ 0x3cbe, 0x00 }, { 0x3cbf, 0x05 },
	{ 0x0202, 0x04 }, { 0x0203, 0x86 },
	{ 0x0224, 0x01 }, { 0x0225, 0xf4 },
	{ 0x3116, 0x01 }, { 0x3117, 0xf4 },
	{ 0x0204, 0x00 }, { 0x0205, 0x70 },
	{ 0x0216, 0x00 }, { 0x0217, 0x70 },
	{ 0x0218, 0x01 }, { 0x0219, 0x00 },
	{ 0x020e, 0x01 }, { 0x020f, 0x00 },
	{ 0x3118, 0x00 }, { 0x3119, 0x70 },
	{ 0x311a, 0x01 }, { 0x311b, 0x00 },
	{ 0x341a, 0x00 }, { 0x341b, 0x00 },
	{ 0x341c, 0x00 }, { 0x341d, 0x00 },
	{ 0x341e, 0x00 }, { 0x341f, 0x60 },
	{ 0x3420, 0x00 }, { 0x3421, 0x48 },
	{ 0x3366, 0x00 }, { 0x3367, 0x00 },
	{ 0x3368, 0x00 }, { 0x3369, 0x00 },
};

/* 2304 × 1296 HDR mode, 10-bit */
static const struct reg_sequence imx708_mode_hdr_regs[] = {
	{ 0x0342, 0x14 }, { 0x0343, 0x60 },	/* LINE_LENGTH = 5216 */
	{ 0x0340, 0x0a }, { 0x0341, 0x5b },	/* FRAME_LENGTH = 2651 */
	{ 0x0344, 0x00 }, { 0x0345, 0x00 },
	{ 0x0346, 0x00 }, { 0x0347, 0x00 },
	{ 0x0348, 0x11 }, { 0x0349, 0xff },
	{ 0x034a, 0x0a }, { 0x034b, 0x1f },
	{ 0x0220, 0x01 }, { 0x0222, IMX708_HDR_EXPOSURE_RATIO },
	{ 0x0900, 0x00 }, { 0x0901, 0x11 },
	{ 0x0902, 0x0a },
	{ 0x3200, 0x01 }, { 0x3201, 0x01 },
	{ 0x32d5, 0x00 }, { 0x32d6, 0x00 },
	{ 0x32db, 0x01 }, { 0x32df, 0x00 },
	{ 0x350c, 0x00 }, { 0x350d, 0x00 },
	{ 0x0408, 0x00 }, { 0x0409, 0x00 },
	{ 0x040a, 0x00 }, { 0x040b, 0x00 },
	{ 0x040c, 0x09 }, { 0x040d, 0x00 },
	{ 0x040e, 0x05 }, { 0x040f, 0x10 },
	{ 0x034c, 0x09 }, { 0x034d, 0x00 },
	{ 0x034e, 0x05 }, { 0x034f, 0x10 },
	{ 0x0301, 0x05 }, { 0x0303, 0x02 },
	{ 0x0305, 0x02 }, { 0x0306, 0x00 },
	{ 0x0307, 0xa2 }, { 0x030b, 0x02 },
	{ 0x030d, 0x04 }, { 0x030e, 0x01 },
	{ 0x030f, 0x2c }, { 0x0310, 0x01 },
	{ 0x3ca0, 0x00 }, { 0x3ca1, 0x00 },
	{ 0x3ca4, 0x00 }, { 0x3ca5, 0x00 },
	{ 0x3ca6, 0x00 }, { 0x3ca7, 0x28 },
	{ 0x3caa, 0x00 }, { 0x3cab, 0x00 },
	{ 0x3cb8, 0x00 }, { 0x3cb9, 0x30 },
	{ 0x3cba, 0x00 }, { 0x3cbb, 0x00 },
	{ 0x3cbc, 0x00 }, { 0x3cbd, 0x32 },
	{ 0x3cbe, 0x00 }, { 0x3cbf, 0x00 },
	{ 0x0202, 0x0a }, { 0x0203, 0x2b },
	{ 0x0224, 0x0a }, { 0x0225, 0x2b },
	{ 0x3116, 0x0a }, { 0x3117, 0x2b },
	{ 0x0204, 0x00 }, { 0x0205, 0x00 },
	{ 0x0216, 0x00 }, { 0x0217, 0x00 },
	{ 0x0218, 0x01 }, { 0x0219, 0x00 },
	{ 0x020e, 0x01 }, { 0x020f, 0x00 },
	{ 0x3118, 0x00 }, { 0x3119, 0x00 },
	{ 0x311a, 0x01 }, { 0x311b, 0x00 },
	{ 0x341a, 0x00 }, { 0x341b, 0x00 },
	{ 0x341c, 0x00 }, { 0x341d, 0x00 },
	{ 0x341e, 0x00 }, { 0x341f, 0x90 },
	{ 0x3420, 0x00 }, { 0x3421, 0x6c },
	{ 0x3360, 0x01 }, { 0x3361, 0x01 },
	{ 0x3366, 0x09 }, { 0x3367, 0x00 },
	{ 0x3368, 0x05 }, { 0x3369, 0x10 },
};

/* ------------------------------------------------------------------ */
/* Mode tables                                                         */
/* ------------------------------------------------------------------ */

static const struct imx708_mode imx708_modes_no_hdr[] = {
	{
		/* 4608 × 2592 full resolution @ 14.35fps */
		.width		= 4608,
		.height		= 2592,
		.code		= MEDIA_BUS_FMT_SRGGB10_1X10,
		.fps		= 14,
		.hblank		= IMX708_LINE_LENGTH_FULL - 4608,
		.vblank		= 58,
		.pixel_rate	= IMX708_PIXEL_RATE_FULL,
		.line_length	= IMX708_LINE_LENGTH_FULL,
		.reg_list	= imx708_mode_4608x2592_regs,
		.num_regs	= ARRAY_SIZE(imx708_mode_4608x2592_regs),
	},
	{
		/* 2304 × 1296 2x2 binned @ 56fps */
		.width		= 2304,
		.height		= 1296,
		.code		= MEDIA_BUS_FMT_SRGGB10_1X10,
		.fps		= 56,
		.hblank		= IMX708_LINE_LENGTH_BINNED - 2304,
		.vblank		= 40,
		.pixel_rate	= IMX708_PIXEL_RATE_BINNED,
		.line_length	= IMX708_LINE_LENGTH_BINNED,
		.reg_list	= imx708_mode_2x2binned_regs,
		.num_regs	= ARRAY_SIZE(imx708_mode_2x2binned_regs),
	},
	{
		/* 1536 × 864 2x2 binned + cropped @ 120fps */
		.width		= 1536,
		.height		= 864,
		.code		= MEDIA_BUS_FMT_SRGGB10_1X10,
		.fps		= 120,
		.hblank		= IMX708_LINE_LENGTH_720P - 1536,
		.vblank		= 40,
		.pixel_rate	= IMX708_PIXEL_RATE_720P,
		.line_length	= IMX708_LINE_LENGTH_720P,
		.reg_list	= imx708_mode_2x2binned_720p_regs,
		.num_regs	= ARRAY_SIZE(imx708_mode_2x2binned_720p_regs),
	},
};

static const struct imx708_mode imx708_modes_hdr[] = {
	{
		/* 2304 × 1296 HDR mode @ 30fps */
		.width		= 2304,
		.height		= 1296,
		.code		= MEDIA_BUS_FMT_SRGGB10_1X10,
		.fps		= 30,
		.hblank		= IMX708_LINE_LENGTH_720P - 2304,
		.vblank		= 3673,
		.pixel_rate	= IMX708_PIXEL_RATE_HDR,
		.line_length	= IMX708_LINE_LENGTH_720P,
		.reg_list	= imx708_mode_hdr_regs,
		.num_regs	= ARRAY_SIZE(imx708_mode_hdr_regs),
	},
};

/* ------------------------------------------------------------------ */
/* Hardware operations                                                  */
/* ------------------------------------------------------------------ */

static int imx708_hw_init(struct imx708_dev *sensor)
{
	int ret;

	/* Write common register sequence */
	ret = regmap_multi_reg_write(sensor->regmap,
				     imx708_common_regs,
				     ARRAY_SIZE(imx708_common_regs));
	if (ret)
		return ret;

	/* Program PDAF correction gains */
	{
		u32 val;
		int i;

		ret = regmap_read(sensor->regmap,
				  IMX708_REG_BASE_SPC_GAINS_L, &val);
		if (ret)
			return ret;

		if (val == 0x40) {
			for (i = 0; i < 54 && ret == 0; i++) {
				ret = regmap_write(sensor->regmap,
					IMX708_REG_BASE_SPC_GAINS_L + i,
					imx708_pdaf_gains_left[i % 9]);
			}
			for (i = 0; i < 54 && ret == 0; i++) {
				ret = regmap_write(sensor->regmap,
					IMX708_REG_BASE_SPC_GAINS_R + i,
					imx708_pdaf_gains_right[i % 9]);
			}
		}
		if (ret)
			return ret;
	}

	dev_dbg(sensor->dev, "sensor initialized\n");
	return 0;
}

static void imx708_hw_deinit(struct imx708_dev *sensor)
{
	/* Put in standby */
	regmap_write(sensor->regmap, IMX708_REG_MODE_SELECT,
		     IMX708_MODE_STANDBY);
}

static int imx708_hw_power_on(struct imx708_dev *sensor)
{
	int ret;

	/* Take out of standby into streaming mode */
	ret = regmap_write(sensor->regmap, IMX708_REG_MODE_SELECT,
			   IMX708_MODE_STREAMING);
	if (ret)
		return ret;

	usleep_range(10000, 15000);

	dev_dbg(sensor->dev, "streaming enabled\n");
	return 0;
}

static int imx708_hw_power_off(struct imx708_dev *sensor)
{
	int ret;

	ret = regmap_write(sensor->regmap, IMX708_REG_MODE_SELECT,
			   IMX708_MODE_STANDBY);
	if (ret)
		return ret;

	usleep_range(5000, 10000);

	dev_dbg(sensor->dev, "standby\n");
	return 0;
}

static int imx708_hw_set_mode(struct imx708_dev *sensor,
			       const struct v4l2_mbus_framefmt *fmt)
{
	const struct imx708_mode *mode = NULL;
	int i, ret;

	/* Find matching mode */
	for (i = 0; i < sensor->soc->num_modes; i++) {
		if (sensor->soc->modes[i].width == fmt->width &&
		    sensor->soc->modes[i].height == fmt->height &&
		    sensor->soc->modes[i].code == fmt->code) {
			mode = &sensor->soc->modes[i];
			break;
		}
	}

	if (!mode) {
		dev_err(sensor->dev, "unsupported mode %ux%u code 0x%x\n",
			fmt->width, fmt->height, fmt->code);
		return -EINVAL;
	}

	/* Apply mode register sequence */
	ret = regmap_multi_reg_write(sensor->regmap,
				     mode->reg_list, mode->num_regs);
	if (ret) {
		dev_err(sensor->dev, "failed to apply mode config: %d\n", ret);
		return ret;
	}

	sensor->mode = mode;
	dev_dbg(sensor->dev, "mode set to %ux%u @ %ufps\n",
		mode->width, mode->height, mode->fps);

	return 0;
}

static int imx708_hw_set_gain(struct imx708_dev *sensor, u32 gain)
{
	int ret;

	if (gain > IMX708_ANA_GAIN_MAX)
		return -EINVAL;

	ret = imx708_write_reg16(sensor, IMX708_REG_ANALOG_GAIN, gain);
	if (ret)
		return ret;

	dev_dbg(sensor->dev, "gain set to 0x%04x\n", gain);
	return 0;
}

static int imx708_hw_set_exposure(struct imx708_dev *sensor, u32 exposure)
{
	int ret;

	if (exposure < IMX708_EXPOSURE_MIN || exposure > IMX708_EXPOSURE_MAX)
		return -EINVAL;

	ret = imx708_write_reg16(sensor, IMX708_REG_EXPOSURE, exposure);
	if (ret)
		return ret;

	dev_dbg(sensor->dev, "exposure set to %u\n", exposure);
	return 0;
}

static int imx708_hw_set_digital_gain(struct imx708_dev *sensor, u32 dgain)
{
	int ret;

	if (dgain < IMX708_DGTL_GAIN_MIN || dgain > IMX708_DGTL_GAIN_MAX)
		return -EINVAL;

	ret = imx708_write_reg16(sensor, IMX708_REG_DIGITAL_GAIN, dgain);
	if (ret)
		return ret;

	dev_dbg(sensor->dev, "digital gain set to 0x%04x\n", dgain);
	return 0;
}

/*
 * Switch between the standard and the 2-exposure HDR register sets.
 *
 * The HDR mode table reprograms the full timing/binning block, so it must
 * only be applied while the sensor is out of streaming state; callers hold
 * sensor->lock and reject the request when streaming is active.
 */
static int imx708_hw_set_hdr(struct imx708_dev *sensor, u32 mode, u32 ratio)
{
	int ret;

	if (mode > IMX708_HDR_MODE_MAX)
		return -EINVAL;

	if (!mode) {
		/* Restore the non-HDR defaults from the common sequence. */
		ret = regmap_multi_reg_write(sensor->regmap, imx708_common_regs,
					     ARRAY_SIZE(imx708_common_regs));
		if (ret)
			return ret;

		if (sensor->mode)
			ret = regmap_multi_reg_write(sensor->regmap,
						     sensor->mode->reg_list,
						     sensor->mode->num_regs);
		if (ret)
			return ret;

		sensor->hdr_enabled = false;
		sensor->hdr_ratio = 0;
		dev_dbg(sensor->dev, "HDR disabled\n");
		return 0;
	}

	ret = regmap_multi_reg_write(sensor->regmap, imx708_mode_hdr_regs,
				     ARRAY_SIZE(imx708_mode_hdr_regs));
	if (ret)
		return ret;

	if (ratio) {
		ret = regmap_write(sensor->regmap, IMX708_REG_HDR_RATIO,
				   ratio & 0xff);
		if (ret)
			return ret;
	}

	sensor->hdr_enabled = true;
	sensor->hdr_ratio = ratio;
	dev_dbg(sensor->dev, "HDR enabled (mode %u, ratio %u)\n", mode, ratio);

	return 0;
}

static u32 imx708_hw_irq_ack(struct imx708_dev *sensor)
{
	u32 status;
	int ret;

	ret = imx708_read_reg16(sensor, IMX708_REG_INTERRUPT_STATUS, &status);
	if (ret) {
		dev_err_ratelimited(sensor->dev,
				    "failed to read interrupt status: %d\n",
				    ret);
		return 0;
	}

	if (!status)
		return 0;

	/* Clear all pending interrupts */
	imx708_write_reg16(sensor, IMX708_REG_INTERRUPT_CLEAR, status);

	return status;
}

/* ------------------------------------------------------------------ */
/* Ops tables                                                          */
/* ------------------------------------------------------------------ */

static const struct imx708_hw_ops imx708_hw_ops_rpi = {
	.init			= imx708_hw_init,
	.deinit			= imx708_hw_deinit,
	.power_on		= imx708_hw_power_on,
	.power_off		= imx708_hw_power_off,
	.set_mode		= imx708_hw_set_mode,
	.set_gain		= imx708_hw_set_gain,
	.set_exposure		= imx708_hw_set_exposure,
	.set_digital_gain	= imx708_hw_set_digital_gain,
	.set_hdr		= imx708_hw_set_hdr,
	.irq_ack		= imx708_hw_irq_ack,
	.quirk_fixup		= NULL,
};

/* ------------------------------------------------------------------ */
/* Regmap configuration                                                */
/* ------------------------------------------------------------------ */

/*
 * The IMX708 uses 16-bit register addresses and 8-bit data. Using
 * .val_bits = 16 makes regmap emit two data bytes per transfer, which
 * writes the wrong value and desynchronises every subsequent access.
 *
 * Caching is disabled: several registers (status, temperature, frame
 * counters, interrupt status) change without the driver writing them, and
 * the sensor is fully reprogrammed on every runtime-PM resume anyway.
 */
static const struct regmap_config imx708_regmap_16b = {
	.reg_bits	= 16,
	.val_bits	= 8,
	.reg_stride	= 1,
	.max_register	= 0xFFFF,
	.cache_type	= REGCACHE_NONE,
};

/* ------------------------------------------------------------------ */
/* Clock and GPIO names                                                */
/* ------------------------------------------------------------------ */

static const char * const imx708_clocks_rpi[] = {
	"inclk",	/* 24 MHz input clock */
	NULL,
};

static const char * const imx708_gpios_rpi[] = {
	"reset",
	NULL,
};

/* ------------------------------------------------------------------ */
/* SoC data instances                                                  */
/* ------------------------------------------------------------------ */

const struct imx708_soc_data imx708_soc_rpi = {
	.name		= "Sony IMX708 (Raspberry Pi Camera Module 3)",
	.ops		= &imx708_hw_ops_rpi,
	.regmap_cfg	= &imx708_regmap_16b,
	.reg		= NULL,		/* using direct register defines */
	.modes		= imx708_modes_no_hdr,
	.num_modes	= ARRAY_SIZE(imx708_modes_no_hdr),
	.num_channels	= 2,		/* 2-lane MIPI CSI-2 (default) */
	.clk_names	= imx708_clocks_rpi,
	.gpio_names	= imx708_gpios_rpi,
	.i2c_addr	= 0x1a,		/* fixed I2C address */
	.quirks		= 0,
};

const struct imx708_soc_data imx708_soc_rpi_wide = {
	.name		= "Sony IMX708 (Raspberry Pi Camera Module 3 Wide)",
	.ops		= &imx708_hw_ops_rpi,
	.regmap_cfg	= &imx708_regmap_16b,
	.reg		= NULL,
	.modes		= imx708_modes_no_hdr,
	.num_modes	= ARRAY_SIZE(imx708_modes_no_hdr),
	.num_channels	= 2,
	.clk_names	= imx708_clocks_rpi,
	.gpio_names	= imx708_gpios_rpi,
	.i2c_addr	= 0x1a,
	.quirks		= 0,
};
