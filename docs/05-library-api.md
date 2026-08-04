# Library API

## Overview

`libimx708` provides a C API for controlling the IMX708 sensor. It communicates
with the kernel driver through ioctl() on the `/dev/imx708*` device node.

All functions return 0 on success or a negative errno on failure. Use
`imx708_strerror()` to get a human-readable error string.

## Thread safety

The library is thread-safe. Each handle has an internal mutex that serializes
concurrent calls. For maximum throughput, use separate handles in each thread.

## Example program

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "libimx708.h"

int main(void)
{
    struct imx708_handle *h;
    int ret;

    /* Open the sensor */
    ret = imx708_open("/dev/imx7080", &h);
    if (ret < 0) {
        fprintf(stderr, "Failed to open: %s\n", imx708_strerror(ret));
        return 1;
    }

    /* Query available modes */
    uint32_t num_modes;
    imx708_get_num_modes(h, &num_modes);
    printf("Sensor has %u modes\n", num_modes);

    /* Get sensor status */
    struct imx708_sensor_status st;
    imx708_get_status(h, &st);
    printf("Temperature: %d C\n", st.temperature);
    printf("PLL: %s\n", st.pll_locked ? "locked" : "unlocked");

    /* Set gain and exposure */
    imx708_set_gain(h, 0x80, 0x100);
    imx708_set_exposure(h, 1000);

    /* Start streaming */
    printf("Starting stream...\n");
    imx708_start_stream(h);
    sleep(5);
    imx708_stop_stream(h);
    printf("Stream stopped\n");

    /* Clean up */
    imx708_close(h);
    return 0;
}
```

Compile with:

```bash
gcc -o example example.c -limx708
# Or with pkg-config:
gcc -o example example.c $(pkg-config --cflags --libs libimx708)
```

## API reference

### `imx708_open()`

```c
int imx708_open(const char *path, struct imx708_handle **handle);
```

Opens an IMX708 sensor device.

- `path`: Device node path (e.g., `/dev/imx7080`)
- `handle`: On success, points to the allocated handle
- Returns: 0 on success, negative errno on failure
- Thread safety: Not thread-safe (caller must serialize opens)

### `imx708_close()`

```c
void imx708_close(struct imx708_handle *handle);
```

Closes the sensor and frees all resources. Safe to call with NULL.

### `imx708_get_num_modes()`

```c
int imx708_get_num_modes(struct imx708_handle *handle, uint32_t *num_modes);
```

Returns the number of available sensor modes.

### `imx708_get_mode_info()`

```c
int imx708_get_mode_info(struct imx708_handle *handle, uint32_t index,
                          struct imx708_mode_info *info);
```

Returns information about a specific mode.

### `imx708_get_status()`

```c
int imx708_get_status(struct imx708_handle *handle,
                       struct imx708_sensor_status *status);
```

Returns live sensor status (temperature, frame count, PLL, streaming state).

### `imx708_set_gain()`

```c
int imx708_set_gain(struct imx708_handle *handle, uint32_t analog_gain,
                     uint32_t digital_gain);
```

Sets analog and digital gain.

### `imx708_get_gain()`

```c
int imx708_get_gain(struct imx708_handle *handle,
                     struct imx708_gain_config *cfg);
```

Returns current gain settings.

### `imx708_set_exposure()`

```c
int imx708_set_exposure(struct imx708_handle *handle, uint32_t exposure);
```

Sets exposure time in line units.

### `imx708_get_exposure()`

```c
int imx708_get_exposure(struct imx708_handle *handle,
                          struct imx708_exposure_config *cfg);
```

Returns current exposure settings.

### `imx708_set_hdr()`

```c
int imx708_set_hdr(struct imx708_handle *handle,
                    const struct imx708_hdr_config *cfg);
```

Sets HDR mode. TODO(HW): Implementation pending register map verification.

### `imx708_get_hdr()`

```c
int imx708_get_hdr(struct imx708_handle *handle,
                    struct imx708_hdr_config *cfg);
```

Returns current HDR configuration.

### `imx708_set_test_pattern()`

```c
int imx708_set_test_pattern(struct imx708_handle *handle,
                             const struct imx708_test_pattern_config *cfg);
```

Sets test pattern mode.

### `imx708_get_test_pattern()`

```c
int imx708_get_test_pattern(struct imx708_handle *handle,
                             struct imx708_test_pattern_config *cfg);
```

Returns current test pattern.

### `imx708_start_stream()`

```c
int imx708_start_stream(struct imx708_handle *handle);
```

Starts sensor streaming.

### `imx708_stop_stream()`

```c
int imx708_stop_stream(struct imx708_handle *handle);
```

Stops sensor streaming.

### `imx708_soft_reset()`

```c
int imx708_soft_reset(struct imx708_handle *handle);
```

Performs a software reset of the sensor.

### `imx708_read_reg()` (debug)

```c
int imx708_read_reg(struct imx708_handle *handle, uint32_t reg, uint32_t *val);
```

Reads a raw sensor register. Requires root privileges.

### `imx708_write_reg()` (debug)

```c
int imx708_write_reg(struct imx708_handle *handle, uint32_t reg, uint32_t val);
```

Writes a raw sensor register. Requires root privileges.

### `imx708_strerror()`

```c
const char *imx708_strerror(int errnum);
```

Returns a human-readable error string for a negative errno value.
