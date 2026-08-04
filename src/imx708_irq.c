// SPDX-License-Identifier: GPL-2.0-only
/*
 * imx708_irq.c - Interrupt handling for IMX708 sensor driver
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * This file implements the interrupt handling for the IMX708 sensor.
 * The sensor can generate interrupts for frame start/end, FIFO overflow,
 * PLL lock/unlock, temperature events, and errors.
 *
 * The hardirq handler does the minimum: read and acknowledge the status
 * register, latch the event mask, and return IRQ_WAKE_THREAD. Everything
 * else runs in the threaded handler.
 *
 * Locking: The hardirq handler does not take sensor->lock — it only
 * latches events into pending_events (atomic). The threaded handler
 * takes sensor->lock before processing events.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/slab.h>

#include "imx708_platform.h"
#include "imx708_regs.h"
#include "imx708_trace.h"

#define DRV_NAME "imx708"

/* ------------------------------------------------------------------ */
/* Threaded handler                                                    */
/* ------------------------------------------------------------------ */

/**
 * imx708_threaded_irq - Threaded IRQ handler (runs in process context)
 * @irq:   Interrupt number
 * @data:  Pointer to struct imx708_dev
 *
 * The sensor is on an I2C bus, so acknowledging the interrupt requires a
 * bus transfer that may sleep. That makes a hard-IRQ handler impossible:
 * the handler is registered with a NULL primary handler and IRQF_ONESHOT
 * so the line stays masked until this function has read and cleared the
 * status register.
 *
 * Handles each event type:
 *
 *   FRAME_START/END - Update frame counters
 *   FIFO_OVERFLOW   - Log error, increment error counter
 *   PLL_LOCK/UNLOCK - Log state change
 *   TEMPERATURE     - Read and log die temperature
 *   ERROR           - Read error status register
 */
static irqreturn_t imx708_threaded_irq(int irq, void *data)
{
	struct imx708_dev *sensor = data;
	u32 events;

	mutex_lock(&sensor->lock);

	/* Read + clear the status register; this is what may sleep. */
	events = sensor->soc->ops->irq_ack(sensor);
	events |= (u32)atomic_xchg(&sensor->pending_events, 0);

	if (!events)
	{
		mutex_unlock(&sensor->lock);
		return IRQ_NONE; /* shared line, not ours */
	}

	trace_imx708_irq(events, sensor->client->addr);

	if (events & IMX708_INT_FRAME_START)
	{
		if (sensor->irq_counters)
			atomic_inc(&sensor->irq_counters->frame_start);
		trace_imx708_frame(0, sensor->client->addr, true);
	}

	if (events & IMX708_INT_FRAME_END)
	{
		if (sensor->irq_counters)
			atomic_inc(&sensor->irq_counters->frame_end);
		trace_imx708_frame(0, sensor->client->addr, false);
	}

	if (events & IMX708_INT_FIFO_OVERFLOW)
	{
		dev_err_ratelimited(sensor->dev, "FIFO overflow\n");
		if (sensor->irq_counters)
			atomic_inc(&sensor->irq_counters->fifo_overflow);
		if (sensor->error_counters)
			atomic_inc(&sensor->error_counters->other);
		trace_imx708_error("FIFO overflow", -EOVERFLOW,
						   sensor->client->addr);
	}

	if (events & IMX708_INT_PLL_LOCK)
	{
		dev_dbg(sensor->dev, "PLL locked\n");
		if (sensor->irq_counters)
			atomic_inc(&sensor->irq_counters->pll_lock);
	}

	if (events & IMX708_INT_PLL_UNLOCK)
	{
		dev_warn(sensor->dev, "PLL unlocked!\n");
		if (sensor->irq_counters)
			atomic_inc(&sensor->irq_counters->pll_unlock);
		if (sensor->error_counters)
			atomic_inc(&sensor->error_counters->other);
		trace_imx708_error("PLL unlock", -EIO,
						   sensor->client->addr);
	}

	if (events & IMX708_INT_TEMPERATURE)
	{
		u32 temp;

		if (!regmap_read(sensor->regmap, IMX708_REG_TEMPERATURE, &temp))
		{
			dev_dbg(sensor->dev, "die temperature: %d C\n",
					(s32)(s8)temp);
		}
		if (sensor->irq_counters)
			atomic_inc(&sensor->irq_counters->temperature);
	}

	if (events & IMX708_INT_ERROR)
	{
		u32 err_status = 0;

		imx708_read_reg16(sensor, IMX708_REG_STATUS, &err_status);
		dev_err_ratelimited(sensor->dev,
							"sensor error, status=0x%04x\n",
							err_status);
		if (sensor->irq_counters)
			atomic_inc(&sensor->irq_counters->error);
		if (sensor->error_counters)
			atomic_inc(&sensor->error_counters->other);
		trace_imx708_error("sensor error", -EIO,
						   sensor->client->addr);
	}

	mutex_unlock(&sensor->lock);
	return IRQ_HANDLED;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int imx708_irq_init(struct imx708_dev *sensor)
{
	struct device *dev = sensor->dev;
	int ret;

	/* Allocate interrupt counters */
	sensor->irq_counters = devm_kzalloc(dev,
										sizeof(*sensor->irq_counters),
										GFP_KERNEL);
	if (!sensor->irq_counters)
		return -ENOMEM;

	/* Allocate error counters */
	sensor->error_counters = devm_kzalloc(dev,
										  sizeof(*sensor->error_counters),
										  GFP_KERNEL);
	if (!sensor->error_counters)
		return -ENOMEM;

	atomic_set(&sensor->pending_events, 0);

	/*
	 * The IMX708 module on the Raspberry Pi Camera Module 3 does not
	 * route an interrupt to the host, and the DT binding marks
	 * "interrupts" optional. Counters are still allocated above so that
	 * debugfs and the status ioctls work on those boards.
	 */
	if (sensor->client->irq <= 0)
	{
		dev_dbg(dev, "no interrupt line, IRQ handling disabled\n");
		return 0;
	}

	/*
	 * NULL primary handler + IRQF_ONESHOT: acknowledging the interrupt
	 * needs an I2C transfer, which may sleep and therefore cannot run in
	 * hard-IRQ context. The line stays masked until the thread returns.
	 */
	ret = devm_request_threaded_irq(dev, sensor->client->irq,
									NULL,
									imx708_threaded_irq,
									IRQF_TRIGGER_RISING | IRQF_SHARED |
										IRQF_ONESHOT,
									DRV_NAME, sensor);
	if (ret)
	{
		dev_err(dev, "failed to request threaded IRQ %d: %d\n",
				sensor->client->irq, ret);
		return ret;
	}

	dev_dbg(dev, "IRQ %d registered\n", sensor->client->irq);
	return 0;
}
