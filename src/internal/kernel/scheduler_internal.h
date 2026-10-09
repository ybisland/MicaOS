#ifndef SCHEDULER_INTERNAL_H
#define SCHEDULER_INTERNAL_H

#include <micaos_config.h>
#include <micaos/scheduler.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Private kernel wait/schedule interface.
 *
 * This header is shared by task, scheduler, time, and wait objects. User code
 * should include kernel.h instead.
 */

/* Internal task facade used by task_yield(). */
void scheduler_yield(void);

/* Internal task facade used by task_exit(). */
__NO_RETURN void scheduler_exit_current(void);

/* Initialize kernel time state. Called by scheduler_init(). */
void kernel_time_init(void);

/*
 * Return the current OS tick while the scheduler interrupt lock is held.
 *
 * This avoids nesting another lock when kernel objects already run in a locked
 * section.
 */
os_tick_t os_tick_get_locked(void);

#if OS_TIMER_ENABLE
/* Initialize soft timer state. Called by kernel_time_init(). */
void kernel_timer_init(void);

/* Run expired soft timers for the current tick. */
void kernel_timer_advance(os_tick_t now);
#endif

/*
 * Add task to the timeout list.
 *
 * The caller must hold the scheduler interrupt lock. ticks must be nonzero and
 * no larger than OS_TICK_MAX_DELAY.
 */
void kernel_timeout_start_locked(task_t *task, os_tick_t ticks);

/*
 * Remove task from the timeout list if it has a pending timeout.
 *
 * The caller must hold the scheduler interrupt lock.
 */
void kernel_timeout_cancel_locked(task_t *task);

/*
 * Clear a timed-out pipe waiter from its SPSC wait slot.
 *
 * The caller must hold the scheduler interrupt lock.
 */
void pipe_timeout_locked(task_t *task);

/*
 * Mark a blocked or newly created task READY and preempt if its priority wins.
 *
 * This wrapper may be called without holding the scheduler interrupt lock.
 */
void scheduler_make_ready(task_t *task);

/*
 * Mark a blocked or newly created task READY.
 *
 * The caller must already hold the scheduler interrupt lock. This is useful
 * when a wait object removes a task from its wait queue and makes it READY in
 * one critical section.
 */
void scheduler_make_ready_locked(task_t *task);

/*
 * Block the current task and switch to the next ready task.
 *
 * This wrapper may be called without holding the scheduler interrupt lock.
 */
void scheduler_block_current(void);

/*
 * Block the current task while the scheduler interrupt lock is already held.
 *
 * Wait objects should use this after linking the current task into their wait
 * queue. The caller should release the interrupt lock immediately after this
 * call so the deferred context switch can run.
 */
void scheduler_block_current_locked(void);

/*
 * Complete a prepared switch from the architecture exception handler.
 *
 * saved_sp is the software-frame SP of the previous task. The return value is
 * the software-frame SP to restore next.
 */
uint32_t *arch_context_switch_callback(uint32_t *saved_sp);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_INTERNAL_H */
