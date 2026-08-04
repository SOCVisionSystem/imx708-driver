# IOCTL API

The `/dev/imx708*` character device provides sensor-specific controls beyond
the standard V4L2 interface. All ioctls use magic number `0x49` ('I').

## IOCTL reference

### `IMX708_GET_NUM_MODES`

```
IOCTL:  _IOR('I', 1, __u32)
Input:  none
Output: __u32 (number of available modes)
Errors: none
```

Returns the number of sensor modes supported.

### `IMX708_GET_MODE_INFO`

```
IOCTL:  _IOWR('I', 2, struct imx708_mode_info)
Input:  __u32 index (mode index)
Output: struct imx708_mode_info
Errors: -EINVAL (index out of range)
```

Returns information about a specific sensor mode.

```c
struct imx708_mode_info {
    __u32 width;       /* Active pixel width */
    __u32 height;      /* Active pixel height */
    __u32 code;        /* MEDIA_BUS_FMT_* code */
    __u32 fps;         /* Frames per second */
    __u32 hblank;      /* Horizontal blanking (pixels) */
    __u32 vblank;      /* Vertical blanking (lines) */
    __u32 bit_depth;   /* Bits per pixel */
    __u32 __pad;
};
```

### `IMX708_GET_STATUS`

```
IOCTL:  _IOR('I', 3, struct imx708_sensor_status)
Input:  none
Output: struct imx708_sensor_status
Errors: -EIO (I2C error)
```

Returns live sensor status.

```c
struct imx708_sensor_status {
    __s32 temperature;    /* Die temperature in °C */
    __u32 frame_count;    /* Frame counter */
    __u8  pll_locked;     /* 1 = locked */
    __u8  streaming;      /* 1 = streaming */
    __u8  error;          /* 1 = error condition */
    __u8  __pad[5];
};
```

### `IMX708_SET_GAIN`

```
IOCTL:  _IOW('I', 4, struct imx708_gain_config)
Input:  struct imx708_gain_config
Output: none
Errors: -EIO, -EINVAL
```

Sets analog and digital gain.

```c
struct imx708_gain_config {
    __u32 analog_gain;
    __u32 digital_gain;
    __u32 analog_gain_r;    /* 0 = auto */
    __u32 analog_gain_gr;   /* 0 = auto */
    __u32 analog_gain_gb;   /* 0 = auto */
    __u32 analog_gain_b;    /* 0 = auto */
};
```

### `IMX708_GET_GAIN`

```
IOCTL:  _IOR('I', 5, struct imx708_gain_config)
Input:  none
Output: struct imx708_gain_config
Errors: -EIO
```

Returns current gain settings.

### `IMX708_SET_EXPOSURE`

```
IOCTL:  _IOW('I', 6, struct imx708_exposure_config)
Input:  struct imx708_exposure_config
Output: none
Errors: -EIO, -EINVAL
```

Sets exposure time.

```c
struct imx708_exposure_config {
    __u32 exposure;
    __u32 exposure_r;     /* 0 = same as exposure */
    __u32 exposure_gr;    /* 0 = same */
    __u32 exposure_gb;    /* 0 = same */
    __u32 exposure_b;     /* 0 = same */
};
```

### `IMX708_GET_EXPOSURE`

```
IOCTL:  _IOR('I', 7, struct imx708_exposure_config)
Input:  none
Output: struct imx708_exposure_config
Errors: -EIO
```

### `IMX708_SET_HDR`

```
IOCTL:  _IOW('I', 8, struct imx708_hdr_config)
Input:  struct imx708_hdr_config
Output: none
Errors: -EINVAL, -EIO
```

Sets HDR mode.

```c
struct imx708_hdr_config {
    __u32 mode;          /* 0=off, 1=line-interleaved, 2=DOL */
    __u32 ratio;         /* Long/short exposure ratio */
    __u32 exposure_s;    /* Short exposure (0=auto) */
    __u32 gain_s;        /* Short gain (0=auto) */
};
```

### `IMX708_GET_HDR`

```
IOCTL:  _IOR('I', 9, struct imx708_hdr_config)
Input:  none
Output: struct imx708_hdr_config
Errors: -EIO
```

### `IMX708_SET_TEST_PATTERN`

```
IOCTL:  _IOW('I', 10, struct imx708_test_pattern_config)
Input:  struct imx708_test_pattern_config
Output: none
Errors: -EINVAL, -EIO
```

```c
struct imx708_test_pattern_config {
    __u32 pattern;     /* 0=off, 1=color bars, 2=solid, 3=checker, 4=walking */
    __u32 color;      /* Solid color value */
    __u32 brightness;  /* 0-255 */
};
```

### `IMX708_GET_TEST_PATTERN`

```
IOCTL:  _IOR('I', 11, struct imx708_test_pattern_config)
Input:  none
Output: struct imx708_test_pattern_config
Errors: -EIO
```

### `IMX708_START_STREAM`

```
IOCTL:  _IO('I', 12)
Input:  none
Output: none
Errors: -EIO, -EBUSY
```

Starts sensor streaming. Returns -EBUSY if already streaming.

### `IMX708_STOP_STREAM`

```
IOCTL:  _IO('I', 13)
Input:  none
Output: none
Errors: none
```

Stops sensor streaming. Safe to call when not streaming.

### `IMX708_SOFT_RESET`

```
IOCTL:  _IO('I', 14)
Input:  none
Output: none
Errors: -EIO
```

Performs a software reset of the sensor.

### `IMX708_READ_REG` (debug, requires CAP_SYS_ADMIN)

```
IOCTL:  _IOWR('I', 15, struct imx708_reg_access)
Input:  struct imx708_reg_access.reg
Output: struct imx708_reg_access.val
Errors: -EPERM, -EIO
```

Reads a raw sensor register. Requires root privileges.

### `IMX708_WRITE_REG` (debug, requires CAP_SYS_ADMIN)

```
IOCTL:  _IOW('I', 16, struct imx708_reg_access)
Input:  struct imx708_reg_access (reg + val)
Output: none
Errors: -EPERM, -EIO
```

Writes a raw sensor register. Requires root privileges.

```c
struct imx708_reg_access {
    __u32 reg;
    __u32 val;
};
```
