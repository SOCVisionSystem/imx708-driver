# Porting — Adding a new SoC

## Overview

The IMX708 driver is designed so that adding support for a new SoC or platform
requires changes to only one file: `src/imx708_platform.c`. The core driver
(`imx708_main.c`, `imx708_chardev.c`, etc.) never contains SoC-specific code.

## Step-by-step

### 1. Define register offsets

If the new platform uses different register addresses, add a new
`struct imx708_regs` instance in `include/imx708_regs.h`:

```c
static const struct imx708_regs imx708_regs_myboard = {
    .mode_select    = 0x3000,
    .software_reset = 0x3002,
    /* ... fill in from TRM ... */
};
```

### 2. Implement hardware operations

Create a new `struct imx708_hw_ops` instance in `src/imx708_platform.c`:

```c
static int myboard_hw_init(struct imx708_dev *sensor) { /* ... */ }
static void myboard_hw_deinit(struct imx708_dev *sensor) { /* ... */ }
/* ... implement all ops ... */

static const struct imx708_hw_ops imx708_hw_ops_myboard = {
    .init       = myboard_hw_init,
    .deinit     = myboard_hw_deinit,
    .power_on   = myboard_hw_power_on,
    .power_off  = myboard_hw_power_off,
    .set_mode   = myboard_hw_set_mode,
    .set_gain   = myboard_hw_set_gain,
    .set_exposure = myboard_hw_set_exposure,
    .set_digital_gain = myboard_hw_set_digital_gain,
    .irq_ack    = myboard_hw_irq_ack,
    .quirk_fixup = NULL,
};
```

### 3. Define mode table

```c
static const struct imx708_mode myboard_modes[] = {
    {
        .width      = 4608,
        .height     = 2592,
        .code       = MEDIA_BUS_FMT_SRGGB10_1X10,
        .fps        = 30,
        .hblank     = 0x0C,
        .vblank     = 0x0A,
        .reg_list   = myboard_mode_regs,
        .num_regs   = ARRAY_SIZE(myboard_mode_regs),
    },
};
```

### 4. Create SoC data

```c
const struct imx708_soc_data imx708_soc_myboard = {
    .name       = "Sony IMX708 (MyBoard)",
    .ops        = &imx708_hw_ops_myboard,
    .regmap_cfg = &imx708_regmap_16b,
    .reg        = &imx708_regs_myboard,
    .modes      = myboard_modes,
    .num_modes  = ARRAY_SIZE(myboard_modes),
    .num_channels = 4,
    .clk_names  = myboard_clocks,
    .gpio_names = myboard_gpios,
    .i2c_addr   = 0x1a,
    .quirks     = 0,
};
```

### 5. Add OF match entry

In `src/imx708_main.c`, add to `imx708_of_match[]`:

```c
static const struct of_device_id imx708_of_match[] = {
    { .compatible = "sony,imx708", .data = &imx708_soc_rpi },
    { .compatible = "sony,imx708-myboard", .data = &imx708_soc_myboard },
    { /* sentinel */ }
};
```

### 6. Create device tree overlay

Create `dts/imx708-myboard.dts` with the correct I2C bus, GPIOs, and regulators.

### 7. Build and test

```bash
make PLATFORM=native module
# Or cross-compile for the new platform
```

## TODO work list

Run `grep -rn "TODO(" .` for the complete list. Current entries:

### TODO(HW) — Hardware verification required

| Location | Description |
|---|---|
| `include/imx708_regs.h` | All register offsets — verify against IMX708 datasheet |
| `src/imx708_platform.c` | Mode register sequences — verify against vendor init tables |
| `src/imx708_platform.c` | Gain/exposure register values — verify range and units |
| `src/imx708_pm.c` | Register save/restore list — verify which regs need saving |
| `src/imx708_chardev.c` | HDR mode implementation — register map unknown |

### TODO(DT) — Device tree verification

| Location | Description |
|---|---|
| `dts/imx708-rpi.dts` | GPIO pin numbers — verify against Camera Module 3 schematic |
| `dts/imx708-rpi.dts` | Regulator supplies — verify voltage rails |
| `dts/imx708-rpi.dts` | MIPI data lane mapping — verify against PCB layout |

### TODO(PM) — Power management

| Location | Description |
|---|---|
| `src/imx708_pm.c` | Verify register context save/restore works correctly |
| `src/imx708_pm.c` | Test runtime PM autosuspend behavior |

### TODO(PERF) — Performance

| Location | Description |
|---|---|
| `src/imx708_platform.c` | Optimize mode switch timing |
| `src/imx708_irq.c` | Measure IRQ latency under load |
