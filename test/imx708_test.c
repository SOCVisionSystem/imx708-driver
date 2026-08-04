/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * imx708_test.c - Pass/fail test suite for IMX708 sensor driver
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * Pass/fail test suite for the IMX708 sensor. Each test case prints
 * PASS or FAIL, and the suite exits with 0 if all pass, 1 if any fail.
 *
 * Run:
 *   ./imx708_test [/dev/imx7080]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include "libimx708.h"

static struct imx708_handle *g_handle = NULL;
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name)						\
	do {							\
		g_tests_run++;					\
		printf("  TEST %d: %s ... ", g_tests_run, name);	\
		fflush(stdout);					\
	} while (0)

#define PASS()							\
	do {							\
		printf("PASS\n");				\
		g_tests_passed++;				\
	} while (0)

#define FAIL(msg)						\
	do {							\
		printf("FAIL: %s\n", msg);			\
		g_tests_failed++;				\
	} while (0)

#define ASSERT(cond, msg)					\
	do {							\
		if (!(cond)) { FAIL(msg); return; }		\
	} while (0)

/* ------------------------------------------------------------------ */
/* Test cases                                                          */
/* ------------------------------------------------------------------ */

static void test_open_close(void)
{
	struct imx708_handle *h = NULL;
	int ret;

	TEST("open/close cycle");
	ret = imx708_open("/dev/imx7080", &h);
	ASSERT(ret == 0, "open failed");
	ASSERT(h != NULL, "handle is NULL");
	imx708_close(h);
	PASS();
}

static void test_double_close(void)
{
	TEST("double close (NULL safety)");
	imx708_close(NULL);
	PASS();
}

static void test_invalid_open(void)
{
	struct imx708_handle *h = NULL;
	int ret;

	TEST("open non-existent device");
	ret = imx708_open("/dev/imx708_nonexistent", &h);
	ASSERT(ret < 0, "expected failure");
	ASSERT(h == NULL, "handle should be NULL on failure");
	PASS();
}

static void test_get_num_modes(void)
{
	uint32_t num_modes;
	int ret;

	TEST("get number of modes");
	ret = imx708_get_num_modes(g_handle, &num_modes);
	ASSERT(ret == 0, "get_num_modes failed");
	ASSERT(num_modes > 0, "expected at least 1 mode");
	printf("(%u modes) ", num_modes);
	PASS();
}

static void test_get_mode_info(void)
{
	struct imx708_mode_info info;
	uint32_t num_modes;
	int ret;

	TEST("get mode info for all modes");
	ret = imx708_get_num_modes(g_handle, &num_modes);
	ASSERT(ret == 0, "get_num_modes failed");

	for (uint32_t i = 0; i < num_modes; i++) {
		ret = imx708_get_mode_info(g_handle, i, &info);
		ASSERT(ret == 0, "get_mode_info failed");
		ASSERT(info.width > 0, "width must be > 0");
		ASSERT(info.height > 0, "height must be > 0");
		ASSERT(info.fps > 0, "fps must be > 0");
	}
	PASS();
}

static void test_get_status(void)
{
	struct imx708_sensor_status status;
	int ret;

	TEST("get sensor status");
	ret = imx708_get_status(g_handle, &status);
	ASSERT(ret == 0, "get_status failed");
	PASS();
}

static void test_set_get_gain(void)
{
	struct imx708_gain_config cfg;
	int ret;

	TEST("set and get gain");
	ret = imx708_set_gain(g_handle, 0x80, 0x100);
	ASSERT(ret == 0, "set_gain failed");

	ret = imx708_get_gain(g_handle, &cfg);
	ASSERT(ret == 0, "get_gain failed");
	PASS();
}

static void test_set_get_exposure(void)
{
	struct imx708_exposure_config cfg;
	int ret;

	TEST("set and get exposure");
	ret = imx708_set_exposure(g_handle, 1000);
	ASSERT(ret == 0, "set_exposure failed");

	ret = imx708_get_exposure(g_handle, &cfg);
	ASSERT(ret == 0, "get_exposure failed");
	PASS();
}

static void test_set_get_test_pattern(void)
{
	struct imx708_test_pattern_config cfg;
	int ret;

	TEST("set and get test pattern");
	cfg.pattern = 0;
	cfg.color = 0;
	cfg.brightness = 128;
	ret = imx708_set_test_pattern(g_handle, &cfg);
	ASSERT(ret == 0, "set_test_pattern failed");

	ret = imx708_get_test_pattern(g_handle, &cfg);
	ASSERT(ret == 0, "get_test_pattern failed");
	PASS();
}

static void test_start_stop_stream(void)
{
	int ret;

	TEST("start/stop stream cycle");
	ret = imx708_start_stream(g_handle);
	ASSERT(ret == 0, "start_stream failed");

	usleep(100000); /* 100ms */

	ret = imx708_stop_stream(g_handle);
	ASSERT(ret == 0, "stop_stream failed");
	PASS();
}

static void test_soft_reset(void)
{
	int ret;

	TEST("software reset");
	ret = imx708_soft_reset(g_handle);
	ASSERT(ret == 0, "soft_reset failed");

	usleep(50000); /* wait for reset */
	PASS();
}

static void test_invalid_arguments(void)
{
	int ret;

	TEST("NULL handle returns -EINVAL");
	ret = imx708_get_num_modes(NULL, NULL);
	ASSERT(ret == -EINVAL, "expected -EINVAL");
	PASS();
}

static void test_repeated_open_close(void)
{
	TEST("repeated open/close (100 cycles)");
	for (int i = 0; i < 100; i++) {
		struct imx708_handle *h = NULL;
		int ret = imx708_open("/dev/imx7080", &h);
		if (ret != 0) {
			FAIL("open failed during cycle");
			return;
		}
		imx708_close(h);
	}
	PASS();
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

static const struct {
	const char *name;
	void (*func)(void);
} test_table[] = {
	{ "open/close",			test_open_close },
	{ "double close",		test_double_close },
	{ "invalid open",		test_invalid_open },
	{ "get num modes",		test_get_num_modes },
	{ "get mode info",		test_get_mode_info },
	{ "get status",			test_get_status },
	{ "set/get gain",		test_set_get_gain },
	{ "set/get exposure",		test_set_get_exposure },
	{ "set/get test pattern",	test_set_get_test_pattern },
	{ "start/stop stream",		test_start_stop_stream },
	{ "soft reset",			test_soft_reset },
	{ "invalid arguments",		test_invalid_arguments },
	{ "repeated open/close",	test_repeated_open_close },
	{ NULL, NULL },
};

int main(int argc, char *argv[])
{
	const char *devpath = argc > 1 ? argv[1] : "/dev/imx7080";
	int ret;

	printf("IMX708 Sensor Test Suite\n");
	printf("========================\n");
	printf("Device: %s\n\n", devpath);

	/* Open device for tests that need it */
	ret = imx708_open(devpath, &g_handle);
	if (ret != 0) {
		fprintf(stderr, "Warning: Could not open %s (%s) — "
			"some tests will be skipped\n",
			devpath, imx708_strerror(ret));
		g_handle = NULL;
	}

	/* Run tests */
	for (int i = 0; test_table[i].name; i++) {
		/* Skip tests that need a handle if we don't have one */
		if (!g_handle && test_table[i].func != test_open_close &&
		    test_table[i].func != test_double_close &&
		    test_table[i].func != test_invalid_open &&
		    test_table[i].func != test_invalid_arguments) {
			printf("  SKIP %d: %s (no device)\n",
			       g_tests_run + 1, test_table[i].name);
			g_tests_run++;
			continue;
		}
		test_table[i].func();
	}

	/* Cleanup */
	if (g_handle)
		imx708_close(g_handle);

	/* Summary */
	printf("\nResults: %d/%d passed, %d failed\n",
	       g_tests_passed, g_tests_run, g_tests_failed);

	return g_tests_failed > 0 ? 1 : 0;
}
