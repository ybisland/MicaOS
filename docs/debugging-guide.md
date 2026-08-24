# Debugging Guide

MicaOS uses a strong-contract model. It is not a defensive framework that
checks every possible misuse in release builds.

The normal workflow is:

1. expose misuse early in debug builds
2. fix the contract violation
3. keep release builds small and fast

## Diagnostic Levels

### Level 1: OS_ASSERT

`OS_ASSERT()` is used inside MicaOS public API paths for common misuse:

- NULL pointer
- invalid timeout
- invalid priority
- blocking API called from ISR
- invalid initialization parameters

It is controlled by `ASSERT_DEBUG`.

Default:

- enabled when `NDEBUG` is not defined
- disabled when `NDEBUG` is defined

Application code may use `ASSERT()` for its own assertions. MicaOS internals
use `OS_ASSERT()` to avoid mixing OS diagnostics with application style.

### Level 2: OS_DIAG_ASSERT

`OS_DIAG_ASSERT()` is deeper diagnostics. It is intended for difficult bugs and
usually adds more code to hot paths.

It is enabled only when both are true:

```c
#define ASSERT_DEBUG 1
#define OS_DIAGNOSTIC_ENABLE 1
```

Use it when Level 1 assertions do not expose the issue.

## Assertion Failure Hook

Override:

```c
void on_assert_failure(const char *expr, const char *file, int line)
{
    debug_log("ASSERT: %s %s:%d", expr, file, line);

    __disable_irq();
    for (;;) {
        __BKPT(0);
    }
}
```

If there is no logging backend, break into the debugger and inspect the
arguments.

## Stack Watermark

Enable:

```c
#define TASK_STACK_WATERMARK_ENABLE 1
```

Query:

```c
size_t used = task_get_stack_used(task);
size_t unused = task_get_stack_unused(task);
```

Idle stack:

```c
size_t used = scheduler_idle_stack_used();
size_t unused = scheduler_idle_stack_unused();
```

Watermark estimates historical maximum stack usage. It is not stack overflow
protection.

## Trace Hooks

Enable:

```c
#define OS_TRACE_ENABLE 1
```

Override weak hooks:

```c
void os_trace_task_ready(const task_t *task);
void os_trace_task_block(const task_t *task);
void os_trace_task_switch(const task_t *from, const task_t *to);
void os_trace_task_exit(const task_t *task);
```

Trace hooks run in sensitive paths. They must be short and non-blocking.

Good hook behavior:

- increment counters
- write to a small ring buffer
- use RTT/SWO when the backend is known to be safe
- toggle GPIO for timing inspection

Avoid blocking logs or heavy `printf` from trace hooks.

## Bus Publish Failure

`bus_publish()` calls:

```c
void bus_on_publish_fail(bus_channel_t *channel);
```

when an EVENT ring is full.

Default behavior:

- debug build: stops through `OS_ASSERT`
- release build with `NDEBUG`: returns

Override the hook if publish failure should reset, log, or enter a project
specific fault path.

Use `bus_try_publish()` for events that may be dropped.

## Practical Debug Flow

1. Build with assertions enabled.
2. Override `on_assert_failure()`.
3. Reproduce the issue.
4. If no assertion triggers, enable `OS_DIAGNOSTIC_ENABLE`.
5. If stack size is suspected, enable `TASK_STACK_WATERMARK_ENABLE`.
6. If scheduling order is suspected, enable `OS_TRACE_ENABLE`.
7. Fix the contract violation.
8. Disable expensive diagnostics before final size/performance evaluation.
