/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * imx708_regs.h - Register offsets and field masks for Sony IMX708 sensor
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * Register map for the Sony IMX708 11.9MP CMOS image sensor.
 * Verified against the upstream Raspberry Pi kernel driver (rpi-6.6.y)
 * and the Sony IMX708-AAJH5-C datasheet.
 *
 * Registers are addressed with a 16-bit big-endian register address and
 * 8-bit data. Fields wider than 8 bits (marked "16-bit" below) occupy two
 * consecutive addresses, most significant byte first; access them with
 * imx708_read_reg16()/imx708_write_reg16() rather than plain regmap calls.
 * I2C address: 0x1a (fixed)
 * Input clock: 24 MHz
 */

#ifndef _IMX708_REGS_H_
#define _IMX708_REGS_H_

#include <linux/bitfield.h>
#include <linux/bits.h>
#include "imx708_platform.h"

/*
 * Register address map — verified from upstream kernel driver
 * All register addresses are 16-bit.
 */

/* Chip ID (0x0016) — read-only, returns 0x0708 */
#define IMX708_REG_CHIP_ID 0x0016
#define IMX708_CHIP_ID 0x0708

/* Module ID (0x0000) — bit 1=wide, bit 7=noir */
#define IMX708_REG_MODULE_ID 0x0000

/* System control */
#define IMX708_REG_MODE_SELECT 0x0100 /* 0x00=standby, 0x01=streaming */
#define IMX708_REG_ORIENTATION 0x0101 /* bit0=hflip, bit1=vflip */
#define IMX708_REG_DATA_FORMAT 0x0112 /* 0x0a=RAW10 */
#define IMX708_REG_DATA_FORMAT2 0x0113
#define IMX708_REG_MIPI_LANES 0x0114  /* 0x01=2-lane, 0x03=4-lane */
#define IMX708_REG_TEMPERATURE 0x013a /* signed 8-bit, -20 to +80 C */
#define IMX708_REG_DEVICE_CONFIG 0x0136
#define IMX708_REG_DEVICE_CONFIG2 0x0137
#define IMX708_REG_SYNC_CONTROL 0x0138

/* PLL / Clock tree */
#define IMX708_REG_IVT_PXCK_DIV 0x0301
#define IMX708_REG_IVT_SYCK_DIV 0x0303
#define IMX708_REG_IVT_PREPLLCK_DIV 0x0305
#define IMX708_REG_IVT_PLL_MPY 0x0306 /* 16-bit */
#define IMX708_REG_IOP_SYCK_DIV 0x030b
#define IMX708_REG_IOP_PREPLLCK_DIV 0x030d
#define IMX708_REG_IOP_PLL_MPY 0x030e /* 16-bit, link frequency */
#define IMX708_REG_PLL_MULT_DRIV 0x0310

/* Timing — vertical */
#define IMX708_REG_FRAME_LENGTH 0x0340 /* 16-bit, max 0xffff */
#define IMX708_FRAME_LENGTH_MAX 0xffff
#define IMX708_REG_LINE_LENGTH 0x0342  /* 16-bit */
#define IMX708_LINE_LENGTH_FULL 15648  /* 0x3d20 — full res */
#define IMX708_LINE_LENGTH_BINNED 7824 /* 0x1e90 — 2x2 binned */
#define IMX708_LINE_LENGTH_720P 5216   /* 0x1460 — 720p binned */
#define IMX708_VBLANK_MIN 58

/* Imaging area — analogue crop */
#define IMX708_REG_X_ADD_STA 0x0344		/* 16-bit */
#define IMX708_REG_Y_ADD_STA 0x0346		/* 16-bit */
#define IMX708_REG_X_ADD_END 0x0348		/* 16-bit */
#define IMX708_REG_Y_ADD_END 0x034a		/* 16-bit */
#define IMX708_REG_X_OUTPUT_SIZE 0x034c /* 16-bit */
#define IMX708_REG_Y_OUTPUT_SIZE 0x034e /* 16-bit */

/* Digital crop */
#define IMX708_REG_DIG_CROP_X_OFFSET 0x0408 /* 16-bit */
#define IMX708_REG_DIG_CROP_Y_OFFSET 0x040a /* 16-bit */
#define IMX708_REG_DIG_CROP_WIDTH 0x040c	/* 16-bit */
#define IMX708_REG_DIG_CROP_HEIGHT 0x040e	/* 16-bit */

/* Binning */
#define IMX708_REG_BINNING_MODE 0x0900
#define IMX708_REG_BINNING_TYPE 0x0901
#define IMX708_REG_BINNING_WEIGHT 0x0902
#define IMX708_REG_BINNING_PRIORITY_H 0x3200
#define IMX708_REG_BINNING_PRIORITY_V 0x3201

/* Exposure */
#define IMX708_REG_EXPOSURE 0x0202 /* 16-bit */
#define IMX708_EXPOSURE_OFFSET 48
#define IMX708_EXPOSURE_DEFAULT 0x640
#define IMX708_EXPOSURE_MIN 8
#define IMX708_EXPOSURE_MAX (IMX708_FRAME_LENGTH_MAX - IMX708_EXPOSURE_OFFSET)
#define IMX708_EXPOSURE_STEP 1

/* Analog gain */
#define IMX708_REG_ANALOG_GAIN 0x0204 /* 16-bit */
#define IMX708_ANA_GAIN_MIN 0
#define IMX708_ANA_GAIN_MAX 960
#define IMX708_ANA_GAIN_STEP 1
#define IMX708_ANA_GAIN_DEFAULT IMX708_ANA_GAIN_MIN

/* Gain aliases for V4L2 control init */
#define IMX708_GAIN_MIN IMX708_ANA_GAIN_MIN
#define IMX708_GAIN_MAX IMX708_ANA_GAIN_MAX
#define IMX708_GAIN_STEP IMX708_ANA_GAIN_STEP
#define IMX708_GAIN_DEFAULT IMX708_ANA_GAIN_DEFAULT

/* Digital gain */
#define IMX708_REG_DIGITAL_GAIN 0x020e /* 16-bit */
#define IMX708_DGTL_GAIN_MIN 0x0100
#define IMX708_DGTL_GAIN_MAX 0xffff
#define IMX708_DGTL_GAIN_DEFAULT 0x0100
#define IMX708_DGTL_GAIN_STEP 1

/* HDR exposure/gain — short and middle */
#define IMX708_HDR_EXPOSURE_RATIO 4
/*
 * Highest HDR mode the driver can program. Only mode 1 (2-exposure,
 * line-interleaved) has a register table; DOL is not implemented.
 */
#define IMX708_HDR_MODE_MAX 1
#define IMX708_REG_SHT_EXPOSURE 0x0224	   /* 16-bit */
#define IMX708_REG_MID_EXPOSURE 0x3116	   /* 16-bit */
#define IMX708_REG_SHT_ANALOG_GAIN 0x0216  /* 16-bit */
#define IMX708_REG_SHT_DIGITAL_GAIN 0x0218 /* 16-bit */
#define IMX708_REG_MID_ANALOG_GAIN 0x3118  /* 16-bit */
#define IMX708_REG_MID_DIGITAL_GAIN 0x311a /* 16-bit */

/* Long exposure shift */
#define IMX708_LONG_EXP_SHIFT_MAX 7
#define IMX708_LONG_EXP_SHIFT_REG 0x3100

/* Test Pattern Control */
#define IMX708_REG_TEST_PATTERN 0x0600 /* 16-bit */
#define IMX708_TEST_PATTERN_DISABLE 0
#define IMX708_TEST_PATTERN_SOLID_COLOR 1
#define IMX708_TEST_PATTERN_COLOR_BARS 2
#define IMX708_TEST_PATTERN_GREY_COLOR 3
#define IMX708_TEST_PATTERN_PN9 4
#define IMX708_TEST_PATTERN_MAX IMX708_TEST_PATTERN_PN9

/* Test pattern colour components */
#define IMX708_REG_TEST_PATTERN_R 0x0602  /* 16-bit */
#define IMX708_REG_TEST_PATTERN_GR 0x0604 /* 16-bit */
#define IMX708_REG_TEST_PATTERN_B 0x0606  /* 16-bit */
#define IMX708_REG_TEST_PATTERN_GB 0x0608 /* 16-bit */
#define IMX708_TEST_PATTERN_COLOUR_MIN 0
#define IMX708_TEST_PATTERN_COLOUR_MAX 0x0fff
#define IMX708_TEST_PATTERN_COLOUR_STEP 1

/* Colour balance */
#define IMX708_REG_COLOUR_BALANCE_RED 0x0b90  /* 16-bit */
#define IMX708_REG_COLOUR_BALANCE_BLUE 0x0b92 /* 16-bit */
#define IMX708_COLOUR_BALANCE_MIN 0x01
#define IMX708_COLOUR_BALANCE_MAX 0xffff
#define IMX708_COLOUR_BALANCE_STEP 0x01
#define IMX708_COLOUR_BALANCE_DEFAULT 0x100

/* PDAF correction gains */
#define IMX708_REG_BASE_SPC_GAINS_L 0x7b10
#define IMX708_REG_BASE_SPC_GAINS_R 0x7c00

/* QBC Re-mosaic correction */
#define IMX708_REG_QBC_RMSC_EN 0x32d5
#define IMX708_REG_LPF_INTENSITY_EN 0xc428
#define IMX708_LPF_INTENSITY_ENABLED 0x00
#define IMX708_LPF_INTENSITY_DISABLED 0x01
#define IMX708_REG_LPF_INTENSITY 0xc429
#define IMX708_LPF_INTENSITY_DEFAULT 2

/* AE Histogram */
#define IMX708_REG_AEHIST_AUTO_THRESH 0x3360  /* 16-bit */
#define IMX708_REG_AEHIST1_AREA_WIDTH 0x3366  /* 16-bit */
#define IMX708_REG_AEHIST1_AREA_HEIGHT 0x3368 /* 16-bit */

/* Clock lane blanking (non-continuous clock) */
#define IMX708_REG_CLKLANE_BLANK 0x3220
#define IMX708_CLKLANE_BLANK_NONCONT BIT(0)

/* Analogue crop enable */
#define IMX708_REG_ACROPLP_EN 0x32df

/* HDR mode control */
#define IMX708_REG_HDR_CTRL 0x0220
#define IMX708_REG_HDR_RATIO 0x0222

/*
 * Interrupt / status block.
 *
 * The IMX708 module used on the Raspberry Pi Camera Module 3 does not route
 * an interrupt line to the host, and these addresses are NOT documented in
 * the IMX708-AAJH5-C datasheet (0x3200/0x3201 are the binning-priority
 * registers and 0x3400 belongs to the QBC block). They are therefore only
 * touched when the device tree actually supplies an "interrupts" property
 * for a variant that implements them. Do not enable the IRQ path on stock
 * Camera Module 3 hardware.
 *
 * TODO(HW): replace with datasheet-verified offsets before enabling IRQs.
 */
#define IMX708_REG_INTERRUPT_ENABLE 0x3200
#define IMX708_REG_INTERRUPT_STATUS 0x3202
#define IMX708_REG_INTERRUPT_CLEAR 0x3204
#define IMX708_REG_STATUS 0x3400

/* Embedded data / metadata */
#define IMX708_EMBEDDED_LINE_WIDTH (5 * 5760)
#define IMX708_NUM_EMBEDDED_LINES 1

/* Native and active pixel array */
#define IMX708_NATIVE_WIDTH 4640U
#define IMX708_NATIVE_HEIGHT 2658U
#define IMX708_PIXEL_ARRAY_LEFT 16U
#define IMX708_PIXEL_ARRAY_TOP 24U
#define IMX708_PIXEL_ARRAY_WIDTH 4608U
#define IMX708_PIXEL_ARRAY_HEIGHT 2592U

/* Input clock frequency */
#define IMX708_INCLK_FREQ 24000000

/* Pixel rates per mode */
#define IMX708_PIXEL_RATE_FULL 595200000
#define IMX708_PIXEL_RATE_BINNED 585600000
#define IMX708_PIXEL_RATE_720P 566400000
#define IMX708_PIXEL_RATE_HDR 777600000

/* Link frequencies */
#define IMX708_LINK_FREQ_450MHZ 450000000
#define IMX708_LINK_FREQ_447MHZ 447000000
#define IMX708_LINK_FREQ_453MHZ 453000000

/* Field masks for orientation register */
#define IMX708_HFLIP_MASK BIT(0)
#define IMX708_VFLIP_MASK BIT(1)

/* Field masks for mode select */
#define IMX708_MODE_STANDBY 0x00
#define IMX708_MODE_STREAMING 0x01

/* Field masks for test pattern */
#define IMX708_TEST_PATTERN_MASK GENMASK(3, 0)

/* Field masks for binning */
#define IMX708_BINNING_MODE_MASK GENMASK(7, 0)
#define IMX708_BINNING_TYPE_MASK GENMASK(7, 0)
#define IMX708_BINNING_WEIGHT_MASK GENMASK(7, 0)

/* Interrupt masks */
#define IMX708_INT_FRAME_START BIT(0)
#define IMX708_INT_FRAME_END BIT(1)
#define IMX708_INT_FIFO_OVERFLOW BIT(2)
#define IMX708_INT_PLL_LOCK BIT(3)
#define IMX708_INT_PLL_UNLOCK BIT(4)
#define IMX708_INT_TEMPERATURE BIT(5)
#define IMX708_INT_ERROR BIT(6)
#define IMX708_INT_ALL GENMASK(7, 0)

/* Status masks */
#define IMX708_STATUS_STANDBY BIT(0)
#define IMX708_STATUS_STREAMING BIT(1)
#define IMX708_STATUS_PLL_LOCKED BIT(2)
#define IMX708_STATUS_ERROR BIT(3)

/* Power supply names (from upstream driver) */
#define IMX708_SUPPLY_VANA1 "vana1" /* 2.8V analog */
#define IMX708_SUPPLY_VANA2 "vana2" /* 1.8V analog */
#define IMX708_SUPPLY_VDIG "vdig"	/* 1.1V digital core */
#define IMX708_SUPPLY_VDDL "vddl"	/* 1.8V I/O */
#define IMX708_NUM_SUPPLIES 4

/* Initialisation delays */
#define IMX708_XCLR_MIN_DELAY_US 8000
#define IMX708_XCLR_DELAY_RANGE_US 1000

/* Highest valid register address (16-bit register addressing) */
#define IMX708_REG_ADDR_MAX 0xFFFF

/* PDAF grid dimensions */
#define IMX708_PDAF_ROWS 12
#define IMX708_PDAF_COLS 16

/* Default PDAF correction gains live in imx708_platform.c */
extern const u8 imx708_pdaf_gains_left[9];
extern const u8 imx708_pdaf_gains_right[9];

#endif /* _IMX708_REGS_H_ */
