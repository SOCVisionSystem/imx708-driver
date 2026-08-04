/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * imx708_platform.h - Hardware abstraction seam for IMX708 sensor
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * This header defines the platform abstraction interface for the IMX708
 * camera sensor driver. Every SoC-specific hardware detail lives behind
 * these ops and data structures. The core driver never contains a SoC name,
 * base address, or register offset — all of that is supplied by the platform
 * back-end in imx708_platform.c.
 *
 * To add a new platform:
 *   1. Define a new imx708_soc_data instance in imx708_platform.c
 *   2. Add an entry to imx708_of_match[] with the compatible string
 *   3. Implement the hw_ops callbacks
 *   4. No changes to any other file are needed.
 */

#ifndef _IMX708_PLATFORM_H_
#define _IMX708_PLATFORM_H_

#include <linux/types.h>
#include <linux/regmap.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/cdev.h>
#include <linux/dcache.h>
#include <linux/atomic.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

/* Forward declarations */
struct imx708_dev;
struct imx708_config;
struct imx708_irq_counters;
struct imx708_error_counters;

/**
 * struct imx708_hw_ops - Platform-specific hardware operations
 * @init:          One-time sensor bring-up: power, clocks, reset, default regs
 * @deinit:        Tear-down, exact inverse of @init
 * @power_on:      Transition sensor from standby to streaming-ready
 * @power_off:     Transition sensor to low-power standby
 * @set_mode:      Configure sensor for a given frame size, format, and fps
 * @set_gain:      Set analog gain (dB * 1000 or sensor-specific units)
 * @set_exposure:  Set exposure time (line units)
 * @set_digital_gain: Set digital gain (dB * 1000 or sensor-specific units)
 * @irq_ack:       Acknowledge and clear interrupt source, return event mask
 * @quirk_fixup:   Optional erratum workaround applied late in probe; may be NULL
 *
 * Every SoC back-end supplies one instance of this structure. The core driver
 * calls only through these pointers, so adding a new SoC is a new ops table
 * plus one entry in the match table — never a change to the core.
 */
struct imx708_hw_ops
{
	int (*init)(struct imx708_dev *sensor);
	void (*deinit)(struct imx708_dev *sensor);
	int (*power_on)(struct imx708_dev *sensor);
	int (*power_off)(struct imx708_dev *sensor);
	int (*set_mode)(struct imx708_dev *sensor,
					const struct v4l2_mbus_framefmt *fmt);
	int (*set_gain)(struct imx708_dev *sensor, u32 gain);
	int (*set_exposure)(struct imx708_dev *sensor, u32 exposure);
	int (*set_digital_gain)(struct imx708_dev *sensor, u32 dgain);
	int (*set_hdr)(struct imx708_dev *sensor, u32 mode, u32 ratio);
	u32 (*irq_ack)(struct imx708_dev *sensor);
	int (*quirk_fixup)(struct imx708_dev *sensor);
};

/**
 * struct imx708_mode - One sensor mode (resolution + format + frame rate)
 * @width:         Active pixel width
 * @height:        Active pixel height
 * @code:          Media bus code (MEDIA_BUS_FMT_*)
 * @fps:           Frames per second (numerator, denominator is 1)
 * @hblank:        Horizontal blanking (pixels)
 * @vblank:        Vertical blanking (lines)
 * @pixel_rate:    Pixel rate in Hz
 * @line_length:   Line length in pixels
 * @reg_list:      Register configuration for this mode
 * @num_regs:      Number of register writes in reg_list
 *
 * Each mode is a complete register configuration that puts the sensor into
 * a known resolution, format, and frame rate.
 */
struct imx708_mode
{
	unsigned int width;
	unsigned int height;
	u32 code;
	unsigned int fps;
	unsigned int hblank;
	unsigned int vblank;
	unsigned int pixel_rate;
	unsigned int line_length;
	const struct reg_sequence *reg_list;
	unsigned int num_regs;
};

/**
 * struct imx708_soc_data - Compile-time description of one SoC/variant
 * @name:          Human-readable identifier, used in log messages
 * @ops:           Hardware operation table for this variant
 * @regmap_cfg:    Regmap configuration: register stride, max register, cache
 * @reg:           Register offset table for this variant
 * @modes:         Array of supported sensor modes
 * @num_modes:     Number of entries in modes[]
 * @num_channels:  Number of MIPI CSI-2 data lanes
 * @clk_names:     NULL-terminated list of clocks the block requires
 * @gpio_names:    NULL-terminated list of GPIO names (reset, power, etc.)
 * @i2c_addr:      I2C slave address
 * @quirks:        Bitmask of IMX708_QUIRK_* flags
 */
struct imx708_soc_data
{
	const char *name;
	const struct imx708_hw_ops *ops;
	const struct regmap_config *regmap_cfg;
	const struct imx708_regs *reg;
	const struct imx708_mode *modes;
	unsigned int num_modes;
	unsigned int num_channels;
	const char *const *clk_names;
	const char *const *gpio_names;
	unsigned short i2c_addr;
	u32 quirks;
};

/**
 * struct imx708_dev - Driver state (one per sensor instance)
 * @sd:            V4L2 sub-device embedded struct
 * @pad:           Source pad for media bus
 * @ctrl_handler:  V4L2 control handler
 * @dev:           Pointer to the kernel device structure
 * @client:        I2C client (for register access)
 * @regmap:        Regmap for register I/O
 * @soc:           SoC/platform data for this instance
 * @lock:          Mutex protecting sensor state
 * @mdev:          Media device (if any)
 * @fmt:           Current active format
 * @mode:          Current active mode
 * @streaming:     Whether streaming is active
 * @power_count:   Runtime PM reference count
 * @pending_events: Latched interrupt events for threaded handler
 * @fault_attrs:   Fault injection debugfs attributes (if enabled)
 * @cdev:          Character device
 * @chardev_id:    Character device instance ID
 * @irq_counters:  Interrupt event counters
 * @error_counters: Error counters by type
 * @debugfs_dir:   Debugfs directory dentry
 *
 * Allocated with devm_kzalloc(), reachable from every entry point via
 * container_of() or dev_get_drvdata(). No file-scope globals for per-device
 * state.
 */
struct imx708_dev
{
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;
	const struct imx708_soc_data *soc;

	struct mutex lock; /* protects sensor state */

	struct v4l2_mbus_framefmt fmt;
	const struct imx708_mode *mode;
	bool streaming;
	int power_count;
	bool hdr_enabled;
	u32 hdr_ratio;

	/* Interrupt handling; written from hardirq context */
	atomic_t pending_events;

	/* V4L2 control state */
	u32 test_pattern;
	bool hflip;
	bool vflip;
	int brightness;
	int contrast;
	int saturation;
	int hue;
	int gamma;
	int sharpness;
	bool auto_wb;
	int wb_temp;
	int backlight_comp;
	int power_line_freq;
	int exp_bias;
	u32 a3a_lock;
	int scene_mode;
	int iso;
	int iso_auto;
	int colorfx;
	int zoom;
	int pan;
	int tilt;

	/* Regulator supplies */
	struct regulator *reg_dovdd; /* vana1 (2.8V analog) */
	struct regulator *reg_avdd;	 /* vana2 (1.8V analog) */
	struct regulator *reg_dvdd;	 /* vdig (1.1V digital core) */
	struct regulator *reg_vddl;	 /* vddl (1.8V I/O, optional) */

	/* GPIOs */
	struct gpio_desc *gpio_reset;
	struct gpio_desc *gpio_power;

#ifdef CONFIG_IMX708_FAULT_INJECT
	struct imx708_fault_state *fault;
#endif
	struct cdev cdev;
	unsigned int chardev_id;
	struct imx708_irq_counters *irq_counters;
	struct imx708_error_counters *error_counters;
	struct dentry *debugfs_dir;
};

/* Helper to get imx708_dev from v4l2_subdev */
#define to_imx708_dev(sd) container_of(sd, struct imx708_dev, sd)

/**
 * struct imx708_config - Sensor configuration (for set_config ioctl)
 * @gain:          Analog gain in sensor-specific units
 * @digital_gain:  Digital gain in sensor-specific units
 * @exposure:      Exposure time in line units
 * @hdr_mode:      HDR mode (0=off, 1=line-interleaved, 2=sensor-DOL)
 * @test_pattern:  Test pattern selection (0=off)
 * @binning:       Binning mode (1=no binning, 2=2x2, etc.)
 */
struct imx708_config
{
	u32 gain;
	u32 digital_gain;
	u32 exposure;
	u32 hdr_mode;
	u32 test_pattern;
	u32 binning;
};

/* Quirk flags */
#define IMX708_QUIRK_REVERSE_CLOCK BIT(0)
#define IMX708_QUIRK_NO_POWER_GPIO BIT(1)
#define IMX708_QUIRK_FLIP_MIPI BIT(2)

/* Register map unverified sentinel */
#define IMX708_REG_UNVERIFIED 0xFFFFFFFFU

/* Interrupt counters */
struct imx708_irq_counters
{
	atomic_t frame_start;
	atomic_t frame_end;
	atomic_t fifo_overflow;
	atomic_t pll_lock;
	atomic_t pll_unlock;
	atomic_t temperature;
	atomic_t error;
	atomic_t total;
};

/* Error counters */
struct imx708_error_counters
{
	atomic_t i2c_error;
	atomic_t timeout;
	atomic_t invalid_mode;
	atomic_t overtemp;
	atomic_t mipi_error;
	atomic_t other;
};

/*
 * Register I/O helpers.
 *
 * The IMX708 uses 16-bit register addresses with 8-bit data, so multi-byte
 * fields occupy consecutive addresses in big-endian order. regmap is
 * configured for 8-bit values; use these helpers for 16-bit fields instead
 * of calling regmap_read()/regmap_write() directly.
 */
int imx708_read_reg16(struct imx708_dev *sensor, u32 reg, u32 *val);
int imx708_write_reg16(struct imx708_dev *sensor, u32 reg, u32 val);

/* Char device functions */
int imx708_chardev_register(struct imx708_dev *sensor, unsigned int id);
void imx708_chardev_unregister(struct imx708_dev *sensor);

/* Debugfs functions */
int imx708_debugfs_init(void);
void imx708_debugfs_exit(void);
int imx708_debugfs_register(struct imx708_dev *sensor, unsigned int id);
void imx708_debugfs_unregister(struct imx708_dev *sensor);

/* IRQ functions */
int imx708_irq_init(struct imx708_dev *sensor);

/* PM functions */
int imx708_pm_init(struct imx708_dev *sensor);
int imx708_suspend(struct device *dev);
int imx708_resume(struct device *dev);
int imx708_runtime_suspend(struct device *dev);
int imx708_runtime_resume(struct device *dev);

/*
 * Hardware power sequencing (regulators, reset GPIO and the sensor init
 * register sequence). Implemented in imx708_main.c and driven exclusively
 * by the runtime-PM callbacks in imx708_pm.c. Neither takes sensor->lock.
 */
int imx708_hw_power_up(struct device *dev);
int imx708_hw_power_down(struct device *dev);

/* Sysfs attribute groups, attached via i2c_driver.driver.dev_groups */
extern const struct attribute_group *imx708_attr_groups[];

#endif /* _IMX708_PLATFORM_H_ */
