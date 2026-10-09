# Porting Guide

This guide explains how to bring MicaOS up on a board.

## Porting Checklist

1. Add `include/` to the application include path.
2. Add `config/` to the application include path.
3. Add `src/` while compiling MicaOS implementation files.
4. Configure `config/micaos_config.h`.
5. Add MicaOS source files to the build.
6. Select the correct `MICAOS_ARCH_PORT`.
7. Make sure MicaOS provides `PendSV_Handler`.
8. Call `os_tick_advance()` from a periodic timer interrupt.
9. Create tasks and start the scheduler.

## Include Path

Add:

```text
path/to/micaos/include
path/to/micaos/config
```

Then user code can include:

```c
#include <micaos/kernel.h>
```

## Architecture Port

Select in `config/micaos_config.h`:

```c
#define MICAOS_ARCH_PORT MICAOS_ARCH_PORT_ARMV7M
```

| Core | Port |
| --- | --- |
| Cortex-M0 / M0+ | `MICAOS_ARCH_PORT_ARMV6M` |
| Cortex-M3 / M4 / M7 soft-float ABI | `MICAOS_ARCH_PORT_ARMV7M` |
| Cortex-M4F / M7 hard-float ABI | `MICAOS_ARCH_PORT_ARMV7M_FPU` |

Hard-float port selection must match compiler flags.

## PendSV

Cortex-M context switching uses PendSV.

Make sure the final vector table uses the `PendSV_Handler` from the selected
MicaOS architecture port.

Common cases:

- startup file defines `PendSV_Handler` as weak: MicaOS overrides it
- project already defines a strong `PendSV_Handler`: remove it or route it to
  MicaOS

## Tick Integration

Use SysTick or another periodic interrupt:

```c
void SysTick_Handler(void)
{
    os_tick_advance();
}
```

The tick frequency is chosen by the board project. If it runs at 1 kHz, one OS
tick is approximately 1 ms.

## Minimal Board Main

```c
#include <micaos/kernel.h>

static task_t worker;
static task_stack(worker_stack, 512);

static void worker_entry(void *arg)
{
    (void)arg;

    for (;;) {
        board_led_toggle();
        task_delay(500);
    }
}

int main(void)
{
    board_init();

    scheduler_init();

    task_init(&worker,
              "worker",
              worker_entry,
              NULL,
              worker_stack,
              sizeof(worker_stack),
              0);

    scheduler_add(&worker);
    scheduler_start();
}
```

## Idle Hook

Default idle waits for interrupt.

Override if needed:

```c
void scheduler_idle_hook(void)
{
    feed_watchdog();
    __WFI(); // CMSIS/platform WFI instruction
}
```

If the hook does more than WFI, increase `SCHED_IDLE_STACK_SIZE` and measure
with stack watermark.

Observed WFI-only idle stack usage on F411 debug builds:

| Port | Idle stack size | Used | Unused |
| --- | --- | --- | --- |
| ARMv7M | 128 | 88 | 40 |
| ARMv7M_FPU | 128 | 92 | 36 |

Observed WFI-only idle stack usage on G070 debug build:

| Port | Idle stack size | Used | Unused |
| --- | --- | --- | --- |
| ARMv6M | 128 | 96 | 32 |

## ISR Usage

ISR may wake tasks but must not block.

Allowed examples:

```c
task_notify(&task);
eventset_set(&events, BIT);
sem_give(&sem);
msgq_send(&q, &msg, OS_NO_WAIT);
pipe_write(&pipe, data, len, OS_NO_WAIT);
```

Do not use blocking APIs from ISR.

## First Bring-Up Test

Recommended order:

1. boot and print before scheduler
2. start one blinking task
3. call `task_delay()`
4. add a second equal-priority task and test FIFO yield
5. add a higher-priority task and test preemption
6. enable SysTick and verify delay timing
7. test one ISR wakeup path
8. enable stack watermark and inspect idle/task usage

## Debugger Notes

For Cortex-M, keeping SWD pins enabled is critical during bring-up.

If firmware accidentally reconfigures SWD pins, recovery may require
connect-under-reset or a power-on race attach/erase depending on the board.
Boards whose debug probe does not wire NRST are harder to recover.
