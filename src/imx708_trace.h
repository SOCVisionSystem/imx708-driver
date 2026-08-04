/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * imx708_trace.h - Trace event definitions for IMX708 sensor driver
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * Tracepoints for the IMX708 driver. These are near-zero cost when disabled
 * and give users trace-cmd and perf for free. Cover: probe/remove, IRQ entry
 * with event mask, frame start/end, error events with errno, state transitions.
 *
 * Usage:
 *   # trace-cmd record -e imx708:*
 *   # perf trace -e imx708:*
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM imx708

#if !defined(_IMX708_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _IMX708_TRACE_H_

#include <linux/tracepoint.h>
#include <linux/types.h>

DECLARE_EVENT_CLASS(imx708_event,
	TP_PROTO(const char *name, u32 instance),
	TP_ARGS(name, instance),
	TP_STRUCT__entry(
		__string(name, name)
		__field(u32, instance)
	),
	TP_fast_assign(
		__assign_str(name);
		__entry->instance = instance;
	),
	TP_printk("%s[%u]", __get_str(name), __entry->instance)
);

/* Probe and remove events */
DEFINE_EVENT(imx708_event, imx708_probe,
	TP_PROTO(const char *name, u32 instance),
	TP_ARGS(name, instance)
);

DEFINE_EVENT(imx708_event, imx708_remove,
	TP_PROTO(const char *name, u32 instance),
	TP_ARGS(name, instance)
);

/* Interrupt events */
TRACE_EVENT(imx708_irq,
	TP_PROTO(u32 events, u32 instance),
	TP_ARGS(events, instance),
	TP_STRUCT__entry(
		__field(u32, events)
		__field(u32, instance)
	),
	TP_fast_assign(
		__entry->events = events;
		__entry->instance = instance;
	),
	TP_printk("instance=%u events=0x%08x", __entry->instance, __entry->events)
);

/* Frame events */
TRACE_EVENT(imx708_frame,
	TP_PROTO(u32 frame_count, u32 instance, bool start),
	TP_ARGS(frame_count, instance, start),
	TP_STRUCT__entry(
		__field(u32, frame_count)
		__field(u32, instance)
		__field(bool, start)
	),
	TP_fast_assign(
		__entry->frame_count = frame_count;
		__entry->instance = instance;
		__entry->start = start;
	),
	TP_printk("instance=%u frame=%u %s",
		  __entry->instance, __entry->frame_count,
		  __entry->start ? "START" : "END")
);

/* Error events */
TRACE_EVENT(imx708_error,
	TP_PROTO(const char *msg, int errno, u32 instance),
	TP_ARGS(msg, errno, instance),
	TP_STRUCT__entry(
		__string(msg, msg)
		__field(int, errno)
		__field(u32, instance)
	),
	TP_fast_assign(
		__assign_str(msg);
		__entry->errno = errno;
		__entry->instance = instance;
	),
	TP_printk("instance=%u err=%d msg=%s",
		  __entry->instance, __entry->errno, __get_str(msg))
);

/* Streaming state transitions */
TRACE_EVENT(imx708_stream,
	TP_PROTO(u32 instance, bool active),
	TP_ARGS(instance, active),
	TP_STRUCT__entry(
		__field(u32, instance)
		__field(bool, active)
	),
	TP_fast_assign(
		__entry->instance = instance;
		__entry->active = active;
	),
	TP_printk("instance=%u %s",
		  __entry->instance,
		  __entry->active ? "STREAM_ON" : "STREAM_OFF")
);

/* Power state transitions */
TRACE_EVENT(imx708_power,
	TP_PROTO(u32 instance, bool on),
	TP_ARGS(instance, on),
	TP_STRUCT__entry(
		__field(u32, instance)
		__field(bool, on)
	),
	TP_fast_assign(
		__entry->instance = instance;
		__entry->on = on;
	),
	TP_printk("instance=%u %s",
		  __entry->instance,
		  __entry->on ? "POWER_ON" : "POWER_OFF")
);

/* Register access trace (debug, rate-limited) */
TRACE_EVENT(imx708_reg_access,
	TP_PROTO(u32 reg, u32 val, bool write, u32 instance),
	TP_ARGS(reg, val, write, instance),
	TP_STRUCT__entry(
		__field(u32, reg)
		__field(u32, val)
		__field(bool, write)
		__field(u32, instance)
	),
	TP_fast_assign(
		__entry->reg = reg;
		__entry->val = val;
		__entry->write = write;
		__entry->instance = instance;
	),
	TP_printk("instance=%u reg=0x%04x %s=0x%04x",
		  __entry->instance, __entry->reg,
		  __entry->write ? "W" : "R",
		  __entry->val)
);

#endif /* _IMX708_TRACE_H_ */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE imx708_trace
#include <trace/define_trace.h>
