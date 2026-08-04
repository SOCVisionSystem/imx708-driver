# Debugging

## Dynamic debug

```bash
# Enable all imx708 debug messages
echo 'module imx708 +p' > /sys/kernel/debug/dynamic_debug/control

# Enable for a specific source file
echo 'file imx708_main.c +p' > /sys/kernel/debug/dynamic_debug/control

# Enable for a specific function
echo 'func imx708_s_stream +p' > /sys/kernel/debug/dynamic_debug/control

# Disable all
echo 'module imx708 -p' > /sys/kernel/debug/dynamic_debug/control
```

## Ftrace

```bash
# Set up ftrace for imx708
echo imx708 > /sys/kernel/debug/tracing/set_ftrace_filter
echo function_graph > /sys/kernel/debug/tracing/current_tracer
cat /sys/kernel/debug/tracing/trace_pipe
```

## Tracepoints

```bash
# List available tracepoints
grep imx708 /sys/kernel/debug/tracing/available_events

# Enable all imx708 tracepoints
echo 1 > /sys/kernel/debug/tracing/events/imx708/enable

# Or enable individually
echo 1 > /sys/kernel/debug/tracing/events/imx708/imx708_probe/enable
echo 1 > /sys/kernel/debug/tracing/events/imx708/imx708_irq/enable
echo 1 > /sys/kernel/debug/tracing/events/imx708/imx708_error/enable

# View trace
cat /sys/kernel/debug/tracing/trace
```

## Regmap debugfs

```bash
# Find the regmap instance
ls /sys/kernel/debug/regmap/

# Dump all registers
cat /sys/kernel/debug/regmap/*-i2c-*/registers

# Dump register ranges
cat /sys/kernel/debug/regmap/*-i2c-*/range
```

## Debugfs

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

## KGDB / KDB

```bash
# Enter KDB
echo g > /proc/sysrq-trigger

# In KDB:
#   bt          - backtrace
#   rd          - register dump
#   md <addr>   - memory dump
#   mm <addr> <val> - memory modify
#   go          - continue execution
```

## Common failures

| Symptom | Likely cause | Check |
|---|---|---|
| `chip ID mismatch` | Wrong I2C address or sensor not powered | Verify DOVDD/AVDD/DVDD voltages, I2C address in DT |
| `failed to read chip ID` | I2C bus not working | `i2cdetect -y <bus>`, check pull-ups |
| `probe fails with -EPROBE_DEFER` | Clock or regulator not ready | Check `clk_summary` in debugfs, regulator status |
| `streaming fails` | Mode configuration wrong | Check register dump, verify mode table |
| `no frames` | MIPI CSI-2 not configured | Check CSI-2 receiver driver, data lanes in DT |
| `PLL unlock` | Clock frequency wrong | Verify xvclk frequency (24 MHz typical) |
| `IRQ storm` | Interrupt not acknowledged | Check irq_ack callback, interrupt polarity |
| `oops in regmap_read` | Register address out of range | Check max_register in regmap_config |
| `lockdep warning` | Lock ordering violation | Check locking documentation in imx708_main.c |

## Kernel config for debugging

```
CONFIG_DEBUG_KERNEL=y
CONFIG_DEBUG_INFO=y
CONFIG_DYNAMIC_DEBUG=y
CONFIG_FTRACE=y
CONFIG_FUNCTION_TRACER=y
CONFIG_FUNCTION_GRAPH_TRACER=y
CONFIG_STACK_TRACER=y
CONFIG_PROVE_LOCKING=y
CONFIG_LOCKDEP=y
CONFIG_DEBUG_ATOMIC_SLEEP=y
CONFIG_DEBUG_SPINLOCK=y
CONFIG_DEBUG_MUTEXES=y
CONFIG_KASAN=y
CONFIG_UBSAN=y
CONFIG_KCSAN=y
CONFIG_FAULT_INJECTION=y
CONFIG_FAULT_INJECTION_DEBUG_FS=y
```
