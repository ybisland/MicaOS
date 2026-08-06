#ifndef SCHEDULER_H
#define SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "kernel_config.h"
#include "task.h"

/*
 * Scheduler
 *
 * Static-priority scheduler for stackful tasks.
 *
 * Usage:
 *   1. Call scheduler_init().
 *   2. Initialize tasks with task_init().
 *   3. Add tasks with scheduler_add().
 *   4. Call scheduler_start(). It does not return.
 *
 * Scheduling policy:
 *   - Lower numeric priority values are higher priorities.
 *   - Higher-priority READY tasks preempt lower-priority running tasks.
 *   - Equal-priority tasks do not preempt each other.
 *   - Equal-priority tasks run FIFO at cooperative scheduling points.
 *   - There is no time-slice round-robin scheduling.
 *
 * Idle:
 *   The scheduler owns the internal idle task and idle stack. Override
 *   scheduler_idle_hook() to customize idle behavior.
 *
 * User code should normally include kernel.h instead of this header directly.
 */

/* Initialize scheduler. */
void scheduler_init(void);

/*
 * Add an initialized task to its priority ready queue.
 *
 * If the scheduler is already running, a newly added higher-priority task will
 * preempt the current lower-priority task.
 */
void scheduler_add(task_t *task);

/* Start the highest-priority ready task. This function does not return. */
__NO_RETURN void scheduler_start(void);

/* Return the currently running task, or NULL before scheduler_start(). */
task_t *scheduler_current(void);

/*
 * Hook called by the internal idle task.
 *
 * The default implementation waits for interrupt. Projects may override this
 * weak hook to feed a watchdog, enter a different low-power mode, or simply
 * return immediately.
 */
void scheduler_idle_hook(void);

#if TASK_STACK_WATERMARK_ENABLE
/* Return estimated unused bytes in the internal idle task stack. */
size_t scheduler_idle_stack_unused(void);

/* Return SCHED_IDLE_STACK_SIZE - scheduler_idle_stack_unused(). */
size_t scheduler_idle_stack_used(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */
