#include "eventset.h"
#include "scheduler_internal.h"
#include "common/assert.h"
#include "arch/arch_context.h"

static eventset_bits_t eventset_match_(eventset_bits_t current,
                                       eventset_bits_t mask,
                                       bool wait_all)
{
    eventset_bits_t matched;

    matched = current & mask;
    if (wait_all) {
        return (matched == mask) ? mask : 0U;
    }

    return matched;
}

static void eventset_wake_ready_locked_(eventset_t *eventset)
{
    dlist_node_t *node;
    dlist_node_t *next;

    dlist_for_each_safe(node, next, &eventset->wait_list) {
        task_t *task = dlist_entry(node, task_t, sched_node);
        eventset_bits_t result;

        OS_DIAG_ASSERT((task->wait_type == TASK_WAIT_EVENTSET) &&
                       (task->wait_object == eventset));

        result = eventset_match_(eventset->bits,
                                 task->eventset_wait_mask,
                                 task->eventset_wait_all);
        if (result == 0U) {
            continue;
        }

        dlist_remove(&task->sched_node);
        kernel_timeout_cancel_locked(task);

        task->eventset_wait_mask = 0U;
        task->eventset_wait_result = result;
        task->eventset_wait_all = false;
        task->wait_object = NULL;
        task->wait_type = TASK_WAIT_NONE;

        scheduler_make_ready_locked(task);
    }
}

static eventset_bits_t eventset_wait_(eventset_t *eventset,
                                      eventset_bits_t mask,
                                      bool wait_all,
                                      os_tick_t timeout)
{
    uint32_t key;
    task_t *current;
    eventset_bits_t result;

    OS_ASSERT((eventset != NULL) &&
           (mask != 0U) &&
           !arch_in_isr() &&
           ((timeout == OS_WAIT_FOREVER) || (timeout <= OS_TICK_MAX_DELAY)));

    key = arch_irq_lock();

    result = eventset_match_(eventset->bits, mask, wait_all);
    if ((result != 0U) || (timeout == OS_NO_WAIT)) {
        arch_irq_unlock(key);
        return result;
    }

    current = scheduler_current();
    OS_ASSERT((current != NULL) && (current->state == TASK_STATE_RUNNING));
    OS_DIAG_ASSERT((current->wait_type == TASK_WAIT_NONE) &&
                   (current->wait_object == NULL) &&
                   dlist_node_is_detached(&current->sched_node) &&
                   dlist_node_is_detached(&current->timeout_node));

    current->wait_type = TASK_WAIT_EVENTSET;
    current->wait_object = eventset;
    current->eventset_wait_mask = mask;
    current->eventset_wait_result = 0U;
    current->eventset_wait_all = wait_all;

    dlist_push_back(&eventset->wait_list, &current->sched_node);

    if (timeout != OS_WAIT_FOREVER) {
        kernel_timeout_start_locked(current, timeout);
    }

    scheduler_block_current_locked();
    arch_irq_unlock(key);

    return current->eventset_wait_result;
}

void eventset_init(eventset_t *eventset)
{
    OS_ASSERT(eventset != NULL);

    eventset->bits = 0U;
    dlist_init(&eventset->wait_list);
}

void eventset_set(eventset_t *eventset, eventset_bits_t bits)
{
    uint32_t key;

    OS_ASSERT((eventset != NULL) && (bits != 0U));

    key = arch_irq_lock();
    eventset->bits |= bits;
    eventset_wake_ready_locked_(eventset);
    arch_irq_unlock(key);
}

void eventset_clear(eventset_t *eventset, eventset_bits_t bits)
{
    uint32_t key;

    OS_ASSERT((eventset != NULL) && (bits != 0U));

    key = arch_irq_lock();
    eventset->bits &= ~bits;
    arch_irq_unlock(key);
}

eventset_bits_t eventset_get(eventset_t *eventset)
{
    uint32_t key;
    eventset_bits_t bits;

    OS_ASSERT(eventset != NULL);

    key = arch_irq_lock();
    bits = eventset->bits;
    arch_irq_unlock(key);

    return bits;
}

eventset_bits_t eventset_wait_any(eventset_t *eventset,
                                  eventset_bits_t mask,
                                  os_tick_t timeout)
{
    return eventset_wait_(eventset, mask, false, timeout);
}

eventset_bits_t eventset_wait_all(eventset_t *eventset,
                                  eventset_bits_t mask,
                                  os_tick_t timeout)
{
    return eventset_wait_(eventset, mask, true, timeout);
}
