#ifndef KERNEL_H
#define KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Kernel public API.
 *
 * Users should include this header as the single public kernel entry point.
 * kernel_config.h, task.h, scheduler.h, time.h, eventset.h, sem.h, msgq.h,
 * pipe.h, and the optional timer.h are split only to keep implementation
 * files small and focused. trace.h is kept separate for projects that enable
 * a trace backend.
 *
 * Startup:
 *   1. Statically allocate one task_t and one stack for each task.
 *   2. Call scheduler_init().
 *   3. Initialize tasks with task_init().
 *   4. Add tasks with scheduler_add().
 *   5. Call scheduler_start(). It does not return.
 *
 * Scheduling:
 *   - Lower numeric priority values represent higher priorities.
 *   - A higher-priority READY task preempts the current lower-priority task.
 *   - Equal-priority tasks do not preempt each other.
 *   - Equal-priority tasks run in FIFO order at cooperative scheduling points.
 *   - There is no time-slice round-robin scheduling.
 *
 * Idle:
 *   The kernel owns the internal idle task and idle stack. Override
 *   scheduler_idle_hook() to customize idle behavior.
 *
 * Internal interfaces:
 *   scheduler_internal.h is private to kernel objects and architecture glue.
 */

#include "kernel_config.h"
#include "time.h"

#if OS_TIMER_ENABLE
#include "timer.h"
#endif

#include "task.h"
#include "eventset.h"
#include "sem.h"
#include "msgq.h"
#include "pipe.h"
#include "scheduler.h"

#ifdef __cplusplus
}
#endif

#endif /* KERNEL_H */
