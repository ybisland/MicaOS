#include "msgq.h"
#include "scheduler_internal.h"
#include "common/assert.h"
#include "arch/arch_context.h"

#include <string.h>

static uint8_t *msgq_slot_(msgq_t *q, uint16_t index)
{
    return &q->buffer[(size_t)index * q->msg_size];
}

static uint16_t msgq_next_index_(const msgq_t *q, uint16_t index)
{
    index++;
    return (index == q->capacity) ? 0U : index;
}

static void msgq_wake_one_locked_(dlist_t *wait_list, task_wait_type_t wait_type)
{
    // wake up one task from the wait_list and make it ready to run.
    dlist_node_t *node;
    task_t *task;

    node = dlist_pop_front(wait_list);
    if (node == NULL) {
        return;
    }

    task = dlist_entry(node, task_t, sched_node);
    OS_DIAG_ASSERT((task->wait_type == wait_type) &&
                   (task->wait_object != NULL));

    kernel_timeout_cancel_locked(task);
    task->msgq_wait_result = true;
    task->wait_object = NULL;
    task->wait_type = TASK_WAIT_NONE;
    scheduler_make_ready_locked(task);
}

static void msgq_copy_in_(msgq_t *q, const void *msg)
{
    memcpy(msgq_slot_(q, q->write), msg, q->msg_size);
    q->write = msgq_next_index_(q, q->write);
    q->count++;
}

static void msgq_copy_out_(msgq_t *q, void *msg)
{
    memcpy(msg, msgq_slot_(q, q->read), q->msg_size);
    q->read = msgq_next_index_(q, q->read);
    q->count--;
}

static os_tick_t msgq_remaining_timeout_(os_tick_t deadline)
{
    os_tick_t now;

    now = os_tick_get();
    if (os_tick_after_eq(now, deadline)) {
        return 0U;
    }

    return (os_tick_t)(deadline - now);
}

void msgq_init(msgq_t *q, void *buffer, uint16_t msg_size, uint16_t capacity)
{
    OS_ASSERT((q != NULL) &&
           (buffer != NULL) &&
           (msg_size != 0U) &&
           (capacity != 0U));

    q->buffer = (uint8_t *)buffer;
    q->msg_size = msg_size;
    q->capacity = capacity;
    q->read = 0U;
    q->write = 0U;
    q->count = 0U;
    dlist_init(&q->send_wait_list);
    dlist_init(&q->recv_wait_list);
}

bool msgq_send(msgq_t *q, const void *msg, os_tick_t timeout)
{
    // Enqueue the message. Block the task if the queue is full,
    // until a message is dequeued or timeout elapses.
    // Return false immediately if timeout == OS_NO_WAIT (non-blocking).
    uint32_t key;
    task_t *current;
    os_tick_t deadline;
    os_tick_t wait_ticks;
    bool finite_timeout;

    OS_ASSERT((q != NULL) &&
           (msg != NULL) &&
           (!arch_in_isr() || (timeout == OS_NO_WAIT)) &&
           ((timeout == OS_WAIT_FOREVER) || (timeout <= OS_TICK_MAX_DELAY)));

    finite_timeout = (timeout != OS_NO_WAIT) && (timeout != OS_WAIT_FOREVER);
    if (finite_timeout) {
        deadline = os_tick_get() + timeout;
    } else {
        deadline = 0U;
    }

    for (;;) {
        key = arch_irq_lock();

        // if queue is not full, copy the message and wake one receiver
        if (q->count < q->capacity) {
            msgq_copy_in_(q, msg);
            msgq_wake_one_locked_(&q->recv_wait_list, TASK_WAIT_MSGQ_RECV);
            arch_irq_unlock(key);
            return true;
        }

        // if queue is full and timeout is OS_NO_WAIT, return false immediately
        if (timeout == OS_NO_WAIT) {
            arch_irq_unlock(key);
            return false;
        }

        // if queue is full and timeout is not OS_NO_WAIT, wait for a receiver or timeout.
        wait_ticks = timeout;
        if (finite_timeout) {
            wait_ticks = msgq_remaining_timeout_(deadline);
            if (wait_ticks == 0U) {
                arch_irq_unlock(key);
                return false;
            }
        }

        // otherwise, block the task, put it into send_wait_list, and start a timeout if needed
        current = scheduler_current();
        OS_ASSERT((current != NULL) && (current->state == TASK_STATE_RUNNING));
        OS_DIAG_ASSERT((current->wait_type == TASK_WAIT_NONE) &&
                       (current->wait_object == NULL) &&
                       dlist_node_is_detached(&current->sched_node) &&
                       dlist_node_is_detached(&current->timeout_node));

        current->wait_type = TASK_WAIT_MSGQ_SEND;
        current->wait_object = q;
        current->msgq_wait_result = false;
        dlist_push_back(&q->send_wait_list, &current->sched_node);

        // if timeout is finite, start a timeout for the task,
        if (wait_ticks != OS_WAIT_FOREVER) {
            kernel_timeout_start_locked(current, wait_ticks);
        }

        // block the task and switch to another task.
        scheduler_block_current_locked();
        arch_irq_unlock(key);

        // when the task is woken up, check if it was woken up by a timeout or by a receiver.
        // if it was woken up by a timeout, return false. If it was woken up by a receiver,
        // msgq_wake_one_locked_() sets current->msgq_wait_result to true, so we go to the
        // next iteration of the loop to try to send the message again.
        if (!current->msgq_wait_result) {
            return false;
        }
    }
}

bool msgq_recv(msgq_t *q, void *msg, os_tick_t timeout)
{
    // Take one message from the queue. Block the task if the queue is empty,
    // until a message is sent or timeout elapses.
    // Return false immediately if timeout == OS_NO_WAIT (non-blocking).
    uint32_t key;
    task_t *current;
    os_tick_t deadline;
    os_tick_t wait_ticks;
    bool finite_timeout;

    OS_ASSERT((q != NULL) &&
           (msg != NULL) &&
           (!arch_in_isr() || (timeout == OS_NO_WAIT)) &&
           ((timeout == OS_WAIT_FOREVER) || (timeout <= OS_TICK_MAX_DELAY)));

    finite_timeout = (timeout != OS_NO_WAIT) && (timeout != OS_WAIT_FOREVER);
    if (finite_timeout) {
        deadline = os_tick_get() + timeout;
    } else {
        deadline = 0U;
    }

    for (;;) {
        key = arch_irq_lock();

        // if queue is not empty, copy the message and wake one sender, and return true
        if (q->count > 0U) {
            msgq_copy_out_(q, msg);
            msgq_wake_one_locked_(&q->send_wait_list, TASK_WAIT_MSGQ_SEND);
            arch_irq_unlock(key);
            return true;
        }

        // if queue is empty and timeout is OS_NO_WAIT, return false immediately
        if (timeout == OS_NO_WAIT) {
            arch_irq_unlock(key);
            return false;
        }

        // if queue is empty and timeout is not OS_NO_WAIT, wait for a message or timeout.
        wait_ticks = timeout;
        if (finite_timeout) {
            wait_ticks = msgq_remaining_timeout_(deadline);
            if (wait_ticks == 0U) {
                arch_irq_unlock(key);
                return false;
            }
        }

        // otherwise, block the task, put it into recv_wait_list, and start a timeout if needed
        current = scheduler_current();
        OS_ASSERT((current != NULL) && (current->state == TASK_STATE_RUNNING));
        OS_DIAG_ASSERT((current->wait_type == TASK_WAIT_NONE) &&
                       (current->wait_object == NULL) &&
                       dlist_node_is_detached(&current->sched_node) &&
                       dlist_node_is_detached(&current->timeout_node));

        current->wait_type = TASK_WAIT_MSGQ_RECV;
        current->wait_object = q;
        current->msgq_wait_result = false;
        dlist_push_back(&q->recv_wait_list, &current->sched_node);

        if (wait_ticks != OS_WAIT_FOREVER) {
            kernel_timeout_start_locked(current, wait_ticks);
        }

        // block the task and switch to another task.
        scheduler_block_current_locked();
        arch_irq_unlock(key);

        // when the task is woken up, check if it was woken up by a timeout or by a sender.
        // if it was woken up by a timeout, return false. If it was woken up by a sender,
        // msgq_wake_one_locked_() sets current->msgq_wait_result to true, so we go to the
        // next iteration of the loop to try to receive the message again.
        if (!current->msgq_wait_result) {
            return false;
        }
    }
}

uint16_t msgq_count(msgq_t *q)
{
    uint32_t key;
    uint16_t count;

    OS_ASSERT(q != NULL);

    key = arch_irq_lock();
    count = q->count;
    arch_irq_unlock(key);

    return count;
}

uint16_t msgq_space(msgq_t *q)
{
    uint32_t key;
    uint16_t space;

    OS_ASSERT(q != NULL);

    key = arch_irq_lock();
    space = (uint16_t)(q->capacity - q->count);
    arch_irq_unlock(key);

    return space;
}
