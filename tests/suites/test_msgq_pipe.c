#include "tests/test.h"
#include "tests/host/kernel_host_stub.h"
#include "kernel/msgq.h"
#include "kernel/pipe.h"

#include <string.h>

typedef struct test_msg {
    uint32_t id;
    uint32_t value;
} test_msg_t;

static void test_task_prepare_blocked(task_t *task,
                                      task_wait_type_t wait_type,
                                      void *wait_object)
{
    memset(task, 0, sizeof(*task));
    dlist_init(&task->sched_node);
    dlist_init(&task->timeout_node);
    task->state = TASK_STATE_BLOCKED;
    task->wait_type = wait_type;
    task->wait_object = wait_object;
}

static void test_task_prepare_running(task_t *task)
{
    memset(task, 0, sizeof(*task));
    dlist_init(&task->sched_node);
    dlist_init(&task->timeout_node);
    task->state = TASK_STATE_RUNNING;
}

static void test_bytes_eq(const uint8_t *expected,
                          const uint8_t *actual,
                          size_t len)
{
    size_t i;

    for (i = 0U; i < len; i++) {
        TEST_EQ_U32(expected[i], actual[i]);
    }
}

static void test_msgq_init_sets_empty_count_and_space(void)
{
    msgq_t q;
    msgq_storage(storage, test_msg_t, 3);

    msgq_init(&q, storage, sizeof(test_msg_t), 3U);

    TEST_EQ_PTR(storage, q.buffer);
    TEST_EQ_U32(sizeof(test_msg_t), q.msg_size);
    TEST_EQ_U32(3U, q.capacity);
    TEST_EQ_U32(0U, msgq_count(&q));
    TEST_EQ_U32(3U, msgq_space(&q));
    TEST_ASSERT(dlist_empty(&q.send_wait_list));
    TEST_ASSERT(dlist_empty(&q.recv_wait_list));
}

static void test_msgq_send_recv_preserves_fifo_order(void)
{
    msgq_t q;
    msgq_storage(storage, test_msg_t, 3);
    test_msg_t in1 = { 1U, 10U };
    test_msg_t in2 = { 2U, 20U };
    test_msg_t out = { 0U, 0U };

    msgq_init(&q, storage, sizeof(test_msg_t), 3U);

    TEST_ASSERT(msgq_send(&q, &in1, OS_NO_WAIT));
    TEST_ASSERT(msgq_send(&q, &in2, OS_NO_WAIT));
    TEST_EQ_U32(2U, msgq_count(&q));
    TEST_EQ_U32(1U, msgq_space(&q));

    TEST_ASSERT(msgq_recv(&q, &out, OS_NO_WAIT));
    TEST_EQ_U32(1U, out.id);
    TEST_EQ_U32(10U, out.value);
    TEST_ASSERT(msgq_recv(&q, &out, OS_NO_WAIT));
    TEST_EQ_U32(2U, out.id);
    TEST_EQ_U32(20U, out.value);
    TEST_EQ_U32(0U, msgq_count(&q));
    TEST_EQ_U32(3U, msgq_space(&q));
}

static void test_msgq_full_and_empty_no_wait_return_false(void)
{
    msgq_t q;
    msgq_storage(storage, test_msg_t, 1);
    test_msg_t msg = { 7U, 70U };
    test_msg_t out = { 0U, 0U };

    msgq_init(&q, storage, sizeof(test_msg_t), 1U);

    TEST_ASSERT(!msgq_recv(&q, &out, OS_NO_WAIT));
    TEST_ASSERT(msgq_send(&q, &msg, OS_NO_WAIT));
    TEST_ASSERT(!msgq_send(&q, &msg, OS_NO_WAIT));
    TEST_EQ_U32(1U, msgq_count(&q));
    TEST_EQ_U32(0U, msgq_space(&q));
}

static void test_msgq_send_wakes_manual_blocked_receiver(void)
{
    msgq_t q;
    msgq_storage(storage, test_msg_t, 2);
    task_t receiver;
    test_msg_t msg = { 3U, 30U };

    kernel_host_reset();
    msgq_init(&q, storage, sizeof(test_msg_t), 2U);
    test_task_prepare_blocked(&receiver, TASK_WAIT_MSGQ_RECV, &q);
    dlist_push_back(&q.recv_wait_list, &receiver.sched_node);

    TEST_ASSERT(msgq_send(&q, &msg, OS_NO_WAIT));

    TEST_EQ_U32(1U, msgq_count(&q));
    TEST_ASSERT(dlist_empty(&q.recv_wait_list));
    TEST_ASSERT(dlist_node_is_detached(&receiver.sched_node));
    TEST_EQ_U32(TASK_STATE_READY, receiver.state);
    TEST_EQ_U32(TASK_WAIT_NONE, receiver.wait_type);
    TEST_EQ_PTR(NULL, receiver.wait_object);
    TEST_ASSERT(receiver.msgq_wait_result);
    TEST_EQ_U32(1U, kernel_host_ready_count());
}

static void test_msgq_recv_wakes_manual_blocked_sender(void)
{
    msgq_t q;
    msgq_storage(storage, test_msg_t, 1);
    task_t sender;
    test_msg_t in = { 4U, 40U };
    test_msg_t out = { 0U, 0U };

    kernel_host_reset();
    msgq_init(&q, storage, sizeof(test_msg_t), 1U);
    TEST_ASSERT(msgq_send(&q, &in, OS_NO_WAIT));
    test_task_prepare_blocked(&sender, TASK_WAIT_MSGQ_SEND, &q);
    dlist_push_back(&q.send_wait_list, &sender.sched_node);

    TEST_ASSERT(msgq_recv(&q, &out, OS_NO_WAIT));

    TEST_EQ_U32(4U, out.id);
    TEST_EQ_U32(40U, out.value);
    TEST_EQ_U32(0U, msgq_count(&q));
    TEST_ASSERT(dlist_empty(&q.send_wait_list));
    TEST_ASSERT(dlist_node_is_detached(&sender.sched_node));
    TEST_EQ_U32(TASK_STATE_READY, sender.state);
    TEST_EQ_U32(TASK_WAIT_NONE, sender.wait_type);
    TEST_EQ_PTR(NULL, sender.wait_object);
    TEST_ASSERT(sender.msgq_wait_result);
    TEST_EQ_U32(1U, kernel_host_ready_count());
}

static void test_pipe_init_sets_empty_count_and_space(void)
{
    pipe_t pipe;
    pipe_storage(storage, 5);

    pipe_init(&pipe, storage, sizeof(storage));

    TEST_EQ_U32(0U, pipe_count(&pipe));
    TEST_EQ_U32(5U, pipe_space(&pipe));
    TEST_EQ_PTR(NULL, pipe.reader_waiter);
    TEST_EQ_PTR(NULL, pipe.writer_waiter);
}

static void test_pipe_write_read_preserves_fifo_order(void)
{
    pipe_t pipe;
    pipe_storage(storage, 6);
    const uint8_t in[] = { 1U, 2U, 3U, 4U };
    uint8_t out[4] = { 0U };

    pipe_init(&pipe, storage, sizeof(storage));

    TEST_EQ_U32(4U, pipe_write(&pipe, in, sizeof(in), OS_NO_WAIT));
    TEST_EQ_U32(4U, pipe_count(&pipe));
    TEST_EQ_U32(2U, pipe_space(&pipe));
    TEST_EQ_U32(4U, pipe_read(&pipe, out, sizeof(out), OS_NO_WAIT));
    test_bytes_eq(in, out, sizeof(in));
    TEST_EQ_U32(0U, pipe_count(&pipe));
    TEST_EQ_U32(6U, pipe_space(&pipe));
}

static void test_pipe_partial_write_and_read_return_transferred_bytes(void)
{
    pipe_t pipe;
    pipe_storage(storage, 4);
    const uint8_t in[] = { 9U, 8U, 7U, 6U, 5U };
    const uint8_t expected[] = { 9U, 8U };
    uint8_t out[2] = { 0U };

    pipe_init(&pipe, storage, sizeof(storage));

    TEST_EQ_U32(4U, pipe_write(&pipe, in, sizeof(in), OS_NO_WAIT));
    TEST_EQ_U32(4U, pipe_count(&pipe));
    TEST_EQ_U32(2U, pipe_read(&pipe, out, sizeof(out), OS_NO_WAIT));
    test_bytes_eq(expected, out, sizeof(expected));
    TEST_EQ_U32(2U, pipe_count(&pipe));
    TEST_EQ_U32(2U, pipe_space(&pipe));
}

static void test_pipe_empty_and_full_no_wait_return_zero(void)
{
    pipe_t pipe;
    pipe_storage(storage, 2);
    const uint8_t in[] = { 1U, 2U, 3U };
    uint8_t out[1] = { 0U };

    pipe_init(&pipe, storage, sizeof(storage));

    TEST_EQ_U32(0U, pipe_read(&pipe, out, sizeof(out), OS_NO_WAIT));
    TEST_EQ_U32(2U, pipe_write(&pipe, in, 2U, OS_NO_WAIT));
    TEST_EQ_U32(0U, pipe_space(&pipe));
    TEST_EQ_U32(0U, pipe_write(&pipe, &in[2], 1U, OS_NO_WAIT));
}

static void test_pipe_write_wakes_manual_blocked_reader(void)
{
    pipe_t pipe;
    pipe_storage(storage, 4);
    task_t reader;
    const uint8_t in[] = { 11U };

    kernel_host_reset();
    pipe_init(&pipe, storage, sizeof(storage));
    test_task_prepare_blocked(&reader, TASK_WAIT_PIPE_READ, &pipe);
    pipe.reader_waiter = &reader;

    TEST_EQ_U32(1U, pipe_write(&pipe, in, sizeof(in), OS_NO_WAIT));

    TEST_EQ_PTR(NULL, pipe.reader_waiter);
    TEST_EQ_U32(TASK_STATE_READY, reader.state);
    TEST_EQ_U32(TASK_WAIT_NONE, reader.wait_type);
    TEST_EQ_PTR(NULL, reader.wait_object);
    TEST_ASSERT(reader.pipe_wait_result);
    TEST_EQ_U32(1U, kernel_host_ready_count());
}

static void test_pipe_read_wakes_manual_blocked_writer(void)
{
    pipe_t pipe;
    pipe_storage(storage, 2);
    task_t writer;
    const uint8_t in[] = { 21U, 22U };
    uint8_t out[1] = { 0U };

    kernel_host_reset();
    pipe_init(&pipe, storage, sizeof(storage));
    TEST_EQ_U32(2U, pipe_write(&pipe, in, sizeof(in), OS_NO_WAIT));
    test_task_prepare_blocked(&writer, TASK_WAIT_PIPE_WRITE, &pipe);
    pipe.writer_waiter = &writer;

    TEST_EQ_U32(1U, pipe_read(&pipe, out, sizeof(out), OS_NO_WAIT));

    TEST_EQ_U32(21U, out[0]);
    TEST_EQ_PTR(NULL, pipe.writer_waiter);
    TEST_EQ_U32(TASK_STATE_READY, writer.state);
    TEST_EQ_U32(TASK_WAIT_NONE, writer.wait_type);
    TEST_EQ_PTR(NULL, writer.wait_object);
    TEST_ASSERT(writer.pipe_wait_result);
    TEST_EQ_U32(1U, kernel_host_ready_count());
}

static void test_pipe_reset_aborts_reader_and_writer_waiters(void)
{
    pipe_t pipe;
    pipe_storage(storage, 3);
    task_t reader;
    task_t writer;
    const uint8_t in[] = { 31U, 32U };

    kernel_host_reset();
    pipe_init(&pipe, storage, sizeof(storage));
    TEST_EQ_U32(2U, pipe_write(&pipe, in, sizeof(in), OS_NO_WAIT));
    test_task_prepare_blocked(&reader, TASK_WAIT_PIPE_READ, &pipe);
    test_task_prepare_blocked(&writer, TASK_WAIT_PIPE_WRITE, &pipe);
    reader.pipe_wait_result = true;
    writer.pipe_wait_result = true;
    pipe.reader_waiter = &reader;
    pipe.writer_waiter = &writer;

    pipe_reset(&pipe);

    TEST_EQ_U32(0U, pipe_count(&pipe));
    TEST_EQ_U32(3U, pipe_space(&pipe));
    TEST_EQ_PTR(NULL, pipe.reader_waiter);
    TEST_EQ_PTR(NULL, pipe.writer_waiter);
    TEST_EQ_U32(TASK_STATE_READY, reader.state);
    TEST_EQ_U32(TASK_STATE_READY, writer.state);
    TEST_EQ_U32(TASK_WAIT_NONE, reader.wait_type);
    TEST_EQ_U32(TASK_WAIT_NONE, writer.wait_type);
    TEST_EQ_PTR(NULL, reader.wait_object);
    TEST_EQ_PTR(NULL, writer.wait_object);
    TEST_ASSERT(!reader.pipe_wait_result);
    TEST_ASSERT(!writer.pipe_wait_result);
    TEST_EQ_U32(2U, kernel_host_ready_count());
}

static void test_msgq_blocking_send_and_recv_paths_link_current_task(void)
{
    msgq_t q;
    msgq_storage(storage, test_msg_t, 1);
    task_t current;
    test_msg_t msg = { 5U, 50U };
    test_msg_t out = { 0U, 0U };

    kernel_host_reset();
    msgq_init(&q, storage, sizeof(test_msg_t), 1U);
    TEST_ASSERT(msgq_send(&q, &msg, OS_NO_WAIT));

    test_task_prepare_running(&current);
    kernel_host_set_current(&current);
    TEST_ASSERT(!msgq_send(&q, &msg, OS_WAIT_FOREVER));
    TEST_EQ_U32(TASK_STATE_BLOCKED, current.state);
    TEST_EQ_U32(TASK_WAIT_MSGQ_SEND, current.wait_type);
    TEST_EQ_PTR(&q, current.wait_object);
    TEST_EQ_SIZE(1U, dlist_count(&q.send_wait_list));
    TEST_EQ_U32(1U, kernel_host_block_count());

    dlist_remove(&current.sched_node);
    current.wait_type = TASK_WAIT_NONE;
    current.wait_object = NULL;
    current.msgq_wait_result = false;
    current.state = TASK_STATE_RUNNING;
    TEST_ASSERT(msgq_recv(&q, &out, OS_NO_WAIT));
    TEST_ASSERT(!msgq_recv(&q, &out, OS_WAIT_FOREVER));
    TEST_EQ_U32(TASK_STATE_BLOCKED, current.state);
    TEST_EQ_U32(TASK_WAIT_MSGQ_RECV, current.wait_type);
    TEST_EQ_PTR(&q, current.wait_object);
    TEST_EQ_SIZE(1U, dlist_count(&q.recv_wait_list));
}

static void test_pipe_blocking_write_and_read_paths_set_current_waiter(void)
{
    pipe_t pipe;
    pipe_storage(storage, 1);
    task_t current;
    const uint8_t in[] = { 6U };
    uint8_t out[1] = { 0U };

    kernel_host_reset();
    pipe_init(&pipe, storage, sizeof(storage));
    TEST_EQ_U32(1U, pipe_write(&pipe, in, sizeof(in), OS_NO_WAIT));

    test_task_prepare_running(&current);
    kernel_host_set_current(&current);
    TEST_EQ_U32(0U, pipe_write(&pipe, in, sizeof(in), OS_WAIT_FOREVER));
    TEST_EQ_PTR(&current, pipe.writer_waiter);
    TEST_EQ_U32(TASK_STATE_BLOCKED, current.state);
    TEST_EQ_U32(TASK_WAIT_PIPE_WRITE, current.wait_type);
    TEST_EQ_PTR(&pipe, current.wait_object);
    TEST_EQ_U32(1U, kernel_host_block_count());

    pipe.writer_waiter = NULL;
    current.wait_type = TASK_WAIT_NONE;
    current.wait_object = NULL;
    current.pipe_wait_result = false;
    current.state = TASK_STATE_RUNNING;
    TEST_EQ_U32(1U, pipe_read(&pipe, out, sizeof(out), OS_NO_WAIT));
    TEST_EQ_U32(0U, pipe_read(&pipe, out, sizeof(out), OS_WAIT_FOREVER));
    TEST_EQ_PTR(&current, pipe.reader_waiter);
    TEST_EQ_U32(TASK_STATE_BLOCKED, current.state);
    TEST_EQ_U32(TASK_WAIT_PIPE_READ, current.wait_type);
    TEST_EQ_PTR(&pipe, current.wait_object);
}

void test_msgq_pipe_run(void)
{
    TEST_RUN(test_msgq_init_sets_empty_count_and_space);
    TEST_RUN(test_msgq_send_recv_preserves_fifo_order);
    TEST_RUN(test_msgq_full_and_empty_no_wait_return_false);
    TEST_RUN(test_msgq_send_wakes_manual_blocked_receiver);
    TEST_RUN(test_msgq_recv_wakes_manual_blocked_sender);
    TEST_RUN(test_msgq_blocking_send_and_recv_paths_link_current_task);

    TEST_RUN(test_pipe_init_sets_empty_count_and_space);
    TEST_RUN(test_pipe_write_read_preserves_fifo_order);
    TEST_RUN(test_pipe_partial_write_and_read_return_transferred_bytes);
    TEST_RUN(test_pipe_empty_and_full_no_wait_return_zero);
    TEST_RUN(test_pipe_write_wakes_manual_blocked_reader);
    TEST_RUN(test_pipe_read_wakes_manual_blocked_writer);
    TEST_RUN(test_pipe_reset_aborts_reader_and_writer_waiters);
    TEST_RUN(test_pipe_blocking_write_and_read_paths_set_current_waiter);
}
