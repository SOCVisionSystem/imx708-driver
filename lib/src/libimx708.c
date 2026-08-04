/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * libimx708.c - Userspace library for IMX708 camera sensor control
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * This library provides the application-facing API for the IMX708 sensor.
 * It communicates with the kernel driver through ioctl() on the /dev/imx708*
 * device node. All functions return 0 on success or a negative errno.
 *
 * Thread safety: The handle contains a mutex and is thread-safe.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include "libimx708.h"
#include "imx708_uapi.h"

struct imx708_handle {
	int fd;
	pthread_mutex_t lock;
	void *streaming_data;	/* streaming thread state */
	void *ae_state;		/* auto-exposure state */
};

/* ------------------------------------------------------------------ */
/* Open / Close                                                        */
/* ------------------------------------------------------------------ */

int imx708_open(const char *path, struct imx708_handle **handle)
{
	struct imx708_handle *h;
	int fd;

	if (!path || !handle)
		return -EINVAL;

	fd = open(path, O_RDWR);
	if (fd < 0)
		return -errno;

	h = calloc(1, sizeof(*h));
	if (!h) {
		close(fd);
		return -ENOMEM;
	}

	h->fd = fd;
	pthread_mutex_init(&h->lock, NULL);
	*handle = h;

	return 0;
}

void imx708_close(struct imx708_handle *handle)
{
	if (!handle)
		return;

	pthread_mutex_destroy(&handle->lock);
	close(handle->fd);
	free(handle);
}

/* ------------------------------------------------------------------ */
/* Internal helper                                                     */
/* ------------------------------------------------------------------ */

static int imx708_ioctl(struct imx708_handle *handle,
			 unsigned long cmd, void *arg)
{
	int ret;

	pthread_mutex_lock(&handle->lock);
	ret = ioctl(handle->fd, cmd, arg);
	pthread_mutex_unlock(&handle->lock);

	if (ret < 0)
		return -errno;

	return 0;
}

/* ------------------------------------------------------------------ */
/* Mode queries                                                        */
/* ------------------------------------------------------------------ */

int imx708_get_num_modes(struct imx708_handle *handle, uint32_t *num_modes)
{
	if (!handle || !num_modes)
		return -EINVAL;

	return imx708_ioctl(handle, IMX708_GET_NUM_MODES, num_modes);
}

int imx708_get_mode_info(struct imx708_handle *handle, uint32_t index,
			  struct imx708_mode_info *info)
{
	if (!handle || !info)
		return -EINVAL;

	return imx708_ioctl(handle, IMX708_GET_MODE_INFO, &index);
}

/* ------------------------------------------------------------------ */
/* Status                                                              */
/* ------------------------------------------------------------------ */

int imx708_get_status(struct imx708_handle *handle,
		       struct imx708_sensor_status *status)
{
	if (!handle || !status)
		return -EINVAL;

	return imx708_ioctl(handle, IMX708_GET_STATUS, status);
}

/* ------------------------------------------------------------------ */
/* Gain                                                                */
/* ------------------------------------------------------------------ */

int imx708_set_gain(struct imx708_handle *handle, uint32_t analog_gain,
		     uint32_t digital_gain)
{
	struct imx708_gain_config cfg;

	if (!handle)
		return -EINVAL;

	memset(&cfg, 0, sizeof(cfg));
	cfg.analog_gain = analog_gain;
	cfg.digital_gain = digital_gain;

	return imx708_ioctl(handle, IMX708_SET_GAIN, &cfg);
}

int imx708_get_gain(struct imx708_handle *handle,
		     struct imx708_gain_config *cfg)
{
	if (!handle || !cfg)
		return -EINVAL;

	return imx708_ioctl(handle, IMX708_GET_GAIN, cfg);
}

/* ------------------------------------------------------------------ */
/* Exposure                                                            */
/* ------------------------------------------------------------------ */

int imx708_set_exposure(struct imx708_handle *handle, uint32_t exposure)
{
	struct imx708_exposure_config cfg;

	if (!handle)
		return -EINVAL;

	memset(&cfg, 0, sizeof(cfg));
	cfg.exposure = exposure;

	return imx708_ioctl(handle, IMX708_SET_EXPOSURE, &cfg);
}

int imx708_get_exposure(struct imx708_handle *handle,
			  struct imx708_exposure_config *cfg)
{
	if (!handle || !cfg)
		return -EINVAL;

	return imx708_ioctl(handle, IMX708_GET_EXPOSURE, cfg);
}

/* ------------------------------------------------------------------ */
/* HDR                                                                 */
/* ------------------------------------------------------------------ */

int imx708_set_hdr(struct imx708_handle *handle,
		    const struct imx708_hdr_config *cfg)
{
	if (!handle || !cfg)
		return -EINVAL;

	return imx708_ioctl(handle, IMX708_SET_HDR, (void *)cfg);
}

int imx708_get_hdr(struct imx708_handle *handle,
		    struct imx708_hdr_config *cfg)
{
	if (!handle || !cfg)
		return -EINVAL;

	return imx708_ioctl(handle, IMX708_GET_HDR, cfg);
}

/* ------------------------------------------------------------------ */
/* Test pattern                                                        */
/* ------------------------------------------------------------------ */

int imx708_set_test_pattern(struct imx708_handle *handle,
			     const struct imx708_test_pattern_config *cfg)
{
	if (!handle || !cfg)
		return -EINVAL;

	return imx708_ioctl(handle, IMX708_SET_TEST_PATTERN, (void *)cfg);
}

int imx708_get_test_pattern(struct imx708_handle *handle,
			     struct imx708_test_pattern_config *cfg)
{
	if (!handle || !cfg)
		return -EINVAL;

	return imx708_ioctl(handle, IMX708_GET_TEST_PATTERN, cfg);
}

/* ------------------------------------------------------------------ */
/* Streaming                                                           */
/* ------------------------------------------------------------------ */

int imx708_start_stream(struct imx708_handle *handle)
{
	if (!handle)
		return -EINVAL;

	return imx708_ioctl(handle, IMX708_START_STREAM, NULL);
}

int imx708_stop_stream(struct imx708_handle *handle)
{
	if (!handle)
		return -EINVAL;

	return imx708_ioctl(handle, IMX708_STOP_STREAM, NULL);
}

/* ------------------------------------------------------------------ */
/* Reset                                                               */
/* ------------------------------------------------------------------ */

int imx708_soft_reset(struct imx708_handle *handle)
{
	if (!handle)
		return -EINVAL;

	return imx708_ioctl(handle, IMX708_SOFT_RESET, NULL);
}

/* ------------------------------------------------------------------ */
/* Raw register access                                                  */
/* ------------------------------------------------------------------ */

int imx708_read_reg(struct imx708_handle *handle, uint32_t reg, uint32_t *val)
{
	struct imx708_reg_access ra;

	if (!handle || !val)
		return -EINVAL;

	ra.reg = reg;
	ra.val = 0;

	int ret = imx708_ioctl(handle, IMX708_READ_REG, &ra);
	if (ret == 0)
		*val = ra.val;

	return ret;
}

int imx708_write_reg(struct imx708_handle *handle, uint32_t reg, uint32_t val)
{
	struct imx708_reg_access ra;

	if (!handle)
		return -EINVAL;

	ra.reg = reg;
	ra.val = val;

	return imx708_ioctl(handle, IMX708_WRITE_REG, &ra);
}

/* ------------------------------------------------------------------ */
/* Error strings                                                       */
/* ------------------------------------------------------------------ */

const char *imx708_strerror(int errnum)
{
	switch (errnum) {
	case 0:			return "Success";
	case -EACCES:		return "Permission denied";
	case -EAGAIN:		return "Try again";
	case -EBUSY:		return "Device or resource busy";
	case -EFAULT:		return "Bad address";
	case -EINVAL:		return "Invalid argument";
	case -EIO:		return "I/O error";
	case -ENODEV:		return "No such device";
	case -ENOMEM:		return "Out of memory";
	case -ENOTTY:		return "Not a typewriter (invalid ioctl)";
	case -EPERM:		return "Operation not permitted";
	case -ETIMEDOUT:	return "Operation timed out";
	default:
		if (errnum < 0)
			return strerror(-errnum);
		return strerror(errnum);
	}
}

/* ================================================================== */
/* Frame Capture Implementation                                         */
/* ================================================================== */

int imx708_capture_frame(struct imx708_handle *handle,
			  const struct imx708_capture_params *params,
			  struct imx708_frame *frame)
{
	int ret;
	uint32_t frame_size;

	if (!handle || !params || !frame)
		return -EINVAL;

	/* Start streaming */
	ret = imx708_start_stream(handle);
	if (ret < 0)
		return ret;

	/* Calculate frame size (raw Bayer, 10-bit = 2 bytes per 2 pixels) */
	frame_size = params->width * params->height * 2;
	if (frame_size < 1)
		frame_size = 4608 * 2592 * 2;

	/* Allocate frame buffer */
	frame->data = calloc(1, frame_size);
	if (!frame->data) {
		imx708_stop_stream(handle);
		return -ENOMEM;
	}

	/* Simulate frame capture (in real implementation, this would
	 * read from the V4L2 video device node via mmap/DMA) */
	frame->size = frame_size;
	frame->width = params->width ? params->width : 4608;
	frame->height = params->height ? params->height : 2592;
	frame->stride = frame->width * 2;
	frame->format = params->format;
	frame->timestamp_ns = 0;
	frame->frame_number = 0;

	/* Read current gain/exposure for metadata */
	{
		struct imx708_gain_config gc;
		struct imx708_exposure_config ec;
		if (imx708_get_gain(handle, &gc) == 0)
			frame->gain = gc.analog_gain;
		if (imx708_get_exposure(handle, &ec) == 0)
			frame->exposure = ec.exposure;
	}

	/* Stop streaming */
	imx708_stop_stream(handle);

	return 0;
}

int imx708_capture_frames(struct imx708_handle *handle,
			   const struct imx708_capture_params *params,
			   struct imx708_frame **frames,
			   uint32_t *num_captured)
{
	uint32_t n = params->num_frames ? params->num_frames : 1;
	int ret;

	if (!handle || !params || !frames || !num_captured)
		return -EINVAL;

	*frames = calloc(n, sizeof(struct imx708_frame));
	if (!*frames)
		return -ENOMEM;

	for (uint32_t i = 0; i < n; i++) {
		ret = imx708_capture_frame(handle, params, &(*frames)[i]);
		if (ret < 0) {
			imx708_frames_free(*frames, i);
			*num_captured = i;
			return ret;
		}
		(*frames)[i].frame_number = i;
	}

	*num_captured = n;
	return 0;
}

void imx708_frame_free(struct imx708_frame *frame)
{
	if (frame) {
		free(frame->data);
		frame->data = NULL;
		frame->size = 0;
	}
}

void imx708_frames_free(struct imx708_frame *frames, uint32_t count)
{
	if (frames) {
		for (uint32_t i = 0; i < count; i++)
			free(frames[i].data);
		free(frames);
	}
}

int imx708_frame_save_pgm(const struct imx708_frame *frame,
			   const char *filepath)
{
	FILE *f;
	int ret;

	if (!frame || !frame->data || !filepath)
		return -EINVAL;

	f = fopen(filepath, "wb");
	if (!f)
		return -errno;

	/* Write PGM header (grayscale) */
	ret = fprintf(f, "P5\n%u %u\n65535\n", frame->width, frame->height);
	if (ret < 0) {
		fclose(f);
		return -EIO;
	}

	/* Write pixel data */
	if (fwrite(frame->data, 1, frame->size, f) != frame->size) {
		fclose(f);
		unlink(filepath);
		return -EIO;
	}

	fclose(f);
	return 0;
}

int imx708_frame_save_jpeg(const struct imx708_frame *frame,
			    const char *filepath, int quality)
{
	(void)frame;
	(void)filepath;
	(void)quality;
	/* Requires libjpeg — not linked by default */
	return -ENOSYS;
}

/* ================================================================== */
/* Streaming / Callback API                                             */
/* ================================================================== */

struct streaming_thread_data {
	struct imx708_handle	*handle;
	imx708_frame_callback_t	callback;
	void			*user_data;
	struct imx708_capture_params params;
	volatile int		running;
	pthread_t		thread;
};

static void *streaming_thread(void *arg)
{
	struct streaming_thread_data *st = arg;

	while (st->running) {
		struct imx708_frame frame;
		int ret;

		ret = imx708_capture_frame(st->handle, &st->params, &frame);
		if (ret == 0 && st->callback)
			st->callback(&frame, st->user_data);

		imx708_frame_free(&frame);
	}

	return NULL;
}

int imx708_start_streaming(struct imx708_handle *handle,
			    const struct imx708_capture_params *params,
			    imx708_frame_callback_t callback,
			    void *user_data)
{
	struct streaming_thread_data *st;

	if (!handle || !params || !callback)
		return -EINVAL;

	st = calloc(1, sizeof(*st));
	if (!st)
		return -ENOMEM;

	st->handle = handle;
	st->callback = callback;
	st->user_data = user_data;
	st->params = *params;
	st->running = 1;

	pthread_mutex_lock(&handle->lock);
	handle->streaming_data = st;
	pthread_mutex_unlock(&handle->lock);

	if (pthread_create(&st->thread, NULL, streaming_thread, st) != 0) {
		free(st);
		pthread_mutex_lock(&handle->lock);
		handle->streaming_data = NULL;
		pthread_mutex_unlock(&handle->lock);
		return -errno;
	}

	return 0;
}

void imx708_stop_streaming(struct imx708_handle *handle)
{
	struct streaming_thread_data *st;

	if (!handle)
		return;

	pthread_mutex_lock(&handle->lock);
	st = handle->streaming_data;
	handle->streaming_data = NULL;
	pthread_mutex_unlock(&handle->lock);

	if (st) {
		st->running = 0;
		pthread_join(st->thread, NULL);
		free(st);
	}
}

/* ================================================================== */
/* Configuration Profiles                                              */
/* ================================================================== */

int imx708_profile_save(struct imx708_handle *handle,
			 struct imx708_profile *profile)
{
	struct imx708_gain_config gc;
	struct imx708_exposure_config ec;

	if (!handle || !profile)
		return -EINVAL;

	/* Read current settings */
	if (imx708_get_gain(handle, &gc) == 0) {
		profile->gain = gc.analog_gain;
		profile->digital_gain = gc.digital_gain;
	}
	if (imx708_get_exposure(handle, &ec) == 0)
		profile->exposure = ec.exposure;

	/* Read mode info */
	{
		uint32_t num_modes;
		if (imx708_get_num_modes(handle, &num_modes) == 0)
			profile->mode_index = 0;
	}

	/* Read sensor status for temperature */
	{
		struct imx708_sensor_status st;
		if (imx708_get_status(handle, &st) == 0) {
			/* Use status info if needed */
			(void)st;
		}
	}

	return 0;
}

int imx708_profile_load(struct imx708_handle *handle,
			 const struct imx708_profile *profile)
{
	if (!handle || !profile)
		return -EINVAL;

	/* Apply gain */
	imx708_set_gain(handle, profile->gain, profile->digital_gain);

	/* Apply exposure */
	imx708_set_exposure(handle, profile->exposure);

	return 0;
}

int imx708_profile_to_file(const struct imx708_profile *profile,
			    const char *filepath)
{
	FILE *f;

	if (!profile || !filepath)
		return -EINVAL;

	f = fopen(filepath, "w");
	if (!f)
		return -errno;

	fprintf(f, "{\n");
	fprintf(f, "  \"name\": \"%s\",\n", profile->name);
	fprintf(f, "  \"mode_index\": %u,\n", profile->mode_index);
	fprintf(f, "  \"gain\": %u,\n", profile->gain);
	fprintf(f, "  \"digital_gain\": %u,\n", profile->digital_gain);
	fprintf(f, "  \"exposure\": %u,\n", profile->exposure);
	fprintf(f, "  \"brightness\": %d,\n", profile->brightness);
	fprintf(f, "  \"contrast\": %d,\n", profile->contrast);
	fprintf(f, "  \"saturation\": %d,\n", profile->saturation);
	fprintf(f, "  \"hue\": %d,\n", profile->hue);
	fprintf(f, "  \"sharpness\": %d,\n", profile->sharpness);
	fprintf(f, "  \"gamma\": %d,\n", profile->gamma);
	fprintf(f, "  \"wb_temperature\": %u,\n", profile->wb_temperature);
	fprintf(f, "  \"hdr_mode\": %d,\n", profile->hdr_mode);
	fprintf(f, "  \"test_pattern\": %d,\n", profile->test_pattern);
	fprintf(f, "  \"hflip\": %d,\n", profile->hflip);
	fprintf(f, "  \"vflip\": %d\n", profile->vflip);
	fprintf(f, "}\n");

	fclose(f);
	return 0;
}

int imx708_profile_from_file(const char *filepath,
			      struct imx708_profile *profile)
{
	FILE *f;
	char line[256];

	if (!filepath || !profile)
		return -EINVAL;

	f = fopen(filepath, "r");
	if (!f)
		return -errno;

	memset(profile, 0, sizeof(*profile));

	while (fgets(line, sizeof(line), f)) {
		char key[64], val[256];
		if (sscanf(line, "  \"%63[^\"]\": \"%255[^\"]\",", key, val) == 2) {
			if (strcmp(key, "name") == 0) {
				size_t len = strlen(val);
				if (len >= sizeof(profile->name))
					len = sizeof(profile->name) - 1;
				memcpy(profile->name, val, len);
				profile->name[len] = '\0';
			}
		} else if (sscanf(line, "  \"%63[^\"]\": %255[^,\n]", key, val) == 2) {
			char *end;
			long v = strtol(val, &end, 10);
			if (end != val) {
				if (strcmp(key, "mode_index") == 0) profile->mode_index = v;
				else if (strcmp(key, "gain") == 0) profile->gain = v;
				else if (strcmp(key, "digital_gain") == 0) profile->digital_gain = v;
				else if (strcmp(key, "exposure") == 0) profile->exposure = v;
				else if (strcmp(key, "brightness") == 0) profile->brightness = v;
				else if (strcmp(key, "contrast") == 0) profile->contrast = v;
				else if (strcmp(key, "saturation") == 0) profile->saturation = v;
				else if (strcmp(key, "hue") == 0) profile->hue = v;
				else if (strcmp(key, "sharpness") == 0) profile->sharpness = v;
				else if (strcmp(key, "gamma") == 0) profile->gamma = v;
				else if (strcmp(key, "wb_temperature") == 0) profile->wb_temperature = v;
				else if (strcmp(key, "hdr_mode") == 0) profile->hdr_mode = v;
				else if (strcmp(key, "test_pattern") == 0) profile->test_pattern = v;
				else if (strcmp(key, "hflip") == 0) profile->hflip = v;
				else if (strcmp(key, "vflip") == 0) profile->vflip = v;
			}
		}
	}

	fclose(f);
	return 0;
}

/* ================================================================== */
/* Auto-Exposure Helper                                                 */
/* ================================================================== */

struct ae_state {
	int		target_brightness;
	int		ae_speed;
	uint32_t	exposure_min;
	uint32_t	exposure_max;
	uint32_t	gain_min;
	uint32_t	gain_max;
	int		last_brightness;
};

int imx708_ae_configure(struct imx708_handle *handle,
			  int target_brightness,
			  int ae_speed,
			  uint32_t exposure_min,
			  uint32_t exposure_max,
			  uint32_t gain_min,
			  uint32_t gain_max)
{
	struct ae_state *ae;

	if (!handle)
		return -EINVAL;

	ae = calloc(1, sizeof(*ae));
	if (!ae)
		return -ENOMEM;

	ae->target_brightness = target_brightness;
	ae->ae_speed = ae_speed > 0 ? ae_speed : 5;
	ae->exposure_min = exposure_min;
	ae->exposure_max = exposure_max;
	ae->gain_min = gain_min;
	ae->gain_max = gain_max;
	ae->last_brightness = 128;

	pthread_mutex_lock(&handle->lock);
	if (handle->ae_state)
		free(handle->ae_state);
	handle->ae_state = ae;
	pthread_mutex_unlock(&handle->lock);

	return 0;
}

int imx708_ae_run(struct imx708_handle *handle)
{
	struct ae_state *ae;
	int brightness = 128;
	int error;
	uint32_t gain, exposure;
	float kp;

	if (!handle)
		return -EINVAL;

	pthread_mutex_lock(&handle->lock);
	ae = handle->ae_state;
	pthread_mutex_unlock(&handle->lock);

	if (!ae)
		return -EINVAL;

	/* Read current brightness (from sensor status or simulated) */
	{
		struct imx708_sensor_status st;
		if (imx708_get_status(handle, &st) == 0)
			brightness = 128 - st.temperature / 4;
	}

	/* Proportional control */
	error = ae->target_brightness - brightness;
	kp = ae->ae_speed / 10.0f;

	/* Read current gain/exposure */
	{
		struct imx708_gain_config gc;
		struct imx708_exposure_config ec;
		if (imx708_get_gain(handle, &gc) == 0)
			gain = gc.analog_gain;
		else
			gain = 0x80;
		if (imx708_get_exposure(handle, &ec) == 0)
			exposure = ec.exposure;
		else
			exposure = 1000;
	}

	/* Adjust exposure first, then gain */
	if (error > 0) {
		/* Too dark — increase exposure/gain */
		exposure += (uint32_t)(error * kp * 10);
		if (exposure > ae->exposure_max) {
			exposure = ae->exposure_max;
			gain += (uint32_t)(error * kp);
			if (gain > ae->gain_max)
				gain = ae->gain_max;
		}
	} else if (error < 0) {
		/* Too bright — decrease gain first, then exposure */
		if (gain > ae->gain_min) {
			int32_t dg = (int32_t)(-error * kp);
			if (dg > (int32_t)(gain - ae->gain_min))
				gain = ae->gain_min;
			else
				gain -= dg;
		}
		if (gain <= ae->gain_min && exposure > ae->exposure_min) {
			exposure -= (uint32_t)(-error * kp * 10);
			if (exposure < ae->exposure_min)
				exposure = ae->exposure_min;
		}
	}

	/* Clamp values */
	if (exposure < ae->exposure_min) exposure = ae->exposure_min;
	if (exposure > ae->exposure_max) exposure = ae->exposure_max;
	if (gain < ae->gain_min) gain = ae->gain_min;
	if (gain > ae->gain_max) gain = ae->gain_max;

	/* Apply */
	imx708_set_gain(handle, gain, 0x100);
	imx708_set_exposure(handle, exposure);

	ae->last_brightness = brightness;
	return 0;
}
