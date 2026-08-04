// SPDX-License-Identifier: GPL-2.0-only
/*
 * imx708_debugfs.c - Debugfs interface for IMX708 sensor driver
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * This file implements the debugfs interface for diagnostics and fault
 * injection. Everything here is diagnostic and explicitly unstable — no
 * product should depend on it.
 *
 * Debugfs entries:
 *   /sys/kernel/debug/imx708/<instance>/
 *   ├── registers        - R: dump all sensor registers
 *   ├── irq_counters     - R: interrupt event counters
 *   ├── error_counters   - R: error counters by type
 *   ├── state            - R: driver state machine
 *   └── fault/           - W: fault injection controls (if CONFIG_IMX708_FAULT_INJECT=y)
 *       ├── probe_fail
 *       ├── irq_timeout
 *       ├── reg_read_fail
 *       ├── reg_write_fail
 *       └── stats
 */

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "imx708_platform.h"
#include "imx708_regs.h"

#define DRV_NAME	"imx708"

/* ------------------------------------------------------------------ */
/* Debugfs directory tree                                               */
/* ------------------------------------------------------------------ */

static struct dentry *imx708_debugfs_root;

/* Register dump */
static int imx708_debugfs_registers_show(struct seq_file *s, void *v)
{
	struct imx708_dev *sensor = s->private;
	u32 val;
	int ret;

	mutex_lock(&sensor->lock);

	seq_printf(s, "=== %s Register Dump ===\n\n", sensor->soc->name);

#define DUMP_REG(addr, name)						\
	do {								\
		ret = regmap_read(sensor->regmap, addr, &val);	\
		if (ret)						\
			seq_printf(s, "  %-30s = ERROR %d\n", name, ret); \
		else							\
			seq_printf(s, "  %-30s = 0x%04x\n", name, val);	\
	} while (0)

	DUMP_REG(IMX708_REG_MODE_SELECT, "MODE_SELECT");
	DUMP_REG(IMX708_REG_CHIP_ID, "CHIP_ID");
	DUMP_REG(IMX708_REG_MODULE_ID, "MODULE_ID");
	DUMP_REG(IMX708_REG_FRAME_LENGTH, "FRAME_LENGTH");
	DUMP_REG(IMX708_REG_LINE_LENGTH, "LINE_LENGTH");
	DUMP_REG(IMX708_REG_X_OUTPUT_SIZE, "X_OUTPUT_SIZE");
	DUMP_REG(IMX708_REG_Y_OUTPUT_SIZE, "Y_OUTPUT_SIZE");
	DUMP_REG(IMX708_REG_EXPOSURE, "EXPOSURE");
	DUMP_REG(IMX708_REG_ANALOG_GAIN, "ANALOG_GAIN");
	DUMP_REG(IMX708_REG_DIGITAL_GAIN, "DIGITAL_GAIN");
	DUMP_REG(IMX708_REG_TEST_PATTERN, "TEST_PATTERN");
	DUMP_REG(IMX708_REG_TEMPERATURE, "TEMPERATURE");
	DUMP_REG(IMX708_REG_ORIENTATION, "ORIENTATION");
	DUMP_REG(IMX708_REG_BINNING_MODE, "BINNING_MODE");
	DUMP_REG(IMX708_REG_BINNING_TYPE, "BINNING_TYPE");
	DUMP_REG(IMX708_REG_INTERRUPT_ENABLE, "INTERRUPT_ENABLE");
	DUMP_REG(IMX708_REG_INTERRUPT_STATUS, "INTERRUPT_STATUS");
	DUMP_REG(IMX708_REG_STATUS, "STATUS");
	DUMP_REG(IMX708_LONG_EXP_SHIFT_REG, "LONG_EXP_SHIFT");
	DUMP_REG(IMX708_REG_COLOUR_BALANCE_RED, "COLOUR_BALANCE_R");
	DUMP_REG(IMX708_REG_COLOUR_BALANCE_BLUE, "COLOUR_BALANCE_B");

#undef DUMP_REG

	seq_printf(s, "\n  Streaming: %s\n",
		   sensor->streaming ? "ACTIVE" : "STANDBY");
	seq_printf(s, "  Power count: %d\n", sensor->power_count);
	seq_printf(s, "  Current mode: %ux%u @ %u fps\n",
		   sensor->mode ? sensor->mode->width : 0,
		   sensor->mode ? sensor->mode->height : 0,
		   sensor->mode ? sensor->mode->fps : 0);

	mutex_unlock(&sensor->lock);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(imx708_debugfs_registers);

/* Interrupt counters */
static int imx708_debugfs_irq_counters_show(struct seq_file *s, void *v)
{
	struct imx708_dev *sensor = s->private;
	struct imx708_irq_counters *cnt = sensor->irq_counters;

	if (!cnt) {
		seq_puts(s, "IRQ counters not initialized\n");
		return 0;
	}

	seq_printf(s, "Frame Start:     %u\n",
		   atomic_read(&cnt->frame_start));
	seq_printf(s, "Frame End:       %u\n",
		   atomic_read(&cnt->frame_end));
	seq_printf(s, "FIFO Overflow:   %u\n",
		   atomic_read(&cnt->fifo_overflow));
	seq_printf(s, "PLL Lock:        %u\n",
		   atomic_read(&cnt->pll_lock));
	seq_printf(s, "PLL Unlock:      %u\n",
		   atomic_read(&cnt->pll_unlock));
	seq_printf(s, "Temperature:     %u\n",
		   atomic_read(&cnt->temperature));
	seq_printf(s, "Error:           %u\n",
		   atomic_read(&cnt->error));
	seq_printf(s, "Total IRQs:      %u\n",
		   atomic_read(&cnt->total));

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(imx708_debugfs_irq_counters);

/* Error counters */
static int imx708_debugfs_error_counters_show(struct seq_file *s, void *v)
{
	struct imx708_dev *sensor = s->private;
	struct imx708_error_counters *cnt = sensor->error_counters;

	if (!cnt) {
		seq_puts(s, "Error counters not initialized\n");
		return 0;
	}

	seq_printf(s, "I2C Errors:      %u\n",
		   atomic_read(&cnt->i2c_error));
	seq_printf(s, "Timeouts:        %u\n",
		   atomic_read(&cnt->timeout));
	seq_printf(s, "Invalid Mode:    %u\n",
		   atomic_read(&cnt->invalid_mode));
	seq_printf(s, "Overtemp:        %u\n",
		   atomic_read(&cnt->overtemp));
	seq_printf(s, "MIPI Errors:     %u\n",
		   atomic_read(&cnt->mipi_error));
	seq_printf(s, "Other:           %u\n",
		   atomic_read(&cnt->other));

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(imx708_debugfs_error_counters);

/* Driver state */
static int imx708_debugfs_state_show(struct seq_file *s, void *v)
{
	struct imx708_dev *sensor = s->private;

	mutex_lock(&sensor->lock);

	seq_printf(s, "Driver:          %s\n", DRV_NAME);
	seq_printf(s, "Version:         %s\n", DRV_VERSION);
	seq_printf(s, "Sensor:          %s\n", sensor->soc->name);
	seq_printf(s, "I2C Addr:        0x%02x\n", sensor->client->addr);
	seq_printf(s, "MIPI Lanes:      %u\n", sensor->soc->num_channels);
	seq_printf(s, "Streaming:       %s\n",
		   sensor->streaming ? "yes" : "no");
	seq_printf(s, "Power Count:     %d\n", sensor->power_count);
	seq_printf(s, "Current Mode:    %ux%u @ %u fps\n",
		   sensor->mode ? sensor->mode->width : 0,
		   sensor->mode ? sensor->mode->height : 0,
		   sensor->mode ? sensor->mode->fps : 0);
	seq_printf(s, "Num Modes:       %u\n", sensor->soc->num_modes);
	seq_printf(s, "Quirks:          0x%08x\n", sensor->soc->quirks);

	mutex_unlock(&sensor->lock);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(imx708_debugfs_state);

/* ------------------------------------------------------------------ */
/* Fault injection (conditional)                                       */
/* ------------------------------------------------------------------ */

#ifdef CONFIG_IMX708_FAULT_INJECT

struct imx708_fault_state {
	struct dentry *dir;
	atomic_t probe_fail_errno;
	atomic_t irq_timeout_count;
	atomic_t reg_read_fail_count;
	atomic_t reg_write_fail_count;
	atomic_t dma_fail_count;
	atomic_t alloc_fail_count;

	/* Statistics */
	atomic_t injected_probe_fail;
	atomic_t injected_irq_timeout;
	atomic_t injected_reg_read_fail;
	atomic_t injected_reg_write_fail;
	atomic_t injected_dma_fail;
	atomic_t injected_alloc_fail;
};

static int imx708_fault_stats_show(struct seq_file *s, void *v)
{
	struct imx708_fault_state *fault = s->private;

	seq_printf(s, "probe_fail:       %u\n",
		   atomic_read(&fault->injected_probe_fail));
	seq_printf(s, "irq_timeout:      %u\n",
		   atomic_read(&fault->injected_irq_timeout));
	seq_printf(s, "reg_read_fail:    %u\n",
		   atomic_read(&fault->injected_reg_read_fail));
	seq_printf(s, "reg_write_fail:   %u\n",
		   atomic_read(&fault->injected_reg_write_fail));
	seq_printf(s, "dma_fail:         %u\n",
		   atomic_read(&fault->injected_dma_fail));
	seq_printf(s, "alloc_fail:       %u\n",
		   atomic_read(&fault->injected_alloc_fail));

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(imx708_fault_stats);

static ssize_t imx708_fault_store(struct file *filp, const char __user *ubuf,
				   size_t count, loff_t *ppos,
				   atomic_t *target)
{
	char buf[32];
	int val, ret;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, count))
		return -EFAULT;

	buf[count] = '\0';
	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;

	atomic_set(target, val);
	return count;
}

static int imx708_fault_probe_fail_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t imx708_fault_probe_fail_read(struct file *filp, char __user *ubuf,
					     size_t count, loff_t *ppos)
{
	struct imx708_fault_state *fault = filp->private_data;
	char buf[32];
	int len;

	len = snprintf(buf, sizeof(buf), "%d\n",
		       atomic_read(&fault->probe_fail_errno));
	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static ssize_t imx708_fault_probe_fail_write(struct file *filp,
					      const char __user *ubuf,
					      size_t count, loff_t *ppos)
{
	struct imx708_fault_state *fault = filp->private_data;
	return imx708_fault_store(filp, ubuf, count, ppos,
				   &fault->probe_fail_errno);
}

static const struct file_operations imx708_fault_probe_fail_fops = {
	.open	= imx708_fault_probe_fail_open,
	.read	= imx708_fault_probe_fail_read,
	.write	= imx708_fault_probe_fail_write,
};

/* Similar fops for other fault injection points would go here */

static int imx708_debugfs_create_fault_dir(struct imx708_dev *sensor,
					    struct dentry *parent)
{
	struct imx708_fault_state *fault;
	struct dentry *dir;

	fault = devm_kzalloc(sensor->dev, sizeof(*fault), GFP_KERNEL);
	if (!fault)
		return -ENOMEM;

	dir = debugfs_create_dir("fault", parent);
	if (IS_ERR(dir))
		return PTR_ERR(dir);

	fault->dir = dir;
	sensor->fault = fault;

	debugfs_create_file("probe_fail", 0600, dir, fault,
			    &imx708_fault_probe_fail_fops);
	debugfs_create_u32("irq_timeout", 0600, dir,
			    (u32 *)&fault->irq_timeout_count);
	debugfs_create_u32("reg_read_fail", 0600, dir,
			    (u32 *)&fault->reg_read_fail_count);
	debugfs_create_u32("reg_write_fail", 0600, dir,
			    (u32 *)&fault->reg_write_fail_count);
	debugfs_create_file("stats", 0400, dir, fault,
			    &imx708_fault_stats_fops);

	return 0;
}

#endif /* CONFIG_IMX708_FAULT_INJECT */

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int imx708_debugfs_init(void)
{
	imx708_debugfs_root = debugfs_create_dir(DRV_NAME, NULL);
	if (IS_ERR(imx708_debugfs_root))
		return PTR_ERR(imx708_debugfs_root);

	return 0;
}

void imx708_debugfs_exit(void)
{
	debugfs_remove_recursive(imx708_debugfs_root);
	imx708_debugfs_root = NULL;
}

int imx708_debugfs_register(struct imx708_dev *sensor, unsigned int id)
{
	struct dentry *dir;
	char name[16];

	if (!imx708_debugfs_root)
		return -ENODEV;

	snprintf(name, sizeof(name), "%u", id);

	dir = debugfs_create_dir(name, imx708_debugfs_root);
	if (IS_ERR(dir))
		return PTR_ERR(dir);

	sensor->debugfs_dir = dir;

	debugfs_create_file("registers", 0400, dir, sensor,
			    &imx708_debugfs_registers_fops);
	debugfs_create_file("irq_counters", 0400, dir, sensor,
			    &imx708_debugfs_irq_counters_fops);
	debugfs_create_file("error_counters", 0400, dir, sensor,
			    &imx708_debugfs_error_counters_fops);
	debugfs_create_file("state", 0400, dir, sensor,
			    &imx708_debugfs_state_fops);

#ifdef CONFIG_IMX708_FAULT_INJECT
	imx708_debugfs_create_fault_dir(sensor, dir);
#endif

	return 0;
}

void imx708_debugfs_unregister(struct imx708_dev *sensor)
{
	if (sensor->debugfs_dir)
		debugfs_remove_recursive(sensor->debugfs_dir);
}
