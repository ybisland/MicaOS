# Kernel Guide

This guide explains how to use the MicaOS kernel API.

User code normally includes:

```c
#include "kernel/kernel.h"
```

## Kernel Model

MicaOS is a static RTOS:

- User code statically allocates `task_t` objects.
- User code statically allocates task stacks.
- The kernel owns the idle task and idle stack.
- No dynamic allocation is used by the kernel.
- Higher-priority READY tasks preempt lower-priority tasks.
- Equal-priority tasks do not preempt each other.
- Equal-priority tasks run FIFO at cooperative scheduling points.
- There is no time-slice round-robin scheduling.

## Task Creation

```c
static task_t worker;
static task_stack(worker_stack, 512);

static void worker_entry(void *arg)
{
    (void)arg;

    for (;;) {
        task_delay(10);
    }
}
```

Initialize the task:

```c
task_init(&worker,
          "worker",
          worker_entry,
          NULL,
          worker_stack,
          sizeof(worker_stack),
          0);
```

Priority rule:

```text
Lower number = higher priority.
```

`priority` must be less than `SCHED_PRIORITY_LEVELS`.

## Scheduler Startup

```c
scheduler_init();
scheduler_add(&worker);
scheduler_start();
```

`scheduler_start()` starts the highest-priority READY task and does not return.

## Adding Tasks After Start

`scheduler_add()` may also be called after the scheduler has started. If the
new task has higher priority than the current task, it preempts the current
task.

## Scheduling Points

Common scheduling points:

- `task_yield()`
- `task_delay()`
- `task_notify_wait()`
- `eventset_wait_any()` / `eventset_wait_all()`
- `sem_take()`
- `msgq_send()` / `msgq_recv()`
- `pipe_write()` / `pipe_read()`
- `task_exit()`

High-priority preemption may also happen when another task becomes READY.

Equal-priority tasks only switch at cooperative scheduling points.

## Yield

```c
task_yield();
```

`task_yield()` gives the scheduler a chance to run another READY task.

If the current task is still the highest-priority READY task, it may continue
running.

## Delay

```c
task_delay(10);
```

`task_delay(0)` is equivalent to `task_yield()`.

Nonzero delay blocks the current task for that many OS ticks. The maximum
finite delay is `OS_TICK_MAX_DELAY`.

## OS Tick

The OS tick is advanced by:

```c
void os_tick_advance(void);
```

Usually:

```c
void SysTick_Handler(void)
{
    os_tick_advance();
}
```

Read current tick:

```c
os_tick_t now = os_tick_get();
```

Use wraparound-safe helpers:

```c
if (os_tick_elapsed(os_tick_get(), start, 100)) {
    /* 100 ticks elapsed */
}

if (os_tick_after_eq(os_tick_get(), deadline)) {
    /* deadline reached */
}
```

Do not compare tick values with plain `<` or `>` when wraparound matters.
All time ordering logic is only unambiguous when the real distance is less than
half of the `uint32_t` range.

## Notification

Task notification is the lightest direct wakeup mechanism.

```c
void task_notify(task_t *task);
bool task_notify_wait(os_tick_t timeout);
```

Usage:

```c
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

Properties:

- Notification is stored as one pending flag per task.
- Multiple notifications do not accumulate.
- A notification sent before wait is not lost.
- `task_notify()` may be called from ISR.
- `task_notify_wait()` must not be called from ISR.

## Task Exit

```c
task_exit();
```

If a task entry function returns, MicaOS routes it to `task_exit()`.

Terminated tasks are not automatically recycled. If the application wants task
object reuse, it must build that policy above the kernel.

## Idle Task

MicaOS owns the idle task and idle stack. User code does not create an idle
task.

Customize idle behavior by overriding:

```c
void scheduler_idle_hook(void)
{
    arch_wait_for_interrupt();
}
```

Default idle behavior waits for interrupt.

The default `SCHED_IDLE_STACK_SIZE` is 128 bytes and is intended only for
minimal idle behavior. If the idle hook does real work, increase the idle stack
and measure with stack watermark.

## Task Debug Information

```c
const char *task_get_name(const task_t *task);
task_priority_t task_get_priority(const task_t *task);
task_state_t task_get_state(const task_t *task);
task_wait_type_t task_get_wait_type(const task_t *task);
```

These APIs return snapshots. They are useful for debugging and logging, but
should not be used as a replacement for synchronization primitives.

## Stack Watermark

When `TASK_STACK_WATERMARK_ENABLE` is 1:

```c
size_t task_get_stack_unused(const task_t *task);
size_t task_get_stack_used(const task_t *task);
size_t scheduler_idle_stack_unused(void);
size_t scheduler_idle_stack_used(void);
```

Watermark is a debug sizing tool, not stack overflow protection.
