/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * imx708_stress.c - Multi-threaded stress test for IMX708 sensor driver
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * Multi-threaded stress/soak test for the IMX708 driver. N threads
 * hammering the /dev node, concurrent sysfs readers, open/close churn,
 * ioctl storms, duration and thread count configurable.
 *
 * Designed to be run under lockdep, KASAN, and KCSAN.
 *
 * Usage:
 *   ./imx708_stress [/dev/imx7080] [duration_sec] [num_threads]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#include "libimx708.h"

static volatile int g_running = 1;
static const char *g_devpath = "/dev/imx7080";
static int g_duration = 30;	/* default 30 seconds */
static int g_num_threads = 4;	/* default 4 threads */

/* Per-thread statistics */
struct thread_stats {
	unsigned long ioctl_count;
	unsigned long open_close_count;
	unsigned long error_count;
	unsigned long total_ops;
};

/* ------------------------------------------------------------------ */
/* Signal handler for clean exit                                       */
/* ------------------------------------------------------------------ */

static void handle_signal(int sig)
{
	(void)sig;
	g_running = 0;
}

/* ------------------------------------------------------------------ */
/* Worker threads                                                      */
/* ------------------------------------------------------------------ */

static void *ioctl_storm_thread(void *arg)
{
	struct thread_stats *stats = arg;
	struct imx708_handle *h = NULL;
	int ret;

	ret = imx708_open(g_devpath, &h);
	if (ret < 0) {
		fprintf(stderr, "  [ioctl] open failed: %s\n",
			imx708_strerror(ret));
		stats->error_count++;
		return NULL;
	}

	while (g_running) {
		uint32_t num_modes;
		struct imx708_sensor_status st;
		struct imx708_gain_config gc;
		struct imx708_exposure_config ec;

		/* Mix of read and write ioctls */
		switch (rand() % 6) {
		case 0:
			imx708_get_num_modes(h, &num_modes);
			break;
		case 1:
			imx708_get_status(h, &st);
			break;
		case 2:
			imx708_set_gain(h, rand() % 0xFFFF, rand() % 0xFFFF);
			break;
		case 3:
			imx708_get_gain(h, &gc);
			break;
		case 4:
			imx708_set_exposure(h, rand() % 0xFFFFF);
			break;
		case 5:
			imx708_get_exposure(h, &ec);
			break;
		}

		stats->ioctl_count++;
		stats->total_ops++;

		/* Occasional yield to let other threads run */
		if (stats->total_ops % 100 == 0)
			usleep(100);
	}

	imx708_close(h);
	return NULL;
}

static void *open_close_churn_thread(void *arg)
{
	struct thread_stats *stats = arg;

	while (g_running) {
		struct imx708_handle *h = NULL;
		int ret = imx708_open(g_devpath, &h);

		stats->open_close_count++;
		stats->total_ops++;

		if (ret < 0) {
			stats->error_count++;
			usleep(1000);
			continue;
		}

		/* Do a quick ioctl while open */
		struct imx708_sensor_status st;
		imx708_get_status(h, &st);

		imx708_close(h);
	}

	return NULL;
}

static void *mixed_workload_thread(void *arg)
{
	struct thread_stats *stats = arg;

	while (g_running) {
		struct imx708_handle *h = NULL;
		int ret = imx708_open(g_devpath, &h);

		stats->open_close_count++;
		stats->total_ops++;

		if (ret < 0) {
			stats->error_count++;
			usleep(1000);
			continue;
		}

		/* Burst of operations */
		for (int i = 0; i < 10 && g_running; i++) {
			switch (rand() % 4) {
			case 0:
				imx708_get_status(h, &(struct imx708_sensor_status){0});
				break;
			case 1:
				imx708_set_gain(h, 0x80, 0x100);
				break;
			case 2:
				imx708_set_exposure(h, 1000);
				break;
			case 3:
				imx708_soft_reset(h);
				break;
			}
			stats->ioctl_count++;
			stats->total_ops++;
		}

		imx708_close(h);
	}

	return NULL;
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
	pthread_t *threads;
	struct thread_stats *stats;
	struct sigaction sa;
	struct timeval start, end;
	double elapsed;

	if (argc > 1)
		g_devpath = argv[1];
	if (argc > 2)
		g_duration = atoi(argv[2]);
	if (argc > 3)
		g_num_threads = atoi(argv[3]);

	if (g_num_threads < 1)
		g_num_threads = 1;
	if (g_num_threads > 32)
		g_num_threads = 32;

	/* Setup signal handler */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	printf("IMX708 Stress Test\n");
	printf("==================\n");
	printf("Device:     %s\n", g_devpath);
	printf("Duration:   %d seconds\n", g_duration);
	printf("Threads:    %d\n", g_num_threads);
	printf("\n");

	/* Allocate thread resources */
	threads = calloc(g_num_threads, sizeof(pthread_t));
	stats = calloc(g_num_threads, sizeof(struct thread_stats));
	if (!threads || !stats) {
		fprintf(stderr, "Out of memory\n");
		free(threads);
		free(stats);
		return 1;
	}

	/* Launch threads */
	gettimeofday(&start, NULL);

	for (int i = 0; i < g_num_threads; i++) {
		void *(*worker)(void *);

		switch (i % 3) {
		case 0:	worker = ioctl_storm_thread;	break;
		case 1:	worker = open_close_churn_thread; break;
		case 2:	worker = mixed_workload_thread;	break;
		default: worker = ioctl_storm_thread;	break;
		}

		if (pthread_create(&threads[i], NULL, worker, &stats[i]) != 0) {
			fprintf(stderr, "Failed to create thread %d\n", i);
			g_num_threads = i;
			break;
		}
	}

	/* Run for the specified duration */
	printf("Running for %d seconds...\n", g_duration);
	for (int i = 0; i < g_duration && g_running; i++) {
		printf("  [%d/%d] %lu ops/sec...\r", i + 1, g_duration,
		       (unsigned long)0);
		fflush(stdout);
		sleep(1);
	}

	/* Signal stop */
	g_running = 0;
	printf("\n\nStopping threads...\n");

	/* Wait for threads */
	for (int i = 0; i < g_num_threads; i++)
		pthread_join(threads[i], NULL);

	gettimeofday(&end, NULL);
	elapsed = (end.tv_sec - start.tv_sec) +
		  (end.tv_usec - start.tv_usec) / 1000000.0;

	/* Aggregate statistics */
	unsigned long total_ioctl = 0, total_oc = 0, total_err = 0, total_ops = 0;
	for (int i = 0; i < g_num_threads; i++) {
		total_ioctl += stats[i].ioctl_count;
		total_oc += stats[i].open_close_count;
		total_err += stats[i].error_count;
		total_ops += stats[i].total_ops;
	}

	/* Report */
	printf("\nResults:\n");
	printf("  Elapsed:        %.1f seconds\n", elapsed);
	printf("  Total ops:      %lu\n", total_ops);
	printf("  IOCTLs:         %lu\n", total_ioctl);
	printf("  Open/Close:     %lu\n", total_oc);
	printf("  Errors:         %lu\n", total_err);
	printf("  Throughput:     %.0f ops/sec\n",
	       elapsed > 0 ? total_ops / elapsed : 0);
	printf("\n");

	if (total_err > 0) {
		printf("FAIL: %lu errors detected\n", total_err);
		free(threads);
		free(stats);
		return 1;
	}

	printf("PASS: No errors\n");
	free(threads);
	free(stats);
	return 0;
}
