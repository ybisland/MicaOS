# Getting Started

This guide shows the smallest useful MicaOS application shape.

## 1. Configure MicaOS

Edit:

```text
MicaOS/config.h
```

Select a MCU architecture:

```c
// For a Cortex-M3/M4/M7 project without hard-float ABI:
#define MICAOS_ARCH_PORT MICAOS_ARCH_PORT_ARMV7M

// For Cortex-M0/M0+:
#define MICAOS_ARCH_PORT MICAOS_ARCH_PORT_ARMV6M

// For Cortex-M4F/M7 with hard-float ABI enabled:
#define MICAOS_ARCH_PORT MICAOS_ARCH_PORT_ARMV7M_FPU
```

## 2. Advance OS Tick

It is strongly recommended that configure SysTick as 1ms period timer and call `os_tick_advance()` from the interrupt.

```c
#include "kernel/kernel.h"

void SysTick_Handler(void)
{
    os_tick_advance();
}
```

If SysTick runs every 1 ms, `task_delay(10)` delays for about 10 ms.

## 3. Use Idle Hook When Needed

MicaOS owns the idle task and idle stack. User code does not create an idle
task.

Override this weak hook if needed:

```c
void scheduler_idle_hook(void)
{
    feed_watchdog();
    arch_wait_for_interrupt();
}
```

**The default idle hook only waits for interrupt.**

> The default idle stack size is 128 bytes and is intended only for minimal idle
> work. If the hook calls HAL functions, prints logs, feeds a watchdog through a
> large driver, or enters a complex low-power path, increase
> `SCHED_IDLE_STACK_SIZE` and measure the watermark.

## 4. Create a Task

Resources in MicaOS are statically allocated. So user have to manually allocate 
the task control block(`task_t`) and task stack.

Each task needs:

- one `task_t`
- one caller-owned stack
- one task entry function

```c
static task_t worker;
static task_stack(worker_stack, 512);

static void worker_entry(void *arg)
{
    (void)arg;

    for (;;) {
        do_work();
        task_delay(10);
    }
}
```

`task_stack(name, size)` declares an 8-byte aligned byte stack. `size` is in
bytes.

## 5. Initialize and Start the Scheduler

```c

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

`scheduler_start()` does not return.

`task_init()` prototype:
```c
void task_init(task_t *task,
               const char *name,
               task_entry_t entry,
               void *arg,
               void *stack,
               size_t stack_size,
               task_priority_t priority)
```

Priority values are numeric. Lower values are higher priority.


## 6. First Communication Pattern

For a simple ISR-to-task wakeup, use task notification:

```c
static task_t uart_task;

void USART_IRQHandler(void)
{
    task_notify(&uart_task);
}

static void uart_entry(void *arg)
{
    (void)arg;

    for (;;) {
        if (task_notify_wait(OS_WAIT_FOREVER)) {
            uart_process_rx();
        }
    }
}
```

If you need event bits, use `eventset`. If you need counting, use `sem`. If you
need data transfer, use `msgq` or `pipe`. Read [IPC guide](ipc-guide.md) for more infomation.