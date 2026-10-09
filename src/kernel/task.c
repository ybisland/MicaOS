#include <micaos/task.h>
#include "internal/kernel/scheduler_internal.h"
#include <micaos/common/assert.h>
#include "internal/arch/arch_context.h"

#if TASK_STACK_WATERMARK_ENABLE
#include <string.h>
#endif

void task_init(task_t *task,
               const char *name,
               task_entry_t entry,
               void *arg,
               void *stack,
               size_t stack_size,
               task_priority_t priority)
{
    OS_ASSERT((task != NULL) && (entry != NULL) && (stack != NULL));

    task->stack = stack;
    task->stack_size = stack_size;
    task->entry = entry;
    task->arg = arg;
    task->priority = priority;
    task->state = TASK_STATE_CREATED;
    task->wait_type = TASK_WAIT_NONE;
    task->wait_object = NULL;
    task->wake_tick = 0U;
    task->notify_pending = false;
    task->notify_result = false;
    task->eventset_wait_mask = 0U;
    task->eventset_wait_result = 0U;
    task->eventset_wait_all = false;
    task->sem_take_result = false;
    task->msgq_wait_result = false;
    task->pipe_wait_result = false;
    task->name = name;
    dlist_init(&task->sched_node);
    dlist_init(&task->timeout_node);

#if TASK_STACK_WATERMARK_ENABLE
    memset(stack, TASK_STACK_FILL_PATTERN, stack_size);
#endif

    task->sp = arch_context_init(stack, stack_size, entry, arg, task_exit);
}

task_t *task_current(void)
{
    return scheduler_current();
}

const char *task_get_name(const task_t *task)
{
    OS_ASSERT(task != NULL);

    return task->name;
}

task_priority_t task_get_priority(const task_t *task)
{
    OS_ASSERT(task != NULL);

    return task->priority;
}

task_state_t task_get_state(const task_t *task)
{
    uint32_t key;
    task_state_t state;

    OS_ASSERT(task != NULL);

    key = arch_irq_lock();
    state = task->state;
    arch_irq_unlock(key);

    return state;
}

task_wait_type_t task_get_wait_type(const task_t *task)
{
    uint32_t key;
    task_wait_type_t wait_type;

    OS_ASSERT(task != NULL);

    key = arch_irq_lock();
    wait_type = task->wait_type;
    arch_irq_unlock(key);

    return wait_type;
}

#if TASK_STACK_WATERMARK_ENABLE
size_t task_get_stack_unused(const task_t *task)
{
    const uint8_t *stack;
    size_t unused;

    OS_ASSERT(task != NULL);

    stack = (const uint8_t *)task->stack;
    unused = 0U;

    while ((unused < task->stack_size) &&
           (stack[unused] == (uint8_t)TASK_STACK_FILL_PATTERN)) {
        unused++;
    }

    return unused;
}

size_t task_get_stack_used(const task_t *task)
{
    OS_ASSERT(task != NULL);

    return task->stack_size - task_get_stack_unused(task);
}
#endif

void task_yield(void)
{
    scheduler_yield();
}

void task_notify(task_t *task)
{
    uint32_t key;

    OS_ASSERT((task != NULL) && (task->state != TASK_STATE_TERMINATED));

    key = arch_irq_lock();

    task->notify_pending = true;
    if ((task->state == TASK_STATE_BLOCKED) && (task->wait_type == TASK_WAIT_NOTIFY)) {
        task->notify_pending = false;
        task->notify_result = true;
        task->wait_type = TASK_WAIT_NONE;
        kernel_timeout_cancel_locked(task);
        scheduler_make_ready_locked(task);
    }

    arch_irq_unlock(key);
}

bool task_notify_wait(os_tick_t timeout)
{
    uint32_t key;
    bool result;
    task_t *current;

    OS_ASSERT(!arch_in_isr() &&
           ((timeout == OS_WAIT_FOREVER) || (timeout <= OS_TICK_MAX_DELAY)));

    key = arch_irq_lock();

    current = scheduler_current();
    OS_ASSERT((current != NULL) && (current->state == TASK_STATE_RUNNING));
    OS_DIAG_ASSERT((current->wait_type == TASK_WAIT_NONE) &&
                   (current->wait_object == NULL) &&
                   dlist_node_is_detached(&current->sched_node) &&
                   dlist_node_is_detached(&current->timeout_node));

    result = current->notify_pending;
    if (result || (timeout == OS_NO_WAIT)) {
        current->notify_pending = false;
        arch_irq_unlock(key);
        return result;
    }

    current->wait_type = TASK_WAIT_NOTIFY;
    current->notify_result = false;

    if (timeout != OS_WAIT_FOREVER) {
        kernel_timeout_start_locked(current, timeout);
    }

    scheduler_block_current_locked();
    arch_irq_unlock(key);

    return current->notify_result;
}

__NO_RETURN void task_exit(void)
{
    scheduler_exit_current();

    for (;;) {
    }
}
