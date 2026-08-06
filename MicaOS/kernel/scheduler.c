#include "scheduler_internal.h"
#include "kernel_debug.h"
#include "trace.h"
#include "arch/arch_context.h"
#include "data_structure/bitmap.h"

typedef struct scheduler {
    task_t *current;
    task_t *next;
    task_t idle_task;
    dlist_t ready_queues[SCHED_PRIORITY_LEVELS];
    bitmap_t ready_bitmap;
    bool running;
} scheduler_t;

typedef enum scheduler_requeue {
    SCHEDULER_REQUEUE_NONE,
    SCHEDULER_REQUEUE_FRONT,
    SCHEDULER_REQUEUE_BACK
} scheduler_requeue_t;

static scheduler_t scheduler_;
static bitmap_storage(scheduler_ready_words_, SCHED_PRIORITY_LEVELS);
static task_stack(scheduler_idle_stack_, SCHED_IDLE_STACK_SIZE);

task_t *scheduler_current(void)
{
    return scheduler_.current;
}

#if TASK_STACK_WATERMARK_ENABLE
size_t scheduler_idle_stack_unused(void)
{
    return task_get_stack_unused(&scheduler_.idle_task);
}

size_t scheduler_idle_stack_used(void)
{
    return task_get_stack_used(&scheduler_.idle_task);
}
#endif

__WEAK void scheduler_idle_hook(void)
{
    arch_wait_for_interrupt();
}

static void scheduler_idle_entry_(void *arg)
{
    (void)arg;

    for (;;) {
        scheduler_idle_hook();
    }
}

static task_t *scheduler_pop_ready_(void)
{
    dlist_node_t *node;
    uint32_t priority;

    if (!bitmap_find_first_set(&scheduler_.ready_bitmap, &priority)) {
        return NULL;
    }

    node = dlist_pop_front(&scheduler_.ready_queues[priority]);
    OS_DIAG_ASSERT(node != NULL);

    if (dlist_empty(&scheduler_.ready_queues[priority])) {
        bitmap_clear(&scheduler_.ready_bitmap, priority);
    }

    return dlist_entry(node, task_t, sched_node);
}

static void scheduler_push_ready_(task_t *task)
{
    dlist_t *queue;
#if OS_TRACE_ENABLE
    task_state_t old_state;
#endif

    OS_DIAG_ASSERT((task->priority < SCHED_PRIORITY_LEVELS) &&
                   dlist_node_is_detached(&task->sched_node));

    queue = &scheduler_.ready_queues[task->priority];
#if OS_TRACE_ENABLE
    old_state = task->state;
#endif
    task->state = TASK_STATE_READY;
    dlist_push_back(queue, &task->sched_node);
    bitmap_set(&scheduler_.ready_bitmap, task->priority);
#if OS_TRACE_ENABLE
    if (old_state != TASK_STATE_READY) {
        os_trace_task_ready(task);
    }
#endif
}

static void scheduler_push_ready_front_(task_t *task)
{
    dlist_t *queue;
#if OS_TRACE_ENABLE
    task_state_t old_state;
#endif

    OS_DIAG_ASSERT((task->priority < SCHED_PRIORITY_LEVELS) &&
                   dlist_node_is_detached(&task->sched_node));

    queue = &scheduler_.ready_queues[task->priority];
#if OS_TRACE_ENABLE
    old_state = task->state;
#endif
    task->state = TASK_STATE_READY;
    dlist_push_front(queue, &task->sched_node);
    bitmap_set(&scheduler_.ready_bitmap, task->priority);
#if OS_TRACE_ENABLE
    if (old_state != TASK_STATE_READY) {
        os_trace_task_ready(task);
    }
#endif
}

static task_t *scheduler_select_next_(void)
{
    task_t *next;

    next = scheduler_pop_ready_();
    if (next == NULL) {
        next = &scheduler_.idle_task;
    }

    return next;
}

static bool scheduler_ready_task_should_preempt_(const task_t *task)
{
    task_t *target;

    OS_DIAG_ASSERT(task != NULL);

    if (!scheduler_.running || scheduler_.current == NULL) {
        return false;
    }

    target = scheduler_.next;
    if (target == NULL) {
        target = scheduler_.current;
    }

    if (target == &scheduler_.idle_task) {
        return true;
    }

    return task->priority < target->priority;
}

/*
 * Prepare a deferred context switch while the scheduler lock is held.
 *
 * requeue_current describes what should happen to the current task:
 *   - NONE:  current task is blocking or exiting, so it stays out of ready queues.
 *   - FRONT: current task was preempted, so it returns to the front and keeps
 *            its same-priority FIFO position.
 *   - BACK:  current task yielded cooperatively, so it returns to the back.
 *
 * If a previous switch request already selected scheduler_.next but PendSV has
 * not run yet, put that pending next task back at the front and select again.
 * This lets a newly READY higher-priority task replace the pending choice
 * without losing FIFO order.
 */
static bool scheduler_request_switch_locked_(scheduler_requeue_t requeue_current)
{
    task_t *current;
    task_t *next;

    OS_DIAG_ASSERT(scheduler_.running && (scheduler_.current != NULL));

    current = scheduler_.current;

    // If a task is already pending to run next, put it back to the front of
    // its ready queue so it doesn't lose its place. This can happen when a
    // task becomes ready while another switch is pending but hasn't run yet.
    if (scheduler_.next != NULL) {
        if (scheduler_.next != &scheduler_.idle_task) {
            scheduler_push_ready_front_(scheduler_.next);
        }

        scheduler_.next = NULL;
    }

    // Requeue the current task if it's still running and not the idle task.
    if (current != &scheduler_.idle_task &&
        current->state == TASK_STATE_RUNNING) {
        if (requeue_current == SCHEDULER_REQUEUE_FRONT) {
            scheduler_push_ready_front_(current);
        } else if (requeue_current == SCHEDULER_REQUEUE_BACK) {
            scheduler_push_ready_(current);
        }
    }

    // Select the next task to run. If it's the same as the current task, just
    // update its state to RUNNING and skip the context switch.
    next = scheduler_select_next_();
    if (next == current) {
        next->state = TASK_STATE_RUNNING;
        return false;
    }

    scheduler_.next = next;
    arch_context_switch_request();
    return true;
}

void scheduler_init(void)
{
    uint32_t priority;

    kernel_time_init();

    scheduler_.current = NULL;
    scheduler_.next = NULL;
    scheduler_.running = false;
    bitmap_init(&scheduler_.ready_bitmap,
                scheduler_ready_words_,
                SCHED_PRIORITY_LEVELS);

    for (priority = 0U; priority < SCHED_PRIORITY_LEVELS; priority++) {
        dlist_init(&scheduler_.ready_queues[priority]);
    }
}

void scheduler_add(task_t *task)
{
    ASSERT((task != NULL) &&
           (task->state == TASK_STATE_CREATED) &&
           (task->priority < SCHED_PRIORITY_LEVELS) &&
           dlist_node_is_detached(&task->sched_node));

    scheduler_make_ready(task);
}

__NO_RETURN void scheduler_start(void)
{
    ASSERT(!scheduler_.running);

    task_init(&scheduler_.idle_task,
              "idle",
              scheduler_idle_entry_,
              NULL,
              scheduler_idle_stack_,
              sizeof(scheduler_idle_stack_),
              (task_priority_t)(SCHED_PRIORITY_LEVELS - 1U));

    scheduler_.current = scheduler_pop_ready_();
    if (scheduler_.current == NULL) {
        scheduler_.current = &scheduler_.idle_task;
    }

    scheduler_.current->state = TASK_STATE_RUNNING;
    scheduler_.running = true;

    arch_context_start(scheduler_.current->sp);
}

void scheduler_yield(void)
{
    uint32_t key;

    ASSERT(!arch_in_isr());
    key = arch_irq_lock();

    if (!scheduler_.running) {
        arch_irq_unlock(key);
        return;
    }

    OS_DIAG_ASSERT((scheduler_.current != NULL) &&
                   (scheduler_.current->state == TASK_STATE_RUNNING));
    if (scheduler_.current == &scheduler_.idle_task) {
        (void)scheduler_request_switch_locked_(SCHEDULER_REQUEUE_NONE);
    } else {
        (void)scheduler_request_switch_locked_(SCHEDULER_REQUEUE_BACK);
    }
    arch_irq_unlock(key);
}

__NO_RETURN void scheduler_exit_current(void)
{
    uint32_t key;
    task_t *current;

    ASSERT(!arch_in_isr());
    key = arch_irq_lock();

    OS_DIAG_ASSERT(scheduler_.running &&
                   (scheduler_.current != NULL) &&
                   (scheduler_.current != &scheduler_.idle_task));

    current = scheduler_.current;
    current->state = TASK_STATE_TERMINATED;
#if OS_TRACE_ENABLE
    os_trace_task_exit(current);
#endif

    (void)scheduler_request_switch_locked_(SCHEDULER_REQUEUE_NONE);
    arch_irq_unlock(key);

    for (;;) {
    }
}

void scheduler_make_ready(task_t *task)
{
    uint32_t key;

    OS_DIAG_ASSERT((task != NULL) &&
                   ((task->state == TASK_STATE_CREATED) ||
                    (task->state == TASK_STATE_BLOCKED)) &&
                   (task->priority < SCHED_PRIORITY_LEVELS) &&
                   dlist_node_is_detached(&task->sched_node));

    key = arch_irq_lock();
    scheduler_make_ready_locked(task);
    arch_irq_unlock(key);
}

void scheduler_make_ready_locked(task_t *task)
{
    OS_DIAG_ASSERT((task != NULL) &&
                   ((task->state == TASK_STATE_CREATED) ||
                    (task->state == TASK_STATE_BLOCKED)) &&
                   (task->priority < SCHED_PRIORITY_LEVELS) &&
                   dlist_node_is_detached(&task->sched_node));

    scheduler_push_ready_(task);

    // Higher-priority READY tasks preempt;
    // the current task returns to the front of the ready queue to preserve same-priority order.
    if (scheduler_ready_task_should_preempt_(task)) {
        (void)scheduler_request_switch_locked_(SCHEDULER_REQUEUE_FRONT);
    }
}

void scheduler_block_current(void)
{
    uint32_t key;

    OS_DIAG_ASSERT(!arch_in_isr());

    key = arch_irq_lock();
    scheduler_block_current_locked();
    arch_irq_unlock(key);
}

void scheduler_block_current_locked(void)
{
    OS_DIAG_ASSERT(!arch_in_isr() &&
                   scheduler_.running &&
                   (scheduler_.current != NULL) &&
                   (scheduler_.current != &scheduler_.idle_task) &&
                   (scheduler_.current->state == TASK_STATE_RUNNING));

    scheduler_.current->state = TASK_STATE_BLOCKED;
#if OS_TRACE_ENABLE
    os_trace_task_block(scheduler_.current);
#endif
    (void)scheduler_request_switch_locked_(SCHEDULER_REQUEUE_NONE);
}

uint32_t *arch_context_switch_callback(uint32_t *saved_sp)
{
    task_t *from;
    task_t *to;

    OS_DIAG_ASSERT((scheduler_.current != NULL) && (scheduler_.next != NULL));

    from = scheduler_.current;
    to = scheduler_.next;

    from->sp = saved_sp;
    scheduler_.current = to;
    scheduler_.next = NULL;
    to->state = TASK_STATE_RUNNING;
#if OS_TRACE_ENABLE
    os_trace_task_switch(from, to);
#endif

    return to->sp;
}
