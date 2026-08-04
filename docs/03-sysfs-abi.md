# Sysfs ABI

All attributes are under `/sys/devices/platform/.../imx708/` or
`/sys/bus/i2c/devices/.../`.

## Attributes

### `chip_id` (RO)

Sensor chip identification register.

```
What:		/sys/.../chip_id
Date:		January 2026
KernelVersion:	6.6
Contact:	sandesh@soccentric.com
Description:	Read-only. Returns the sensor chip ID in hexadecimal.
		Expected value: 0x0708 for IMX708.
```

### `chip_version` (RO)

Sensor chip version/revision register.

```
What:		/sys/.../chip_version
Date:		January 2026
KernelVersion:	6.6
Contact:	sandesh@soccentric.com
Description:	Read-only. Returns the sensor chip version in hexadecimal.
```

### `temperature` (RO)

Sensor die temperature.

```
What:		/sys/.../temperature
Date:		January 2026
KernelVersion:	6.6
Contact:	sandesh@soccentric.com
Description:	Read-only. Returns the sensor die temperature in degrees
		Celsius. The value is signed.
```

### `streaming` (RW)

Start or stop sensor streaming.

```
What:		/sys/.../streaming
Date:		January 2026
KernelVersion:	6.6
Contact:	sandesh@soccentric.com
Description:	Read-write. Write "1" or "on" to start streaming,
		"0" or "off" to stop. Read returns "1" if streaming,
		"0" if in standby.
```

### `mode` (RO)

Current sensor mode.

```
What:		/sys/.../mode
Date:		January 2026
KernelVersion:	6.6
Contact:	sandesh@soccentric.com
Description:	Read-only. Returns the current sensor mode as
		"WxH @ FPS fps".
```

### `gain` (RW)

Analog gain.

```
What:		/sys/.../gain
Date:		January 2026
KernelVersion:	6.6
Contact:	sandesh@soccentric.com
Description:	Read-write. Analog gain in sensor-specific units.
		Range: 0x0000-0xFFFF. Write in decimal or hex (0x prefix).
```

### `exposure` (RW)

Exposure time.

```
What:		/sys/.../exposure
Date:		January 2026
KernelVersion:	6.6
Contact:	sandesh@soccentric.com
Description:	Read-write. Exposure time in line units.
		Range: 1-0xFFFFF.
```

### `test_pattern` (RW)

Test pattern selection.

```
What:		/sys/.../test_pattern
Date:		January 2026
KernelVersion:	6.6
Contact:	sandesh@soccentric.com
Description:	Read-write. Test pattern mode:
		0 = Disabled
		1 = Color bars
		2 = Solid color
		3 = Checkerboard
		4 = Walking 1s
```

### `pll_locked` (RO)

PLL lock status.

```
What:		/sys/.../pll_locked
Date:		January 2026
KernelVersion:	6.6
Contact:	sandesh@soccentric.com
Description:	Read-only. Returns 1 if PLL is locked, 0 otherwise.
```

### `frame_count` (RO)

Frame counter.

```
What:		/sys/.../frame_count
Date:		January 2026
KernelVersion:	6.6
Contact:	sandesh@soccentric.com
Description:	Read-only. Returns the sensor frame counter since
		last stream start.
```

### `hdr_mode` (RW)

HDR mode.

```
What:		/sys/.../hdr_mode
Date:		January 2026
KernelVersion:	6.6
Contact:	sandesh@soccentric.com
Description:	Read-write. HDR mode:
		0 = Off
		1 = Line-interleaved HDR
		2 = DOL (Digital Overlap) HDR
		TODO(HW): Implementation pending register map verification.
```
