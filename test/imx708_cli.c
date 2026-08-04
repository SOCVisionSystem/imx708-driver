/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * imx708_cli.c - Interactive CLI for IMX708 sensor
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * Interactive command-line tool for driving the IMX708 sensor manually.
 * Useful for bring-up and debugging.
 *
 * Usage:
 *   ./imx708_cli <device> <command> [args...]
 *
 * Commands:
 *   status              - Show sensor status
 *   modes               - List available modes
 *   mode <index>        - Show mode info
 *   gain [val]          - Get/set analog gain
 *   dgain [val]         - Get/set digital gain
 *   exposure [val]      - Get/set exposure
 *   pattern [val]       - Get/set test pattern (0-4)
 *   stream [on|off]     - Start/stop streaming
 *   reset               - Software reset
 *   regread <addr>      - Read register
 *   regwrite <addr> <v> - Write register
 *   monitor             - Continuously monitor status
 *   help                - Show this help
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "libimx708.h"

static struct imx708_handle *g_handle = NULL;

static void cmd_status(void)
{
	struct imx708_sensor_status st;
	int ret;

	ret = imx708_get_status(g_handle, &st);
	if (ret < 0) {
		printf("Error: %s\n", imx708_strerror(ret));
		return;
	}

	printf("Sensor Status:\n");
	printf("  Temperature:  %d C\n", st.temperature);
	printf("  Frame Count:  %u\n", st.frame_count);
	printf("  PLL Locked:   %s\n", st.pll_locked ? "yes" : "no");
	printf("  Streaming:    %s\n", st.streaming ? "yes" : "no");
	printf("  Error:        %s\n", st.error ? "yes" : "no");
}

static void cmd_modes(void)
{
	uint32_t num_modes;
	int ret;

	ret = imx708_get_num_modes(g_handle, &num_modes);
	if (ret < 0) {
		printf("Error: %s\n", imx708_strerror(ret));
		return;
	}

	printf("Available modes: %u\n\n", num_modes);

	for (uint32_t i = 0; i < num_modes; i++) {
		struct imx708_mode_info info;
		ret = imx708_get_mode_info(g_handle, i, &info);
		if (ret < 0) {
			printf("  [%u] Error: %s\n", i, imx708_strerror(ret));
			continue;
		}
		printf("  [%u] %ux%u @ %u fps (code=0x%x, %u-bit)\n",
		       i, info.width, info.height, info.fps,
		       info.code, info.bit_depth);
	}
}

static void cmd_gain(int argc, char *argv[])
{
	if (argc > 0) {
		uint32_t val = strtoul(argv[0], NULL, 0);
		int ret = imx708_set_gain(g_handle, val, 0x100);
		if (ret < 0)
			printf("Error: %s\n", imx708_strerror(ret));
		else
			printf("Gain set to 0x%04x\n", val);
	} else {
		struct imx708_gain_config cfg;
		int ret = imx708_get_gain(g_handle, &cfg);
		if (ret < 0)
			printf("Error: %s\n", imx708_strerror(ret));
		else
			printf("Analog gain:  0x%04x\n"
			       "Digital gain: 0x%04x\n",
			       cfg.analog_gain, cfg.digital_gain);
	}
}

static void cmd_dgain(int argc, char *argv[])
{
	if (argc > 0) {
		uint32_t val = strtoul(argv[0], NULL, 0);
		int ret = imx708_set_gain(g_handle, 0x80, val);
		if (ret < 0)
			printf("Error: %s\n", imx708_strerror(ret));
		else
			printf("Digital gain set to 0x%04x\n", val);
	} else {
		struct imx708_gain_config cfg;
		int ret = imx708_get_gain(g_handle, &cfg);
		if (ret < 0)
			printf("Error: %s\n", imx708_strerror(ret));
		else
			printf("Digital gain: 0x%04x\n", cfg.digital_gain);
	}
}

static void cmd_exposure(int argc, char *argv[])
{
	if (argc > 0) {
		uint32_t val = strtoul(argv[0], NULL, 0);
		int ret = imx708_set_exposure(g_handle, val);
		if (ret < 0)
			printf("Error: %s\n", imx708_strerror(ret));
		else
			printf("Exposure set to %u\n", val);
	} else {
		struct imx708_exposure_config cfg;
		int ret = imx708_get_exposure(g_handle, &cfg);
		if (ret < 0)
			printf("Error: %s\n", imx708_strerror(ret));
		else
			printf("Exposure: %u\n", cfg.exposure);
	}
}

static void cmd_pattern(int argc, char *argv[])
{
	if (argc > 0) {
		struct imx708_test_pattern_config cfg;
		cfg.pattern = strtoul(argv[0], NULL, 0);
		cfg.color = 0;
		cfg.brightness = 128;
		int ret = imx708_set_test_pattern(g_handle, &cfg);
		if (ret < 0)
			printf("Error: %s\n", imx708_strerror(ret));
		else
			printf("Test pattern set to %u\n", cfg.pattern);
	} else {
		struct imx708_test_pattern_config cfg;
		int ret = imx708_get_test_pattern(g_handle, &cfg);
		if (ret < 0)
			printf("Error: %s\n", imx708_strerror(ret));
		else
			printf("Test pattern: %u\n", cfg.pattern);
	}
}

static void cmd_stream(int argc, char *argv[])
{
	if (argc > 0) {
		int enable = !strcmp(argv[0], "on") || !strcmp(argv[0], "1");
		int ret;

		if (enable)
			ret = imx708_start_stream(g_handle);
		else
			ret = imx708_stop_stream(g_handle);

		if (ret < 0)
			printf("Error: %s\n", imx708_strerror(ret));
		else
			printf("Streaming %s\n", enable ? "started" : "stopped");
	} else {
		struct imx708_sensor_status st;
		int ret = imx708_get_status(g_handle, &st);
		if (ret < 0)
			printf("Error: %s\n", imx708_strerror(ret));
		else
			printf("Streaming: %s\n",
			       st.streaming ? "on" : "off");
	}
}

static void cmd_reset(void)
{
	int ret = imx708_soft_reset(g_handle);
	if (ret < 0)
		printf("Error: %s\n", imx708_strerror(ret));
	else
		printf("Sensor reset\n");
}

static void cmd_regread(int argc, char *argv[])
{
	if (argc < 1) {
		printf("Usage: regread <addr>\n");
		return;
	}

	uint32_t addr = strtoul(argv[0], NULL, 0);
	uint32_t val;
	int ret = imx708_read_reg(g_handle, addr, &val);

	if (ret < 0)
		printf("Error: %s\n", imx708_strerror(ret));
	else
		printf("REG[0x%04x] = 0x%04x\n", addr, val);
}

static void cmd_regwrite(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Usage: regwrite <addr> <val>\n");
		return;
	}

	uint32_t addr = strtoul(argv[0], NULL, 0);
	uint32_t val = strtoul(argv[1], NULL, 0);
	int ret = imx708_write_reg(g_handle, addr, val);

	if (ret < 0)
		printf("Error: %s\n", imx708_strerror(ret));
	else
		printf("REG[0x%04x] <- 0x%04x\n", addr, val);
}

static void cmd_monitor(void)
{
	printf("Monitoring sensor (Ctrl+C to stop)...\n\n");
	printf("%-12s %-8s %-8s %-8s %-8s\n",
	       "Time", "Temp(C)", "Frames", "PLL", "Stream");
	printf("------------------------------------------------\n");

	for (int i = 0; i < 60; i++) {
		struct imx708_sensor_status st;
		int ret = imx708_get_status(g_handle, &st);
		if (ret == 0) {
			time_t now = time(NULL);
			struct tm *tm = localtime(&now);
			printf("%02d:%02d:%02d  %-8d %-8u %-8s %-8s\n",
			       tm->tm_hour, tm->tm_min, tm->tm_sec,
			       st.temperature, st.frame_count,
			       st.pll_locked ? "locked" : "unlock",
			       st.streaming ? "on" : "off");
		}
		sleep(1);
	}
}

static void cmd_help(void)
{
	printf("IMX708 Sensor CLI\n");
	printf("=================\n\n");
	printf("Usage: imx708_cli <device> <command> [args...]\n\n");
	printf("Commands:\n");
	printf("  status              - Show sensor status\n");
	printf("  modes               - List available modes\n");
	printf("  gain [val]          - Get/set analog gain\n");
	printf("  dgain [val]         - Get/set digital gain\n");
	printf("  exposure [val]      - Get/set exposure\n");
	printf("  pattern [val]       - Get/set test pattern (0-4)\n");
	printf("  stream [on|off]     - Start/stop streaming\n");
	printf("  reset               - Software reset\n");
	printf("  regread <addr>      - Read register\n");
	printf("  regwrite <addr> <v> - Write register\n");
	printf("  monitor             - Continuously monitor status\n");
	printf("  help                - Show this help\n");
}

int main(int argc, char *argv[])
{
	const char *devpath;
	const char *cmd;
	int ret;

	if (argc < 3) {
		cmd_help();
		return 1;
	}

	devpath = argv[1];
	cmd = argv[2];

	ret = imx708_open(devpath, &g_handle);
	if (ret < 0) {
		fprintf(stderr, "Failed to open %s: %s\n",
			devpath, imx708_strerror(ret));
		return 1;
	}

	if (!strcmp(cmd, "status"))		cmd_status();
	else if (!strcmp(cmd, "modes"))	cmd_modes();
	else if (!strcmp(cmd, "gain"))		cmd_gain(argc - 3, argv + 3);
	else if (!strcmp(cmd, "dgain"))		cmd_dgain(argc - 3, argv + 3);
	else if (!strcmp(cmd, "exposure"))	cmd_exposure(argc - 3, argv + 3);
	else if (!strcmp(cmd, "pattern"))	cmd_pattern(argc - 3, argv + 3);
	else if (!strcmp(cmd, "stream"))	cmd_stream(argc - 3, argv + 3);
	else if (!strcmp(cmd, "reset"))		cmd_reset();
	else if (!strcmp(cmd, "regread"))	cmd_regread(argc - 3, argv + 3);
	else if (!strcmp(cmd, "regwrite"))	cmd_regwrite(argc - 3, argv + 3);
	else if (!strcmp(cmd, "monitor"))	cmd_monitor();
	else if (!strcmp(cmd, "help"))		cmd_help();
	else {
		fprintf(stderr, "Unknown command: %s\n", cmd);
		cmd_help();
		ret = 1;
	}

	imx708_close(g_handle);
	return ret;
}
