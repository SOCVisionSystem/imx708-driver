// SPDX-License-Identifier: GPL-2.0-only
/*
 * imx708_trace.c - Trace point definitions for IMX708 sensor driver
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * This file defines the trace points for the IMX708 driver. It must be
 * compiled exactly once with CREATE_TRACE_POINTS defined so the trace
 * event infrastructure creates the actual tracepoint symbols.
 *
 * This file is intentionally minimal — all the trace event definitions
 * live in include/imx708_trace.h.
 */

#define CREATE_TRACE_POINTS
#include "imx708_trace.h"
