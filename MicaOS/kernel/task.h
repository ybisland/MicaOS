#ifndef TASK_H
#define TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include "common/compiler.h"
#include "common/assert.h"
#include "data_structure/dlist.h"
#include "config.h"
#include "time.h"

/*
 * Task
 *
 * Stackful task object plus task-local operations.
 *
 * Usage:
 *   1. Allocate one task_t for each task.
 *   2. Declare an aligned stack with task_stack().
 *   3. Initialize the task with task_init().
 *   4. Add it to the scheduler with scheduler_add().
 *
 * task_delay() blocks the current task for OS ticks. task_delay(0) is a
 * cooperative yield.
 *
 * task_notify() is a lightweight direct wakeup for one target task. It stores
 * only a pending flag: multiple notifications do not accumulate.
 *
 * User code should normally include kernel.h instead of this header directly.
 */

typedef void (*task_entry_t)(void *arg);
typedef uint8_t task_priority_t;

typedef enum task_wait_type {
    TASK_WAIT_NONE,
    TASK_WAIT_DELAY,
    TASK_WAIT_NOTIFY,
    TASK_WAIT_EVENTSET,
    TASK_WAIT_SEM,
    TASK_WAIT_MSGQ_SEND,
    TASK_WAIT_MSGQ_RECV,
    TASK_WAIT_PIPE_WRITE,
    TASK_WAIT_PIPE_READ
} task_wait_type_t;

typedef enum task_state {
    TASK_STATE_CREATED,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_TERMINATED
} task_state_t;

typedef struct task {
    uint32_t *sp;

    void *stack;
    size_t stack_size;

    task_entry_t entry;
    void *arg;

    task_priority_t priority; /* Lower values represent higher priorities. */
    task_state_t state;
    /* Internal wait reason used by delay, notify, eventset, sem, msgq, and pipe. */
    task_wait_type_t wait_type;
    /* Internal object this task is blocked on, such as eventset, sem, or msgq. */
    void *wait_object;
    os_tick_t wake_tick;

    /* Internal scheduler/wait-list nodes. */
    dlist_node_t sched_node;
    dlist_node_t timeout_node;
    bool notify_pending;
    bool notify_result;
    uint32_t eventset_wait_mask;
    uint32_t eventset_wait_result;
    bool eventset_wait_all;
    bool sem_take_result;
    bool msgq_wait_result;
    bool pipe_wait_result;
    const char *name;
} task_t;

/*
 * Declare an 8-byte aligned task stack.
 *
 * size is in bytes.
 *
 * Example:
 *   static task_stack(worker_stack, 256);
 */
#define task_stack(name, size) uint8_t name[(size)] __ALIGNED(8)

/*
 * Initialize a task control block and its initial stack frame.
 *
 * stack points to caller-owned storage and stack_size is in bytes. It must be
 * large enough for the architecture's initial context frame and the task's
 * worst-case call depth. priority must be less than SCHED_PRIORITY_LEVELS when
 * the task is added to the scheduler.
 */
void task_init(task_t *task,
               const char *name,
               task_entry_t entry,
               void *arg,
               void *stack,
               size_t stack_size,
               task_priority_t priority);

/* Return the task currently running, or NULL before scheduler_start(). */
task_t *task_current(void);

/* Return task's debug name, or NULL if no name was provided. */
const char *task_get_name(const task_t *task);

/* Return task's configured priority. Lower values represent higher priority. */
task_priority_t task_get_priority(const task_t *task);

/* Return a snapshot of task's current state. */
task_state_t task_get_state(const task_t *task);

/* Return a snapshot of what the task is currently waiting for. */
task_wait_type_t task_get_wait_type(const task_t *task);

#if TASK_STACK_WATERMARK_ENABLE
/*
 * Return the estimated number of stack bytes that have never been used.
 *
 * This is based on TASK_STACK_FILL_PATTERN and is intended for debugging stack
 * sizing. The result is an estimate of historical high-water usage.
 */
size_t task_get_stack_unused(const task_t *task);

/* Return stack_size - task_get_stack_unused(task). */
size_t task_get_stack_used(const task_t *task);
#endif

/* Voluntarily give the CPU to the next ready task at a scheduling point. */
void task_yield(void);

/*
 * Block the current task for ticks OS ticks.
 *
 * task_delay(0) is equivalent to task_yield(). ticks must be no larger than
 * OS_TICK_MAX_DELAY so wraparound-safe tick comparisons remain valid.
 */
void task_delay(os_tick_t ticks);

/*
 * Set task's pending notification flag.
 *
 * This function may be called from task or ISR context. Multiple notifications
 * do not accumulate; the task only records that at least one notification is
 * pending. A notification sent before the target task waits remains pending and
 * will be consumed by the next task_notify_wait() call. If the target
 * task is blocked on another wait object, the notification remains pending and
 * does not interrupt that wait.
 */
void task_notify(task_t *task);

/*
 * Wait until the current task receives a notification.
 *
 * This function must not be called from ISR context.
 *
 * Return true when a pending notification is consumed. Return false when
 * timeout is OS_NO_WAIT and no notification is pending, or when a finite
 * timeout expires. When timeout is OS_NO_WAIT, the function only checks once
 * and never blocks. Use OS_WAIT_FOREVER to wait without a timeout.
 */
bool task_notify_wait(os_tick_t timeout);

/*
 * Terminate the current task.
 *
 * This function is also used when a task entry function returns.
 */
__NO_RETURN void task_exit(void);

/* Return true when task has been initialized but not yet scheduled. */
static inline bool task_is_created(const task_t *task)
{
    OS_ASSERT(task != NULL);
    return task->state == TASK_STATE_CREATED;
}

/* Return true when task is ready to run. */
static inline bool task_is_ready(const task_t *task)
{
    OS_ASSERT(task != NULL);
    return task->state == TASK_STATE_READY;
}

/* Return true when task is the currently running task. */
static inline bool task_is_running(const task_t *task)
{
    OS_ASSERT(task != NULL);
    return task->state == TASK_STATE_RUNNING;
}

/* Return true when task is waiting for a future wake event. */
static inline bool task_is_blocked(const task_t *task)
{
    OS_ASSERT(task != NULL);
    return task->state == TASK_STATE_BLOCKED;
}

/* Return true when task has exited. */
static inline bool task_is_terminated(const task_t *task)
{
    OS_ASSERT(task != NULL);
    return task->state == TASK_STATE_TERMINATED;
}

#ifdef __cplusplus
}
#endif

#endif /* TASK_H */
