#include <micaos/sem.h>
#include "internal/kernel/scheduler_internal.h"
#include <micaos/common/assert.h>
#include "internal/arch/arch_context.h"

void sem_init(sem_t *sem, uint16_t initial, uint16_t limit)
{
    OS_ASSERT((sem != NULL) && (limit != 0U) && (initial <= limit));

    sem->count = initial;
    sem->limit = limit;
    dlist_init(&sem->wait_list);
}

void sem_give(sem_t *sem)
{
    uint32_t key;
    dlist_node_t *node;
    task_t *task;

    OS_ASSERT(sem != NULL);

    key = arch_irq_lock();

    // Waiters are queued FIFO: wake the oldest waiter instead of incrementing the count.
    // If no one is waiting, increment the count up to the limit.
    node = dlist_pop_front(&sem->wait_list);
    if (node != NULL) {
        task = dlist_entry(node, task_t, sched_node);
        OS_DIAG_ASSERT((task->wait_type == TASK_WAIT_SEM) &&
                       (task->wait_object == sem));
        kernel_timeout_cancel_locked(task);
        task->sem_take_result = true;
        task->wait_object = NULL;
        task->wait_type = TASK_WAIT_NONE;
        scheduler_make_ready_locked(task);
    } else if (sem->count < sem->limit) {
        sem->count++;
    }

    arch_irq_unlock(key);
}

bool sem_take(sem_t *sem, os_tick_t timeout)
{
    uint32_t key;
    task_t *current;

    OS_ASSERT((sem != NULL) &&
           !arch_in_isr() &&
           ((timeout == OS_WAIT_FOREVER) || (timeout <= OS_TICK_MAX_DELAY)));

    key = arch_irq_lock();

    if (sem->count > 0U) {
        sem->count--;
        arch_irq_unlock(key);
        return true;
    }

    if (timeout == OS_NO_WAIT) {
        arch_irq_unlock(key);
        return false;
    }

    current = scheduler_current();
    OS_ASSERT((current != NULL) && (current->state == TASK_STATE_RUNNING));
    OS_DIAG_ASSERT((current->wait_type == TASK_WAIT_NONE) &&
                   (current->wait_object == NULL) &&
                   dlist_node_is_detached(&current->sched_node) &&
                   dlist_node_is_detached(&current->timeout_node));

    current->wait_type = TASK_WAIT_SEM;
    current->wait_object = sem;
    current->sem_take_result = false;
    dlist_push_back(&sem->wait_list, &current->sched_node);

    if (timeout != OS_WAIT_FOREVER) {
        kernel_timeout_start_locked(current, timeout);
    }

    scheduler_block_current_locked();
    arch_irq_unlock(key);

    return current->sem_take_result;
}
