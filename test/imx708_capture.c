/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * imx708_capture.c - Frame capture and recording tool for IMX708 sensor
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * Full-featured capture and recording application for the IMX708 sensor.
 * Supports single frame capture, burst capture, video recording, and
 * configuration profiles.
 *
 * Usage:
 *   imx708_capture <device> <command> [args...]
 *
 * Commands:
 *   snap <file>         - Capture a single frame (PGM format)
 *   burst <n> <dir>     - Capture N frames to directory
 *   record <sec> <file> - Record video (raw frames)
 *   profile <file>      - Save current config as JSON profile
 *   apply <file>        - Load and apply a JSON profile
 *   info                - Show sensor information
 *   monitor             - Monitor sensor status continuously
 *   ae                  - Run auto-exposure loop
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>

#include "libimx708.h"

static struct imx708_handle *g_handle = NULL;
static volatile int g_running = 1;

static void handle_signal(int sig)
{
	(void)sig;
	g_running = 0;
}

/* ------------------------------------------------------------------ */
/* Single frame capture                                                */
/* ------------------------------------------------------------------ */

static int cmd_snap(int argc, char *argv[])
{
	const char *filepath = argc > 0 ? argv[0] : "capture.pgm";
	struct imx708_capture_params params;
	struct imx708_frame frame;
	int ret;

	printf("Capturing single frame...\n");

	memset(&params, 0, sizeof(params));
	params.width = 0; /* use default */
	params.height = 0;
	params.format = IMX708_CAPTURE_RAW10;
	params.num_frames = 1;
	params.timeout_ms = 5000;

	ret = imx708_capture_frame(g_handle, &params, &frame);
	if (ret < 0)
	{
		fprintf(stderr, "Capture failed: %s\n", imx708_strerror(ret));
		return 1;
	}

	printf("  Frame: %ux%u, %zu bytes\n",
		   frame.width, frame.height, frame.size);
	printf("  Gain: 0x%04x, Exposure: %u\n",
		   frame.gain, frame.exposure);

	ret = imx708_frame_save_pgm(&frame, filepath);
	if (ret < 0)
	{
		fprintf(stderr, "Save failed: %s\n", imx708_strerror(ret));
		imx708_frame_free(&frame);
		return 1;
	}

	printf("  Saved to: %s\n", filepath);
	imx708_frame_free(&frame);
	return 0;
}

/* ------------------------------------------------------------------ */
/* Burst capture                                                       */
/* ------------------------------------------------------------------ */

static int cmd_burst(int argc, char *argv[])
{
	int num_frames = argc > 0 ? atoi(argv[0]) : 10;
	const char *dir = argc > 1 ? argv[1] : ".";
	struct imx708_capture_params params;
	struct imx708_frame *frames = NULL;
	uint32_t num_captured = 0;
	int ret;
	char path[256];

	if (num_frames < 1)
		num_frames = 1;
	if (num_frames > 1000)
		num_frames = 1000;

	printf("Burst capture: %d frames -> %s/\n", num_frames, dir);

	/* Create directory if needed */
	mkdir(dir, 0755);

	memset(&params, 0, sizeof(params));
	params.format = IMX708_CAPTURE_RAW10;
	params.num_frames = num_frames;
	params.timeout_ms = 1000;

	ret = imx708_capture_frames(g_handle, &params, &frames, &num_captured);
	if (ret < 0)
	{
		fprintf(stderr, "Burst capture failed: %s\n", imx708_strerror(ret));
		return 1;
	}

	printf("Captured %u frames\n", num_captured);

	for (uint32_t i = 0; i < num_captured; i++)
	{
		snprintf(path, sizeof(path), "%s/frame_%04u.pgm", dir, i);
		ret = imx708_frame_save_pgm(&frames[i], path);
		if (ret == 0)
			printf("  [%u/%u] %s (%ux%u, %zu bytes)\n",
				   i + 1, num_captured, path,
				   frames[i].width, frames[i].height,
				   frames[i].size);
	}

	imx708_frames_free(frames, num_captured);
	return 0;
}

/* ------------------------------------------------------------------ */
/* Video recording (raw frame sequence)                                */
/* ------------------------------------------------------------------ */

static int cmd_record(int argc, char *argv[])
{
	int duration = argc > 0 ? atoi(argv[0]) : 10;
	const char *filepath = argc > 1 ? argv[1] : "recording.raw";
	FILE *f;
	struct timeval start, now;
	int ret;
	uint64_t frame_count = 0;

	if (duration < 1)
		duration = 1;
	if (duration > 3600)
		duration = 3600;

	printf("Recording for %d seconds -> %s\n", duration, filepath);
	printf("Press Ctrl+C to stop early\n");

	f = fopen(filepath, "wb");
	if (!f)
	{
		fprintf(stderr, "Failed to open %s: %s\n", filepath, strerror(errno));
		return 1;
	}

	/* Write header */
	struct
	{
		uint32_t magic;
		uint32_t version;
		uint32_t width;
		uint32_t height;
		uint32_t format;
		uint64_t duration_ns;
	} header = {
		.magic = 0x494D5837, /* "IMX7" in ASCII */
		.version = 1,
		.width = 4608,
		.height = 2592,
		.format = IMX708_CAPTURE_RAW10,
		.duration_ns = (uint64_t)duration * 1000000000ULL,
	};
	fwrite(&header, sizeof(header), 1, f);

	signal(SIGINT, handle_signal);
	gettimeofday(&start, NULL);

	while (g_running)
	{
		struct imx708_capture_params params;
		struct imx708_frame frame;

		gettimeofday(&now, NULL);
		double elapsed = (now.tv_sec - start.tv_sec) +
						 (now.tv_usec - start.tv_usec) / 1000000.0;
		if (elapsed >= duration)
			break;

		memset(&params, 0, sizeof(params));
		params.format = IMX708_CAPTURE_RAW10;
		params.timeout_ms = 1000;

		ret = imx708_capture_frame(g_handle, &params, &frame);
		if (ret < 0)
		{
			fprintf(stderr, "\nFrame capture error: %s\n",
					imx708_strerror(ret));
			break;
		}

		/* Write frame metadata + data */
		struct
		{
			uint64_t timestamp_ns;
			uint32_t frame_number;
			uint32_t size;
			uint32_t gain;
			uint32_t exposure;
		} fhdr = {
			.timestamp_ns = (uint64_t)(elapsed * 1e9),
			.frame_number = frame_count,
			.size = frame.size,
			.gain = frame.gain,
			.exposure = frame.exposure,
		};
		fwrite(&fhdr, sizeof(fhdr), 1, f);
		fwrite(frame.data, 1, frame.size, f);

		frame_count++;
		imx708_frame_free(&frame);

		printf("\r  Frame %lu (%.1f fps)", frame_count,
			   frame_count / elapsed);
		fflush(stdout);
	}

	fclose(f);
	printf("\nRecorded %lu frames to %s\n", frame_count, filepath);
	return 0;
}

/* ------------------------------------------------------------------ */
/* Profile save/load                                                   */
/* ------------------------------------------------------------------ */

static int cmd_profile(int argc, char *argv[])
{
	const char *filepath = argc > 0 ? argv[0] : "imx708_profile.json";
	struct imx708_profile profile;
	int ret;

	memset(&profile, 0, sizeof(profile));
	snprintf(profile.name, sizeof(profile.name), "default");

	ret = imx708_profile_save(g_handle, &profile);
	if (ret < 0)
	{
		fprintf(stderr, "Profile save failed: %s\n", imx708_strerror(ret));
		return 1;
	}

	ret = imx708_profile_to_file(&profile, filepath);
	if (ret < 0)
	{
		fprintf(stderr, "File write failed: %s\n", imx708_strerror(ret));
		return 1;
	}

	printf("Profile saved to %s\n", filepath);
	return 0;
}

static int cmd_apply(int argc, char *argv[])
{
	const char *filepath = argc > 0 ? argv[0] : "imx708_profile.json";
	struct imx708_profile profile;
	int ret;

	ret = imx708_profile_from_file(filepath, &profile);
	if (ret < 0)
	{
		fprintf(stderr, "File read failed: %s\n", imx708_strerror(ret));
		return 1;
	}

	ret = imx708_profile_load(g_handle, &profile);
	if (ret < 0)
	{
		fprintf(stderr, "Profile apply failed: %s\n", imx708_strerror(ret));
		return 1;
	}

	printf("Profile '%s' applied from %s\n", profile.name, filepath);
	return 0;
}

/* ------------------------------------------------------------------ */
/* Sensor info                                                         */
/* ------------------------------------------------------------------ */

static int cmd_info(void)
{
	struct imx708_sensor_status st;
	uint32_t num_modes;
	int ret;

	ret = imx708_get_status(g_handle, &st);
	if (ret < 0)
	{
		fprintf(stderr, "Status failed: %s\n", imx708_strerror(ret));
		return 1;
	}

	ret = imx708_get_num_modes(g_handle, &num_modes);
	if (ret < 0)
	{
		fprintf(stderr, "Failed to get modes: %s\n", imx708_strerror(ret));
		return 1;
	}

	printf("IMX708 Sensor Information\n");
	printf("=========================\n");
	printf("  Temperature:    %d C\n", st.temperature);
	printf("  Frame Count:    %u\n", st.frame_count);
	printf("  PLL Locked:     %s\n", st.pll_locked ? "yes" : "no");
	printf("  Streaming:      %s\n", st.streaming ? "yes" : "no");
	printf("  Error:          %s\n", st.error ? "yes" : "no");
	printf("  Available Modes: %u\n", num_modes);

	for (uint32_t i = 0; i < num_modes && i < 20; i++)
	{
		struct imx708_mode_info info;
		ret = imx708_get_mode_info(g_handle, i, &info);
		if (ret == 0)
		{
			printf("    [%u] %ux%u @ %u fps\n",
				   i, info.width, info.height, info.fps);
		}
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* Monitor                                                             */
/* ------------------------------------------------------------------ */

static int cmd_monitor(void)
{
	printf("Monitoring sensor (Ctrl+C to stop)...\n\n");
	printf("%-8s %-8s %-8s %-8s %-8s %-8s\n",
		   "Time", "Temp(C)", "Frames", "PLL", "Stream", "Error");
	printf("--------------------------------------------------------\n");

	signal(SIGINT, handle_signal);

	while (g_running)
	{
		struct imx708_sensor_status st;
		int ret = imx708_get_status(g_handle, &st);
		if (ret == 0)
		{
			time_t now = time(NULL);
			struct tm *tm = localtime(&now);
			printf("%02d:%02d:%02d %-8d %-8u %-8s %-8s %-8s\n",
				   tm->tm_hour, tm->tm_min, tm->tm_sec,
				   st.temperature, st.frame_count,
				   st.pll_locked ? "locked" : "unlock",
				   st.streaming ? "on" : "off",
				   st.error ? "ERR" : "OK");
		}
		sleep(1);
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* Auto-exposure loop                                                  */
/* ------------------------------------------------------------------ */

static int cmd_ae(void)
{
	printf("Auto-exposure loop (Ctrl+C to stop)...\n\n");

	signal(SIGINT, handle_signal);

	imx708_ae_configure(g_handle, 128, 5, 100, 10000, 0x10, 0xFFF);

	while (g_running)
	{
		int ret = imx708_ae_run(g_handle);
		if (ret < 0)
		{
			fprintf(stderr, "AE error: %s\n", imx708_strerror(ret));
			break;
		}

		struct imx708_sensor_status st;
		if (imx708_get_status(g_handle, &st) == 0)
		{
			printf("\rTemp: %d C  Frames: %u  PLL: %s",
				   st.temperature, st.frame_count,
				   st.pll_locked ? "locked" : "unlock");
			fflush(stdout);
		}

		usleep(100000); /* 100ms */
	}

	printf("\n");
	return 0;
}

/* ------------------------------------------------------------------ */
/* Help                                                                */
/* ------------------------------------------------------------------ */

static void cmd_help(void)
{
	printf("IMX708 Capture & Recording Tool\n");
	printf("===============================\n\n");
	printf("Usage: imx708_capture <device> <command> [args...]\n\n");
	printf("Commands:\n");
	printf("  snap [file]       - Capture a single frame (default: capture.pgm)\n");
	printf("  burst <n> [dir]   - Capture N frames to directory (default: 10, .)\n");
	printf("  record <sec> [f]  - Record video for N seconds (default: 10, recording.raw)\n");
	printf("  profile [file]    - Save current config as JSON profile\n");
	printf("  apply [file]      - Load and apply a JSON profile\n");
	printf("  info              - Show sensor information\n");
	printf("  monitor           - Monitor sensor status continuously\n");
	printf("  ae                - Run auto-exposure loop\n");
	printf("  help              - Show this help\n");
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
	const char *devpath;
	const char *cmd;
	int ret;

	if (argc < 3)
	{
		cmd_help();
		return 1;
	}

	devpath = argv[1];
	cmd = argv[2];

	ret = imx708_open(devpath, &g_handle);
	if (ret < 0)
	{
		fprintf(stderr, "Failed to open %s: %s\n",
				devpath, imx708_strerror(ret));
		return 1;
	}

	if (strcmp(cmd, "snap") == 0)
		ret = cmd_snap(argc - 3, argv + 3);
	else if (strcmp(cmd, "burst") == 0)
		ret = cmd_burst(argc - 3, argv + 3);
	else if (strcmp(cmd, "record") == 0)
		ret = cmd_record(argc - 3, argv + 3);
	else if (strcmp(cmd, "profile") == 0)
		ret = cmd_profile(argc - 3, argv + 3);
	else if (strcmp(cmd, "apply") == 0)
		ret = cmd_apply(argc - 3, argv + 3);
	else if (strcmp(cmd, "info") == 0)
		ret = cmd_info();
	else if (strcmp(cmd, "monitor") == 0)
		ret = cmd_monitor();
	else if (strcmp(cmd, "ae") == 0)
		ret = cmd_ae();
	else if (strcmp(cmd, "help") == 0)
	{
		cmd_help();
		ret = 0;
	}
	else
	{
		fprintf(stderr, "Unknown command: %s\n", cmd);
		cmd_help();
		ret = 1;
	}

	imx708_close(g_handle);
	return ret;
}
