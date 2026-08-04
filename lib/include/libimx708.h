/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * libimx708.h - Public API for IMX708 camera sensor control
 *
 * Copyright (C) 2026 SoC Centric
 *
 * Author: Sandesh <sandesh@soccentric.com>
 *
 * This is the public API for application developers. It hides the ioctl
 * encoding, sysfs paths, and /dev node discovery behind a clean C API.
 *
 * Thread safety: All functions are thread-safe unless explicitly documented
 * otherwise. The handle is internally synchronized.
 *
 * Usage example:
 * @code
 *   struct imx708_handle *h;
 *   int ret;
 *
 *   ret = imx708_open("/dev/imx7080", &h);
 *   if (ret < 0) { fprintf(stderr, "open failed: %s\\n", imx708_strerror(ret)); return 1; }
 *
 *   struct imx708_sensor_status st;
 *   imx708_get_status(h, &st);
 *   printf("Temperature: %d C\\n", st.temperature);
 *
 *   imx708_set_gain(h, 0x80, 0x100);
 *   imx708_set_exposure(h, 1000);
 *
 *   imx708_start_stream(h);
 *   sleep(5);
 *   imx708_stop_stream(h);
 *
 *   imx708_close(h);
 * @endcode
 */

#ifndef _LIBIMX708_H_
#define _LIBIMX708_H_

#include <stdint.h>
#include <stddef.h>
#include "imx708_uapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Visibility attribute for public API functions */
#if defined(__GNUC__) && __GNUC__ >= 4
#define IMX708_API __attribute__((visibility("default")))
#else
#define IMX708_API
#endif

/* Opaque handle type */
struct imx708_handle;

/*
 * All sensor data structures are defined in imx708_uapi.h (included above).
 * This header provides the public API functions only.
 */

/**
 * imx708_open - Open an IMX708 sensor device
 * @path:  Path to the device node (e.g., "/dev/imx7080")
 * @handle: On success, points to the allocated handle
 *
 * Returns 0 on success, or a negative errno on failure.
 * The handle must be freed with imx708_close().
 */
IMX708_API int imx708_open(const char *path, struct imx708_handle **handle);

/**
 * imx708_close - Close an IMX708 sensor device
 * @handle: Handle returned by imx708_open()
 *
 * Frees all resources associated with the handle.
 * Safe to call with NULL handle.
 */
IMX708_API void imx708_close(struct imx708_handle *handle);

/**
 * imx708_get_num_modes - Get the number of available sensor modes
 * @handle:    Open sensor handle
 * @num_modes: On success, set to the number of modes
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_get_num_modes(struct imx708_handle *handle, uint32_t *num_modes);

/**
 * imx708_get_mode_info - Get information about a specific sensor mode
 * @handle: Open sensor handle
 * @index:  Mode index (0 to num_modes-1)
 * @info:   On success, filled with mode information
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_get_mode_info(struct imx708_handle *handle, uint32_t index,
			  struct imx708_mode_info *info);

/**
 * imx708_get_status - Get live sensor status
 * @handle: Open sensor handle
 * @status: On success, filled with current sensor status
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_get_status(struct imx708_handle *handle,
		       struct imx708_sensor_status *status);

/**
 * imx708_set_gain - Set analog and digital gain
 * @handle:      Open sensor handle
 * @analog_gain: Analog gain value (sensor-specific units)
 * @digital_gain: Digital gain value (sensor-specific units)
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_set_gain(struct imx708_handle *handle, uint32_t analog_gain,
		     uint32_t digital_gain);

/**
 * imx708_get_gain - Get current gain settings
 * @handle: Open sensor handle
 * @cfg:    On success, filled with current gain values
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_get_gain(struct imx708_handle *handle,
		     struct imx708_gain_config *cfg);

/**
 * imx708_set_exposure - Set exposure time
 * @handle:   Open sensor handle
 * @exposure: Exposure time in line units
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_set_exposure(struct imx708_handle *handle, uint32_t exposure);

/**
 * imx708_get_exposure - Get current exposure settings
 * @handle: Open sensor handle
 * @cfg:    On success, filled with current exposure values
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_get_exposure(struct imx708_handle *handle,
			  struct imx708_exposure_config *cfg);

/**
 * imx708_set_hdr - Set HDR mode
 * @handle: Open sensor handle
 * @cfg:    HDR configuration
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_set_hdr(struct imx708_handle *handle,
		    const struct imx708_hdr_config *cfg);

/**
 * imx708_get_hdr - Get current HDR configuration
 * @handle: Open sensor handle
 * @cfg:    On success, filled with current HDR values
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_get_hdr(struct imx708_handle *handle,
		    struct imx708_hdr_config *cfg);

/**
 * imx708_set_test_pattern - Set test pattern
 * @handle: Open sensor handle
 * @cfg:    Test pattern configuration
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_set_test_pattern(struct imx708_handle *handle,
			     const struct imx708_test_pattern_config *cfg);

/**
 * imx708_get_test_pattern - Get current test pattern
 * @handle: Open sensor handle
 * @cfg:    On success, filled with current test pattern values
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_get_test_pattern(struct imx708_handle *handle,
			     struct imx708_test_pattern_config *cfg);

/**
 * imx708_start_stream - Start sensor streaming
 * @handle: Open sensor handle
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_start_stream(struct imx708_handle *handle);

/**
 * imx708_stop_stream - Stop sensor streaming
 * @handle: Open sensor handle
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_stop_stream(struct imx708_handle *handle);

/**
 * imx708_soft_reset - Perform a software reset of the sensor
 * @handle: Open sensor handle
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_soft_reset(struct imx708_handle *handle);

/**
 * imx708_read_reg - Read a raw sensor register (requires root)
 * @handle: Open sensor handle
 * @reg:    Register address
 * @val:    On success, filled with register value
 *
 * Returns 0 on success, or a negative errno.
 * Requires CAP_SYS_ADMIN (typically root).
 */
IMX708_API int imx708_read_reg(struct imx708_handle *handle, uint32_t reg, uint32_t *val);

/**
 * imx708_write_reg - Write a raw sensor register (requires root)
 * @handle: Open sensor handle
 * @reg:    Register address
 * @val:    Value to write
 *
 * Returns 0 on success, or a negative errno.
 * Requires CAP_SYS_ADMIN (typically root).
 */
IMX708_API int imx708_write_reg(struct imx708_handle *handle, uint32_t reg, uint32_t val);

/**
 * imx708_strerror - Get a human-readable error string
 * @errnum: Negative errno value returned by any imx708_* function
 *
 * Returns a statically allocated string describing the error.
 * The string must not be freed.
 */
IMX708_API const char *imx708_strerror(int errnum);

/* ================================================================== */
/* Frame Capture API                                                   */
/* ================================================================== */

/**
 * Frame capture mode
 */
enum imx708_capture_format {
	IMX708_CAPTURE_RAW10,		/* 10-bit raw Bayer */
	IMX708_CAPTURE_RAW8,		/* 8-bit raw Bayer (compressed) */
	IMX708_CAPTURE_JPEG,		/* JPEG (if ISP available) */
	IMX708_CAPTURE_PNG,		/* PNG (if ISP available) */
	IMX708_CAPTURE_BMP,		/* BMP (if ISP available) */
};

/**
 * Frame capture parameters
 */
struct imx708_capture_params {
	uint32_t	width;		/* output width */
	uint32_t	height;		/* output height */
	uint32_t	format;		/* enum imx708_capture_format */
	uint32_t	quality;	/* JPEG quality (1-100), if applicable */
	uint32_t	num_frames;	/* number of frames to capture (0=continuous) */
	uint32_t	timeout_ms;	/* timeout per frame in ms */
	int		burst_mode;	/* 1 = burst capture, 0 = single */
};

/**
 * Captured frame data
 */
struct imx708_frame {
	void		*data;		/* pixel data */
	size_t		size;		/* data size in bytes */
	uint32_t	width;		/* frame width */
	uint32_t	height;		/* frame height */
	uint32_t	stride;		/* bytes per row */
	uint32_t	format;		/* enum imx708_capture_format */
	uint64_t	timestamp_ns;	/* capture timestamp (nanoseconds) */
	uint32_t	frame_number;	/* sequential frame number */
	uint32_t	gain;		/* gain used for this frame */
	uint32_t	exposure;	/* exposure used for this frame */
};

/**
 * imx708_capture_frame - Capture a single frame
 * @handle:  Open sensor handle
 * @params:  Capture parameters (width, height, format)
 * @frame:   On success, filled with captured frame data
 *
 * Captures a single frame from the sensor. The frame data is allocated
 * by the library and must be freed with imx708_frame_free().
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_capture_frame(struct imx708_handle *handle,
				     const struct imx708_capture_params *params,
				     struct imx708_frame *frame);

/**
 * imx708_capture_frames - Capture multiple frames
 * @handle:     Open sensor handle
 * @params:     Capture parameters
 * @frames:     Output array of captured frames (allocated by library)
 * @num_captured: On success, number of frames actually captured
 *
 * Captures multiple frames in sequence. The frames array and each frame's
 * data must be freed with imx708_frames_free().
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_capture_frames(struct imx708_handle *handle,
				      const struct imx708_capture_params *params,
				      struct imx708_frame **frames,
				      uint32_t *num_captured);

/**
 * imx708_frame_free - Free a single captured frame
 * @frame: Frame to free
 */
IMX708_API void imx708_frame_free(struct imx708_frame *frame);

/**
 * imx708_frames_free - Free an array of captured frames
 * @frames: Array of frames to free
 * @count:  Number of frames in the array
 */
IMX708_API void imx708_frames_free(struct imx708_frame *frames, uint32_t count);

/**
 * imx708_frame_save_pgm - Save a raw frame as PGM/PPM
 * @frame:     Captured frame
 * @filepath:  Output file path
 *
 * Saves a raw Bayer frame as a PGM (grayscale) or PPM (color) image.
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_frame_save_pgm(const struct imx708_frame *frame,
				      const char *filepath);

/**
 * imx708_frame_save_jpeg - Save a frame as JPEG (requires libjpeg)
 * @frame:     Captured frame
 * @filepath:  Output file path
 * @quality:   JPEG quality (1-100)
 *
 * Returns 0 on success, or a negative errno.
 * Returns -ENOSYS if libjpeg is not available.
 */
IMX708_API int imx708_frame_save_jpeg(const struct imx708_frame *frame,
				       const char *filepath, int quality);

/* ================================================================== */
/* Streaming / Callback API                                            */
/* ================================================================== */

/**
 * Frame callback type
 * @frame:     Captured frame (valid only during callback)
 * @user_data: User-provided pointer from imx708_start_streaming()
 */
typedef void (*imx708_frame_callback_t)(const struct imx708_frame *frame,
					 void *user_data);

/**
 * imx708_start_streaming - Start streaming with frame callbacks
 * @handle:    Open sensor handle
 * @params:    Capture parameters
 * @callback:  Callback invoked for each captured frame
 * @user_data: User data passed to callback
 *
 * Starts continuous streaming. Each frame triggers the callback.
 * Call imx708_stop_streaming() to stop.
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_start_streaming(struct imx708_handle *handle,
				       const struct imx708_capture_params *params,
				       imx708_frame_callback_t callback,
				       void *user_data);

/**
 * imx708_stop_streaming - Stop streaming with frame callbacks
 * @handle: Open sensor handle
 *
 * Stops the streaming started by imx708_start_streaming().
 */
IMX708_API void imx708_stop_streaming(struct imx708_handle *handle);

/* ================================================================== */
/* Configuration Profiles                                              */
/* ================================================================== */

/**
 * Sensor configuration profile
 */
struct imx708_profile {
	char		name[64];	/* profile name */
	uint32_t	mode_index;	/* sensor mode index */
	uint32_t	gain;		/* analog gain */
	uint32_t	digital_gain;	/* digital gain */
	uint32_t	exposure;	/* exposure time */
	int		brightness;	/* brightness */
	int		contrast;	/* contrast */
	int		saturation;	/* saturation */
	int		hue;		/* hue */
	int		sharpness;	/* sharpness */
	int		gamma;		/* gamma */
	uint32_t	wb_temperature;	/* white balance temperature */
	int		hdr_mode;	/* HDR mode */
	int		test_pattern;	/* test pattern */
	int		hflip;		/* horizontal flip */
	int		vflip;		/* vertical flip */
};

/**
 * imx708_profile_save - Save current settings as a profile
 * @handle:  Open sensor handle
 * @profile: Profile to save (name must be set)
 *
 * Reads current sensor settings and saves them to the profile.
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_profile_save(struct imx708_handle *handle,
				    struct imx708_profile *profile);

/**
 * imx708_profile_load - Load and apply a profile
 * @handle:  Open sensor handle
 * @profile: Profile to apply
 *
 * Applies all settings from the profile to the sensor.
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_profile_load(struct imx708_handle *handle,
				    const struct imx708_profile *profile);

/**
 * imx708_profile_to_file - Write a profile to a JSON file
 * @profile:  Profile to write
 * @filepath: Output file path
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_profile_to_file(const struct imx708_profile *profile,
				       const char *filepath);

/**
 * imx708_profile_from_file - Read a profile from a JSON file
 * @filepath: Input file path
 * @profile:  On success, filled with the profile
 *
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_profile_from_file(const char *filepath,
					 struct imx708_profile *profile);

/* ================================================================== */
/* Auto-Exposure Helper                                                */
/* ================================================================== */

/**
 * imx708_ae_configure - Configure auto-exposure parameters
 * @handle:      Open sensor handle
 * @target_brightness: Target brightness level (0-255)
 * @ae_speed:    AE convergence speed (1=slow, 10=fast)
 * @exposure_min: Minimum exposure time
 * @exposure_max: Maximum exposure time
 * @gain_min:    Minimum analog gain
 * @gain_max:    Maximum analog gain
 *
 * Configures the auto-exposure algorithm parameters.
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_ae_configure(struct imx708_handle *handle,
				    int target_brightness,
				    int ae_speed,
				    uint32_t exposure_min,
				    uint32_t exposure_max,
				    uint32_t gain_min,
				    uint32_t gain_max);

/**
 * imx708_ae_run - Run one iteration of auto-exposure
 * @handle: Open sensor handle
 *
 * Reads current brightness and adjusts gain/exposure to reach target.
 * Should be called periodically (e.g., every 100ms) when AE is enabled.
 * Returns 0 on success, or a negative errno.
 */
IMX708_API int imx708_ae_run(struct imx708_handle *handle);

#ifdef __cplusplus
}
#endif

#endif /* _LIBIMX708_H_ */
