#ifndef CONFIG_H
#define CONFIG_H

/*
 * MicaOS configuration
 *
 * Edit this file to configure MicaOS. CMake, Keil, IAR, and other build
 * systems should use the same config.h so the OS behavior stays consistent.
 * Advanced build systems may still override these macros through compiler
 * definitions when maintaining multiple build variants.
 *
 * Conventions:
 *   - *_ENABLE options use 0 or 1.
 *   - Stack sizes are in bytes.
 *   - Lower numeric priority values represent higher scheduling priorities.
 */

/* Assertions and diagnostics ---------------------------------------------- */

#ifndef ASSERT_DEBUG
# ifdef NDEBUG
#  define ASSERT_DEBUG 0
# else
#  define ASSERT_DEBUG 1
# endif
#endif

#if (ASSERT_DEBUG != 0) && (ASSERT_DEBUG != 1)
#error "ASSERT_DEBUG must be 0 or 1"
#endif

/* Architecture port ------------------------------------------------------- */

/*
 * Select the architecture context-switch port.
 *
 * Build systems may add all arch_context.c files; only the selected port emits
 * code. This option does not replace the compiler CPU/FPU flags. For example,
 * ARMv7M_FPU still requires a hard-float build configuration.
 */
#define MICAOS_ARCH_PORT_ARMV6M      1U
#define MICAOS_ARCH_PORT_ARMV7M      2U
#define MICAOS_ARCH_PORT_ARMV7M_FPU  3U

#ifndef MICAOS_ARCH_PORT
#define MICAOS_ARCH_PORT MICAOS_ARCH_PORT_ARMV7M
#endif

#if (MICAOS_ARCH_PORT != MICAOS_ARCH_PORT_ARMV6M) && \
    (MICAOS_ARCH_PORT != MICAOS_ARCH_PORT_ARMV7M) && \
    (MICAOS_ARCH_PORT != MICAOS_ARCH_PORT_ARMV7M_FPU)
#error "MICAOS_ARCH_PORT must select a valid architecture port"
#endif

/* Kernel scheduler -------------------------------------------------------- */

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

/* Optional features ------------------------------------------------------- */

/* Enable soft timer support. Disabled by default so os_tick_advance() stays minimal. */
#ifndef OS_TIMER_ENABLE
#define OS_TIMER_ENABLE 0
#endif

#if (OS_TIMER_ENABLE != 0) && (OS_TIMER_ENABLE != 1)
#error "OS_TIMER_ENABLE must be 0 or 1"
#endif

/*
 * Enable detailed MicaOS diagnostic assertions.
 *
 * OS_ASSERT checks public API misuse in debug builds. This switch enables
 * deeper contract and consistency checks in data structures, services, and
 * kernel internals while debugging difficult bugs. It has an effect only when
 * ASSERT_DEBUG is 1.
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

/* Memory policy ----------------------------------------------------------- */

#ifndef SLAB_ALLOC_FAILED_HOOK_ENABLE
#define SLAB_ALLOC_FAILED_HOOK_ENABLE 1
#endif

#if (SLAB_ALLOC_FAILED_HOOK_ENABLE != 0) && (SLAB_ALLOC_FAILED_HOOK_ENABLE != 1)
#error "SLAB_ALLOC_FAILED_HOOK_ENABLE must be 0 or 1"
#endif

#endif /* CONFIG_H */
