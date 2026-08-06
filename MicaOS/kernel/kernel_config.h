#ifndef KERNEL_CONFIG_H
#define KERNEL_CONFIG_H

/*
 * Kernel configuration
 *
 * All kernel-wide options live here. Projects should override these macros
 * from the build system or from a project configuration header included before
 * any kernel header.
 *
 * Every kernel source file must see the same values.
 *
 * Conventions:
 *   - *_ENABLE options use 0 or 1.
 *   - Stack sizes are in bytes.
 *   - Lower numeric priority values represent higher scheduling priorities.
 */

/* Scheduler --------------------------------------------------------------- */

/* Number of task priority levels. Valid range: 1..32. */
#ifndef SCHED_PRIORITY_LEVELS
#define SCHED_PRIORITY_LEVELS 8U
#endif

#if (SCHED_PRIORITY_LEVELS < 1U) || (SCHED_PRIORITY_LEVELS > 32U)
#error "SCHED_PRIORITY_LEVELS must be in the range 1..32"
#endif

/*
 * Internal idle task stack size. Must be 8-byte aligned.
 *
 * The default 128 bytes is intended for the minimal idle path only:
 * scheduler_idle_hook() waits for interrupt and does no heavy work. On the
 * F411 test builds this path used 88 bytes with ARMv7M soft-float and 92
 * bytes with ARMv7M_FPU hard-float by stack watermark.
 *
 * Increase this value if the project overrides scheduler_idle_hook() to call
 * HAL low-power functions, feed a watchdog, print logs, or do other work.
 */
#ifndef SCHED_IDLE_STACK_SIZE
#define SCHED_IDLE_STACK_SIZE 128U
#endif

#if ((SCHED_IDLE_STACK_SIZE) % 8U) != 0U
#error "SCHED_IDLE_STACK_SIZE must be 8-byte aligned"
#endif

/* Optional kernel features ------------------------------------------------ */

/* Enable soft timer support. Disabled by default so os_tick_advance() stays minimal. */
#ifndef OS_TIMER_ENABLE
#define OS_TIMER_ENABLE 0
#endif

#if (OS_TIMER_ENABLE != 0) && (OS_TIMER_ENABLE != 1)
#error "OS_TIMER_ENABLE must be 0 or 1"
#endif

/*
 * Enable internal kernel diagnostic assertions.
 *
 * Public API contract checks should stay as normal ASSERT checks.
 * This switch is for OS-internal consistency checks used while developing and
 * testing the kernel itself.
 */
#ifndef OS_DIAGNOSTIC_ENABLE
#define OS_DIAGNOSTIC_ENABLE 0
#endif

#if (OS_DIAGNOSTIC_ENABLE != 0) && (OS_DIAGNOSTIC_ENABLE != 1)
#error "OS_DIAGNOSTIC_ENABLE must be 0 or 1"
#endif

/*
 * Enable task stack watermark measurement.
 *
 * When enabled, task_init() fills the caller-provided stack with
 * TASK_STACK_FILL_PATTERN before the initial architecture frame is built.
 * Query APIs can then estimate historical maximum stack usage.
 */
#ifndef TASK_STACK_WATERMARK_ENABLE
#define TASK_STACK_WATERMARK_ENABLE 0
#endif

#if (TASK_STACK_WATERMARK_ENABLE != 0) && (TASK_STACK_WATERMARK_ENABLE != 1)
#error "TASK_STACK_WATERMARK_ENABLE must be 0 or 1"
#endif

/* Byte pattern used by task stack watermark measurement. */
#ifndef TASK_STACK_FILL_PATTERN
#define TASK_STACK_FILL_PATTERN 0xA5U
#endif

#if (TASK_STACK_FILL_PATTERN > 0xFFU)
#error "TASK_STACK_FILL_PATTERN must fit in one byte"
#endif

/*
 * Enable generic OS trace hooks.
 *
 * Disabled by default. When enabled, the scheduler calls weak trace hooks at
 * key task state transitions. Backends such as SystemView can be built on top
 * of these hooks later.
 */
#ifndef OS_TRACE_ENABLE
#define OS_TRACE_ENABLE 0
#endif

#if (OS_TRACE_ENABLE != 0) && (OS_TRACE_ENABLE != 1)
#error "OS_TRACE_ENABLE must be 0 or 1"
#endif

#endif /* KERNEL_CONFIG_H */
