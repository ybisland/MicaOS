#include "time.h"
#include "scheduler_internal.h"
#include "common/assert.h"
#include "arch/arch_context.h"

/**
 * Tick values are unsigned(UINT32) and wrap naturally. Any code that compares
 * two tick timestamps, or computes elapsed time between them, must keep the real
 * distance below half of the counter range (OS_TICK_MAX_DELAY). Within
 * that window the signed difference has one unambiguous direction; outside it,
 * "before" and "after" cannot be distinguished reliably after wraparound.
 */
static volatile os_tick_t os_tick_count_;

/**
 * Delayed or timeout-waiting tasks are linked here with task_t::timeout_node.
 * The list is sorted by wake_tick in ascending order, so the front task is the
 * next to wake up. If two tasks have the same wake_tick, new tasks are inserted
 * after existing tasks.
 */
static dlist_t kernel_sleep_list_ = dlist_static_init(kernel_sleep_list_);

static void kernel_sleep_insert_locked_(task_t *task)
{
    // insert task into kernel_sleep_list_ sorted by wake_tick, with new tasks
    // inserted after existing tasks with the same wake_tick.
    dlist_node_t *node;

    OS_DIAG_ASSERT((task != NULL) &&
                   dlist_node_is_detached(&task->timeout_node));

    dlist_for_each(node, &kernel_sleep_list_) {
        task_t *entry = dlist_entry(node, task_t, timeout_node);

        if (!os_tick_after_eq(task->wake_tick, entry->wake_tick)) {
            dlist_insert_before(node, &task->timeout_node);
            return;
        }
    }

    dlist_push_back(&kernel_sleep_list_, &task->timeout_node);
}

static void kernel_wake_expired_locked_(os_tick_t now)
{
    // wake tasks from kernel_sleep_list_ whose wake_tick is now or in the past
    dlist_node_t *node;

    for (;;) {
        task_t *task;

        node = dlist_peek_front(&kernel_sleep_list_);
        if (node == NULL) {
            return;
        }

        task = dlist_entry(node, task_t, timeout_node);
        if (!os_tick_after_eq(now, task->wake_tick)) {
            return;
        }

        dlist_remove(&task->timeout_node);

        if (task->wait_type == TASK_WAIT_NOTIFY) {
            task->notify_result = false;
        } else if (task->wait_type == TASK_WAIT_EVENTSET) {
            OS_DIAG_ASSERT(task->wait_object != NULL);
            if (!dlist_node_is_detached(&task->sched_node)) {
                dlist_remove(&task->sched_node);
            }
            task->eventset_wait_mask = 0U;
            task->eventset_wait_result = 0U;
            task->eventset_wait_all = false;
        } else if (task->wait_type == TASK_WAIT_SEM) {
            OS_DIAG_ASSERT(task->wait_object != NULL);
            if (!dlist_node_is_detached(&task->sched_node)) {
                dlist_remove(&task->sched_node);
            }
            task->sem_take_result = false;
        } else if ((task->wait_type == TASK_WAIT_MSGQ_SEND) ||
                   (task->wait_type == TASK_WAIT_MSGQ_RECV)) {
            OS_DIAG_ASSERT(task->wait_object != NULL);
            if (!dlist_node_is_detached(&task->sched_node)) {
                dlist_remove(&task->sched_node);
            }
            task->msgq_wait_result = false;
        } else if ((task->wait_type == TASK_WAIT_PIPE_WRITE) ||
                   (task->wait_type == TASK_WAIT_PIPE_READ)) {
            pipe_timeout_locked(task);
        } else {
            OS_DIAG_ASSERT(task->wait_type == TASK_WAIT_DELAY);
        }

        task->wait_object = NULL;
        task->wait_type = TASK_WAIT_NONE;
        scheduler_make_ready_locked(task);
    }
}

void kernel_time_init(void)
{
    os_tick_count_ = 0U;
    dlist_init(&kernel_sleep_list_);
#if OS_TIMER_ENABLE
    kernel_timer_init();
#endif
}

void kernel_timeout_start_locked(task_t *task, os_tick_t ticks)
{
    OS_DIAG_ASSERT((task != NULL) &&
                   (ticks != 0U) &&
                   (ticks <= OS_TICK_MAX_DELAY) &&
                   dlist_node_is_detached(&task->timeout_node));

    task->wake_tick = os_tick_count_ + ticks;
    kernel_sleep_insert_locked_(task);
}

void kernel_timeout_cancel_locked(task_t *task)
{
    OS_DIAG_ASSERT(task != NULL);

    if (!dlist_node_is_detached(&task->timeout_node)) {
        dlist_remove(&task->timeout_node);
    }
}

os_tick_t os_tick_get(void)
{
    uint32_t key;
    os_tick_t ticks;

    key = arch_irq_lock();
    ticks = os_tick_count_;
    arch_irq_unlock(key);

    return ticks;
}

os_tick_t os_tick_get_locked(void)
{
    return os_tick_count_;
}

void os_tick_advance(void)
{
    uint32_t key;
    os_tick_t now;

    key = arch_irq_lock();
    os_tick_count_++;
    now = os_tick_count_;
    kernel_wake_expired_locked_(now);
    arch_irq_unlock(key);

#if OS_TIMER_ENABLE
    kernel_timer_advance(now);
#endif
}

void task_delay(os_tick_t ticks)
{
    uint32_t key;
    task_t *current;

    if (ticks == 0U) {
        task_yield();
        return;
    }

    OS_ASSERT(!arch_in_isr() && (ticks <= OS_TICK_MAX_DELAY));

    key = arch_irq_lock();

    current = scheduler_current();
    OS_ASSERT((current != NULL) && (current->state == TASK_STATE_RUNNING));
    OS_DIAG_ASSERT((current->wait_type == TASK_WAIT_NONE) &&
                   (current->wait_object == NULL) &&
                   dlist_node_is_detached(&current->sched_node) &&
                   dlist_node_is_detached(&current->timeout_node));

    current->wait_type = TASK_WAIT_DELAY;
    kernel_timeout_start_locked(current, ticks);
    scheduler_block_current_locked();

    arch_irq_unlock(key);
}
