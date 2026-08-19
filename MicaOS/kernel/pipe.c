#include "pipe.h"
#include "scheduler_internal.h"
#include "common/assert.h"
#include "arch/arch_context.h"

static os_tick_t pipe_remaining_timeout_(os_tick_t deadline)
{
    os_tick_t now;

    now = os_tick_get();
    if (os_tick_after_eq(now, deadline)) {
        return 0U;
    }

    return (os_tick_t)(deadline - now);
}

static void pipe_wake_reader_locked_(pipe_t *pipe)
{
    task_t *task;

    task = pipe->reader_waiter;
    if (task == NULL) {
        return;
    }

    OS_DIAG_ASSERT((task->wait_type == TASK_WAIT_PIPE_READ) &&
                   (task->wait_object == pipe));

    pipe->reader_waiter = NULL;
    kernel_timeout_cancel_locked(task);
    task->pipe_wait_result = true;
    task->wait_object = NULL;
    task->wait_type = TASK_WAIT_NONE;
    scheduler_make_ready_locked(task);
}

static void pipe_wake_writer_locked_(pipe_t *pipe)
{
    task_t *task;

    task = pipe->writer_waiter;
    if (task == NULL) {
        return;
    }

    OS_DIAG_ASSERT((task->wait_type == TASK_WAIT_PIPE_WRITE) &&
                   (task->wait_object == pipe));

    pipe->writer_waiter = NULL;
    kernel_timeout_cancel_locked(task);
    task->pipe_wait_result = true;
    task->wait_object = NULL;
    task->wait_type = TASK_WAIT_NONE;
    scheduler_make_ready_locked(task);
}

static void pipe_abort_reader_locked_(pipe_t *pipe)
{
    task_t *task;

    task = pipe->reader_waiter;
    if (task == NULL) {
        return;
    }

    OS_DIAG_ASSERT((task->wait_type == TASK_WAIT_PIPE_READ) &&
                   (task->wait_object == pipe));

    pipe->reader_waiter = NULL;
    kernel_timeout_cancel_locked(task);
    task->pipe_wait_result = false;
    task->wait_object = NULL;
    task->wait_type = TASK_WAIT_NONE;
    scheduler_make_ready_locked(task);
}

static void pipe_abort_writer_locked_(pipe_t *pipe)
{
    task_t *task;

    task = pipe->writer_waiter;
    if (task == NULL) {
        return;
    }

    OS_DIAG_ASSERT((task->wait_type == TASK_WAIT_PIPE_WRITE) &&
                   (task->wait_object == pipe));

    pipe->writer_waiter = NULL;
    kernel_timeout_cancel_locked(task);
    task->pipe_wait_result = false;
    task->wait_object = NULL;
    task->wait_type = TASK_WAIT_NONE;
    scheduler_make_ready_locked(task);
}

static task_t *pipe_current_waiter_(void)
{
    task_t *current;

    current = scheduler_current();
    OS_ASSERT((current != NULL) && (current->state == TASK_STATE_RUNNING));
    OS_DIAG_ASSERT((current->wait_type == TASK_WAIT_NONE) &&
                   (current->wait_object == NULL) &&
                   dlist_node_is_detached(&current->sched_node) &&
                   dlist_node_is_detached(&current->timeout_node));

    return current;
}

void pipe_timeout_locked(task_t *task)
{
    pipe_t *pipe;

    OS_DIAG_ASSERT((task != NULL) && (task->wait_object != NULL));

    pipe = (pipe_t *)task->wait_object;

    if (task->wait_type == TASK_WAIT_PIPE_READ) {
        OS_DIAG_ASSERT(pipe->reader_waiter == task);
        pipe->reader_waiter = NULL;
    } else {
        OS_DIAG_ASSERT((task->wait_type == TASK_WAIT_PIPE_WRITE) &&
                       (pipe->writer_waiter == task));
        pipe->writer_waiter = NULL;
    }

    task->pipe_wait_result = false;
}

void pipe_init(pipe_t *pipe, void *buffer, uint16_t size)
{
    OS_ASSERT((pipe != NULL) && (buffer != NULL) && (size != 0U));

    bytebuf_init(&pipe->buf, (uint8_t *)buffer, size);
    pipe->reader_waiter = NULL;
    pipe->writer_waiter = NULL;
}

uint16_t pipe_write(pipe_t *pipe,
                    const void *data,
                    uint16_t len,
                    os_tick_t timeout)
{
    uint32_t key;
    uint32_t written;
    os_tick_t deadline;
    os_tick_t wait_ticks;
    task_t *current;
    bool finite_timeout;

    OS_ASSERT((pipe != NULL) &&
           ((data != NULL) || (len == 0U)) &&
           (!arch_in_isr() || (timeout == OS_NO_WAIT)) &&
           ((timeout == OS_WAIT_FOREVER) || (timeout <= OS_TICK_MAX_DELAY)));

    if (len == 0U) {
        return 0U;
    }

    finite_timeout = (timeout != OS_NO_WAIT) && (timeout != OS_WAIT_FOREVER);
    if (finite_timeout) {
        deadline = os_tick_get() + timeout;
    } else {
        deadline = 0U;
    }

    for (;;) {
        key = arch_irq_lock();

        written = bytebuf_put(&pipe->buf, (const uint8_t *)data, len);
        if (written != 0U) {
            pipe_wake_reader_locked_(pipe);
            arch_irq_unlock(key);
            return (uint16_t)written;
        }

        if (timeout == OS_NO_WAIT) {
            arch_irq_unlock(key);
            return 0U;
        }

        wait_ticks = timeout;
        if (finite_timeout) {
            wait_ticks = pipe_remaining_timeout_(deadline);
            if (wait_ticks == 0U) {
                arch_irq_unlock(key);
                return 0U;
            }
        }

        OS_DIAG_ASSERT(pipe->writer_waiter == NULL);

        current = pipe_current_waiter_();
        current->wait_type = TASK_WAIT_PIPE_WRITE;
        current->wait_object = pipe;
        current->pipe_wait_result = false;
        pipe->writer_waiter = current;

        if (wait_ticks != OS_WAIT_FOREVER) {
            kernel_timeout_start_locked(current, wait_ticks);
        }

        scheduler_block_current_locked();
        arch_irq_unlock(key);

        if (!current->pipe_wait_result) {
            return 0U;
        }
    }
}

uint16_t pipe_read(pipe_t *pipe,
                   void *data,
                   uint16_t len,
                   os_tick_t timeout)
{
    uint32_t key;
    uint32_t read;
    os_tick_t deadline;
    os_tick_t wait_ticks;
    task_t *current;
    bool finite_timeout;

    OS_ASSERT((pipe != NULL) &&
           ((data != NULL) || (len == 0U)) &&
           (!arch_in_isr() || (timeout == OS_NO_WAIT)) &&
           ((timeout == OS_WAIT_FOREVER) || (timeout <= OS_TICK_MAX_DELAY)));

    if (len == 0U) {
        return 0U;
    }

    finite_timeout = (timeout != OS_NO_WAIT) && (timeout != OS_WAIT_FOREVER);
    if (finite_timeout) {
        deadline = os_tick_get() + timeout;
    } else {
        deadline = 0U;
    }

    for (;;) {
        key = arch_irq_lock();

        read = bytebuf_get(&pipe->buf, (uint8_t *)data, len);
        if (read != 0U) {
            pipe_wake_writer_locked_(pipe);
            arch_irq_unlock(key);
            return (uint16_t)read;
        }

        if (timeout == OS_NO_WAIT) {
            arch_irq_unlock(key);
            return 0U;
        }

        wait_ticks = timeout;
        if (finite_timeout) {
            wait_ticks = pipe_remaining_timeout_(deadline);
            if (wait_ticks == 0U) {
                arch_irq_unlock(key);
                return 0U;
            }
        }

        OS_DIAG_ASSERT(pipe->reader_waiter == NULL);

        current = pipe_current_waiter_();
        current->wait_type = TASK_WAIT_PIPE_READ;
        current->wait_object = pipe;
        current->pipe_wait_result = false;
        pipe->reader_waiter = current;

        if (wait_ticks != OS_WAIT_FOREVER) {
            kernel_timeout_start_locked(current, wait_ticks);
        }

        scheduler_block_current_locked();
        arch_irq_unlock(key);

        if (!current->pipe_wait_result) {
            return 0U;
        }
    }
}

uint16_t pipe_count(pipe_t *pipe)
{
    uint32_t key;
    uint16_t count;

    OS_ASSERT(pipe != NULL);

    key = arch_irq_lock();
    count = (uint16_t)bytebuf_size(&pipe->buf);
    arch_irq_unlock(key);

    return count;
}

uint16_t pipe_space(pipe_t *pipe)
{
    uint32_t key;
    uint16_t space;

    OS_ASSERT(pipe != NULL);

    key = arch_irq_lock();
    space = (uint16_t)bytebuf_space(&pipe->buf);
    arch_irq_unlock(key);

    return space;
}

void pipe_reset(pipe_t *pipe)
{
    uint32_t key;

    OS_ASSERT(pipe != NULL);

    key = arch_irq_lock();
    bytebuf_reset(&pipe->buf);
    pipe_abort_reader_locked_(pipe);
    pipe_abort_writer_locked_(pipe);
    arch_irq_unlock(key);
}
