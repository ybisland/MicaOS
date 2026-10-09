# Kernel Guide

This guide explains how to use the MicaOS kernel: tasks, scheduler, OS tick,
delay/yield, task notification, idle behavior, and task debug information.

For synchronization and data transfer primitives such as `eventset`, `sem`,
`msgq`, and `pipe`, read [IPC guide](ipc-guide.md).

## Startup

Application code usually includes one kernel header:

```c
#include <micaos/kernel.h>
```

A simple startup example:

```c
#include <micaos/kernel.h>

static task_t worker_task;            // task control block
static task_stack(worker_stack, 512); // task stack storage

static void worker_entry(void *arg)   // task entry function
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

    task_init(&worker_task,
              "worker",
              worker_entry,
              NULL,
              worker_stack,
              sizeof(worker_stack),
              0);

    scheduler_add(&worker_task);
    scheduler_start();
}
```

`scheduler_start()` does not return.

## Tasks

MicaOS uses **static stackful tasks** and does not allocate dynamic memory.

The user shall provide:

- one `task_t` object for each task
- one stack for each task
- task entry functions

Declare a task object and stack:

```c
static task_t sensor_task;
static task_stack(sensor_stack, 512);
```

`task_stack(name, size)` declares an 8-byte aligned byte array. `size` is in
bytes.

Initialize the task with `task_init()`:

```c
void task_init(task_t *task,              // caller-owned task control block
               const char *name,          // optional debug name, may be `NULL`
               task_entry_t entry,        // task entry function
               void *arg,                 // argument passed to `entry(arg)`, may be `NULL`
               void *stack,               // caller-owned stack storage
               size_t stack_size,         // stack size in bytes
               task_priority_t priority); // static task priority
```

Priority values start at 0. Lower numeric values are higher priority.
`priority` must be less than `SCHED_PRIORITY_LEVELS`.

If stack usage is unknown, start with a larger stack, enable stack watermark
during testing, then reduce only after measuring.

If a task entry function returns, MicaOS routes it to `task_exit()`.

## Scheduler

### Scheduling Policy

MicaOS uses static priorities:

```text
Different priorities: preemptive.
Same priority: cooperative FIFO.
```

Lower numeric priority values are higher priority. A higher-priority READY task
preempts the current lower-priority task. Equal-priority tasks do not preempt
each other; they run FIFO only when the running task yields, blocks, waits, or
exits.

There is no time-slice round-robin scheduling. If a high-priority task remains
READY forever, lower-priority tasks will not run.

### Scheduler API

```c
void scheduler_init(void);
void scheduler_add(task_t *task);
void scheduler_start(void);
task_t *scheduler_current(void);
```

`scheduler_init()` initializes scheduler state and must be called before
adding tasks.

`scheduler_add()` adds an initialized task to the READY queue. It can be called
before or after `scheduler_start()`. If the scheduler is already running and
the new task has higher priority than the current task, it preempts the current
task.

`scheduler_start()` starts task scheduling and does not return.

`scheduler_current()` returns the current running task, or `NULL` before the
scheduler starts.

High-priority preemption can also happen when an ISR or another task makes a
higher-priority task READY. ISR does not directly run the new task inside the
ISR body; the architecture port requests a deferred context switch.

## OS Tick

The OS tick is a `uint32_t` counter. It advances when the board calls:

```c
void os_tick_advance(void);
```

Typical SysTick integration:

```c
void SysTick_Handler(void)
{
    os_tick_advance();
}
```

### Use tick
Read the current tick:

```c
os_tick_t now = os_tick_get();
```

Use helper functions for time comparisons:

```c
os_tick_t start = os_tick_get();

if (os_tick_elapsed(os_tick_get(), start, 100)) {
    timeout_handler();
}
```

```c
os_tick_t deadline = os_tick_get() + 100;

if (os_tick_after_eq(os_tick_get(), deadline)) {
    deadline_handler();
}
```

Do not use plain `<` or `>` for wraparound-sensitive tick comparisons.

All time-order comparisons are valid only when the real time distance is less
than half of the `uint32_t` counter range. This is why finite delay and timeout
values are limited by `OS_TICK_MAX_DELAY`.

## Delay and Yield

API:
```c
void task_delay(os_tick_t timeout);
void task_yield(void);
```

`task_delay(0)` is equivalent to `task_yield()`.

For nonzero `timeout`, the current task becomes BLOCKED until that many OS ticks
have elapsed. `timeout` must be no larger than `OS_TICK_MAX_DELAY`.

Example:

```c
for (;;) {
    sample_sensor();
    task_delay(10);
}
```

`task_yield()` voluntarily gives the scheduler a chance to run another READY
task. Use it when an equal-priority task should get a chance to run:

```c
for (;;) {
    do_small_piece_of_work();
    task_yield();
}
```

Yield is not a delay. If no other suitable task is READY, the same task may
continue running.

## Task Notification

Task notification is a direct wakeup from one producer to one target task.

API:

```c
void task_notify(task_t *task);
bool task_notify_wait(os_tick_t timeout);
```

`timeout` can be:
- `OS_NO_WAIT`: Check once and never block
- finite tick value: Wait up to that many ticks
- `OS_WAIT_FOREVER`: Wait without a deadline

Example ISR-to-task wakeup:

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

Properties:

- each task has one pending notification flag
- multiple notifications do not accumulate
- notification sent before wait is not lost
- `task_notify()` may be called from task or ISR context
- `task_notify_wait()` must be called only from task context

Use `eventset` if you need named bits or wait-any/wait-all behavior. Use `sem`
if you need counting behavior.

## Task Exit

```c
void task_exit(void);
```

`task_exit()` terminates the current task. It does not return.

Terminated task objects are not automatically reused. If the application wants
object reuse, build that policy above the kernel.

## Idle Task

MicaOS owns the idle task and idle stack.

The idle hook is weak and may be overridden:

```c
void scheduler_idle_hook(void)
{
    feed_watchdog();
}
```

**The default hook waits for interrupt.**

If you override the hook and still want low-power idle behavior, call the
target platform's WFI instruction directly, for example CMSIS `__WFI()`.
MicaOS does not expose its internal architecture context header for this.

`SCHED_IDLE_STACK_SIZE` controls the internal idle stack size. The default is
128 bytes and is intended for minimal idle behavior. If the hook calls HAL
functions, logging functions, watchdog drivers, or other nontrivial code,
increase the idle stack and verify with watermark.

## Task Debug Information

These APIs are for observation:

```c
const char *task_get_name(const task_t *task);
task_priority_t task_get_priority(const task_t *task);
task_state_t task_get_state(const task_t *task);
task_wait_type_t task_get_wait_type(const task_t *task);
```

They return snapshots. State may change immediately after the call, so do not
use these APIs as synchronization.

## Stack Watermark

Enable:

```c
#define TASK_STACK_WATERMARK_ENABLE 1
```

Then use:

```c
size_t task_get_stack_unused(const task_t *task);
size_t task_get_stack_used(const task_t *task);
size_t scheduler_idle_stack_unused(void);
size_t scheduler_idle_stack_used(void);
```

Watermark estimates historical maximum stack usage. It is useful for sizing
tasks during testing. It is not stack overflow protection.
