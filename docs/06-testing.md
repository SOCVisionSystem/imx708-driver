# Testing

## Test suite

```bash
# Run the pass/fail test suite
./build/native/test/imx708_test [/dev/imx7080]

# Expected output:
#   IMX708 Sensor Test Suite
#   ========================
#   Device: /dev/imx7080
#
#     TEST 1: open/close ... PASS
#     TEST 2: double close ... PASS
#     ...
#   Results: 13/13 passed, 0 failed
```

## Interactive CLI

```bash
# Show sensor status
./build/native/test/imx708_cli /dev/imx7080 status

# List available modes
./build/native/test/imx708_cli /dev/imx7080 modes

# Set gain
./build/native/test/imx708_cli /dev/imx7080 gain 0x80

# Set exposure
./build/native/test/imx708_cli /dev/imx7080 exposure 1000

# Start streaming
./build/native/test/imx708_cli /dev/imx7080 stream on

# Monitor continuously
./build/native/test/imx708_cli /dev/imx7080 monitor
```

## Stress test

```bash
# Run stress test (30 seconds, 4 threads)
./build/native/test/imx708_stress [/dev/imx7080] [duration_sec] [num_threads]

# Example: 60 seconds with 8 threads
./build/native/test/imx708_stress /dev/imx7080 60 8
```

## Debug kernel configuration

For thorough testing, build a debug kernel with:

```
CONFIG_DEBUG_INFO=y
CONFIG_KASAN=y
CONFIG_UBSAN=y
CONFIG_PROVE_LOCKING=y
CONFIG_DEBUG_ATOMIC_SLEEP=y
CONFIG_KCSAN=y
CONFIG_IMX708_FAULT_INJECT=y
```

Run the stress test against this kernel to catch locking issues, data races,
and memory corruption.

## Fault injection

When `CONFIG_IMX708_FAULT_INJECT=y`, debugfs provides fault injection controls:

```bash
# List fault injection controls
ls /sys/kernel/debug/imx708/0/fault/

# Force probe to fail with -EIO
echo -5 > /sys/kernel/debug/imx708/0/fault/probe_fail

# Make next 10 regmap reads fail
echo 10 > /sys/kernel/debug/imx708/0/fault/reg_read_fail

# View injection statistics
cat /sys/kernel/debug/imx708/0/fault/stats
```

## Debugfs diagnostics

```bash
# Register dump
cat /sys/kernel/debug/imx708/0/registers

# Interrupt counters
cat /sys/kernel/debug/imx708/0/irq_counters

# Error counters
cat /sys/kernel/debug/imx708/0/error_counters

# Driver state
cat /sys/kernel/debug/imx708/0/state
```

## Regmap debugfs

The kernel's regmap framework provides its own debugfs interface:

```bash
# List all regmap instances
ls /sys/kernel/debug/regmap/

# Dump IMX708 registers
cat /sys/kernel/debug/regmap/*-i2c-*/registers
```

## Dynamic debug

```bash
# Enable all imx708 debug messages
echo 'module imx708 +p' > /sys/kernel/debug/dynamic_debug/control

# Or use finer-grained control
echo 'file imx708_main.c +p' > /sys/kernel/debug/dynamic_debug/control
```

## Tracepoints

```bash
# Record all IMX708 trace events
trace-cmd record -e imx708:*

# View trace
trace-cmd report

# Or use perf
perf trace -e imx708:*
```
