#include "tests/test.h"

#include "tests/host/kernel_host_stub.h"
#include "kernel/eventset.h"
#include "kernel/sem.h"
#include "kernel/task.h"

static void test_task_entry_(void *arg)
{
    (void)arg;
}

static void test_task_make_(task_t *task,
                            const char *name,
                            uint8_t *stack,
                            size_t stack_size,
                            task_priority_t priority)
{
    task_init(task, name, test_task_entry_, task, stack, stack_size, priority);
}

static void test_prepare_notify_waiter_(task_t *task)
{
    task->state = TASK_STATE_BLOCKED;
    task->wait_type = TASK_WAIT_NOTIFY;
    task->notify_pending = false;
    task->notify_result = false;
}

static void test_prepare_eventset_waiter_(eventset_t *eventset,
                                          task_t *task,
                                          eventset_bits_t mask,
                                          bool wait_all)
{
    task->state = TASK_STATE_BLOCKED;
    task->wait_type = TASK_WAIT_EVENTSET;
    task->wait_object = eventset;
    task->eventset_wait_mask = mask;
    task->eventset_wait_result = 0U;
    task->eventset_wait_all = wait_all;
    dlist_push_back(&eventset->wait_list, &task->sched_node);
}

static void test_prepare_sem_waiter_(sem_t *sem, task_t *task)
{
    task->state = TASK_STATE_BLOCKED;
    task->wait_type = TASK_WAIT_SEM;
    task->wait_object = sem;
    task->sem_take_result = false;
    dlist_push_back(&sem->wait_list, &task->sched_node);
}

static void test_task_init_and_getters_report_initial_state(void)
{
    task_t task;
    uint8_t stack[64];
    int arg = 7;

    kernel_host_reset();
    task_init(&task, "worker", test_task_entry_, &arg, stack, sizeof(stack), 3U);

    TEST_EQ_PTR("worker", task_get_name(&task));
    TEST_EQ_U32(3U, task_get_priority(&task));
    TEST_EQ_U32(TASK_STATE_CREATED, task_get_state(&task));
    TEST_EQ_U32(TASK_WAIT_NONE, task_get_wait_type(&task));
    TEST_EQ_PTR(stack, task.stack);
    TEST_EQ_SIZE(sizeof(stack), task.stack_size);
    TEST_EQ_PTR(test_task_entry_, task.entry);
    TEST_EQ_PTR(&arg, task.arg);
    TEST_EQ_PTR(stack + sizeof(stack), task.sp);
    TEST_ASSERT(!task.notify_pending);
    TEST_ASSERT(!task.notify_result);
    TEST_ASSERT(dlist_node_is_detached(&task.sched_node));
    TEST_ASSERT(dlist_node_is_detached(&task.timeout_node));
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_task_current_uses_host_current_task(void)
{
    task_t task;
    uint8_t stack[64];

    kernel_host_reset();
    test_task_make_(&task, "current", stack, sizeof(stack), 2U);

    TEST_EQ_PTR(NULL, task_current());
    kernel_host_set_current(&task);
    TEST_EQ_PTR(&task, task_current());
}

static void test_task_notify_wait_consumes_pending_without_blocking(void)
{
    task_t task;
    uint8_t stack[64];

    kernel_host_reset();
    test_task_make_(&task, "notified", stack, sizeof(stack), 1U);
    task.state = TASK_STATE_RUNNING;
    kernel_host_set_current(&task);

    task_notify(&task);

    TEST_ASSERT(task_notify_wait(OS_NO_WAIT));
    TEST_ASSERT(!task.notify_pending);
    TEST_EQ_U32(TASK_WAIT_NONE, task.wait_type);
    TEST_EQ_U32(TASK_STATE_RUNNING, task.state);
    TEST_EQ_U32(0U, kernel_host_block_count());
    TEST_EQ_U32(0U, kernel_host_ready_count());
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_task_notify_wait_no_wait_reports_absent_notification(void)
{
    task_t task;
    uint8_t stack[64];

    kernel_host_reset();
    test_task_make_(&task, "empty", stack, sizeof(stack), 1U);
    task.state = TASK_STATE_RUNNING;
    kernel_host_set_current(&task);

    TEST_ASSERT(!task_notify_wait(OS_NO_WAIT));
    TEST_ASSERT(!task.notify_pending);
    TEST_EQ_U32(TASK_WAIT_NONE, task.wait_type);
    TEST_EQ_U32(TASK_STATE_RUNNING, task.state);
    TEST_EQ_U32(0U, kernel_host_block_count());
    TEST_EQ_U32(0U, kernel_host_ready_count());
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_task_notify_wakes_blocked_notify_waiter(void)
{
    task_t task;
    uint8_t stack[64];

    kernel_host_reset();
    test_task_make_(&task, "waiter", stack, sizeof(stack), 1U);
    test_prepare_notify_waiter_(&task);

    task_notify(&task);

    TEST_EQ_U32(TASK_STATE_READY, task.state);
    TEST_EQ_U32(TASK_WAIT_NONE, task.wait_type);
    TEST_ASSERT(!task.notify_pending);
    TEST_ASSERT(task.notify_result);
    TEST_EQ_U32(1U, kernel_host_ready_count());
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_eventset_init_set_clear_and_get_track_bits(void)
{
    eventset_t eventset;

    kernel_host_reset();
    eventset_init(&eventset);

    TEST_EQ_U32(0U, eventset_get(&eventset));
    TEST_ASSERT(dlist_empty(&eventset.wait_list));

    eventset_set(&eventset, 0x05U);
    TEST_EQ_U32(0x05U, eventset_get(&eventset));

    eventset_set(&eventset, 0x02U);
    TEST_EQ_U32(0x07U, eventset_get(&eventset));

    eventset_clear(&eventset, 0x03U);
    TEST_EQ_U32(0x04U, eventset_get(&eventset));
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_eventset_wait_any_no_wait_returns_matching_bits(void)
{
    eventset_t eventset;

    kernel_host_reset();
    eventset_init(&eventset);
    eventset_set(&eventset, 0x05U);

    TEST_EQ_U32(0x04U, eventset_wait_any(&eventset, 0x06U, OS_NO_WAIT));
    TEST_EQ_U32(0U, eventset_wait_any(&eventset, 0x08U, OS_NO_WAIT));
    TEST_EQ_U32(0x05U, eventset_get(&eventset));
    TEST_EQ_U32(0U, kernel_host_block_count());
    TEST_ASSERT(dlist_empty(&eventset.wait_list));
}

static void test_eventset_wait_all_no_wait_requires_full_mask(void)
{
    eventset_t eventset;

    kernel_host_reset();
    eventset_init(&eventset);
    eventset_set(&eventset, 0x05U);

    TEST_EQ_U32(0x05U, eventset_wait_all(&eventset, 0x05U, OS_NO_WAIT));
    TEST_EQ_U32(0U, eventset_wait_all(&eventset, 0x07U, OS_NO_WAIT));
    TEST_EQ_U32(0x05U, eventset_get(&eventset));
    TEST_EQ_U32(0U, kernel_host_block_count());
    TEST_ASSERT(dlist_empty(&eventset.wait_list));
}

static void test_eventset_set_wakes_all_matching_blocked_waiters(void)
{
    eventset_t eventset;
    task_t any_bit;
    task_t all_bits;
    task_t unmatched;
    uint8_t any_stack[64];
    uint8_t all_stack[64];
    uint8_t unmatched_stack[64];

    kernel_host_reset();
    eventset_init(&eventset);
    test_task_make_(&any_bit, "any", any_stack, sizeof(any_stack), 1U);
    test_task_make_(&all_bits, "all", all_stack, sizeof(all_stack), 1U);
    test_task_make_(&unmatched, "unmatched", unmatched_stack, sizeof(unmatched_stack), 1U);

    test_prepare_eventset_waiter_(&eventset, &any_bit, 0x01U, false);
    test_prepare_eventset_waiter_(&eventset, &all_bits, 0x03U, true);
    test_prepare_eventset_waiter_(&eventset, &unmatched, 0x08U, false);

    eventset_set(&eventset, 0x03U);

    TEST_EQ_U32(0x03U, eventset_get(&eventset));
    TEST_EQ_U32(2U, kernel_host_ready_count());
    TEST_EQ_SIZE(1U, dlist_count(&eventset.wait_list));
    TEST_EQ_PTR(&unmatched.sched_node, dlist_peek_front(&eventset.wait_list));

    TEST_EQ_U32(TASK_STATE_READY, any_bit.state);
    TEST_EQ_U32(TASK_WAIT_NONE, any_bit.wait_type);
    TEST_EQ_PTR(NULL, any_bit.wait_object);
    TEST_EQ_U32(0U, any_bit.eventset_wait_mask);
    TEST_EQ_U32(0x01U, any_bit.eventset_wait_result);
    TEST_ASSERT(!any_bit.eventset_wait_all);
    TEST_ASSERT(dlist_node_is_detached(&any_bit.sched_node));

    TEST_EQ_U32(TASK_STATE_READY, all_bits.state);
    TEST_EQ_U32(TASK_WAIT_NONE, all_bits.wait_type);
    TEST_EQ_PTR(NULL, all_bits.wait_object);
    TEST_EQ_U32(0U, all_bits.eventset_wait_mask);
    TEST_EQ_U32(0x03U, all_bits.eventset_wait_result);
    TEST_ASSERT(!all_bits.eventset_wait_all);
    TEST_ASSERT(dlist_node_is_detached(&all_bits.sched_node));

    TEST_EQ_U32(TASK_STATE_BLOCKED, unmatched.state);
    TEST_EQ_U32(TASK_WAIT_EVENTSET, unmatched.wait_type);
    TEST_EQ_PTR(&eventset, unmatched.wait_object);
    TEST_EQ_U32(0x08U, unmatched.eventset_wait_mask);
    TEST_EQ_U32(0U, unmatched.eventset_wait_result);
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_sem_init_sets_count_limit_and_empty_wait_list(void)
{
    sem_t sem;

    kernel_host_reset();
    sem_init(&sem, 2U, 4U);

    TEST_EQ_U32(2U, sem.count);
    TEST_EQ_U32(4U, sem.limit);
    TEST_ASSERT(dlist_empty(&sem.wait_list));
}

static void test_sem_take_no_wait_consumes_count_or_reports_empty(void)
{
    sem_t sem;

    kernel_host_reset();
    sem_init(&sem, 1U, 3U);

    TEST_ASSERT(sem_take(&sem, OS_NO_WAIT));
    TEST_EQ_U32(0U, sem.count);
    TEST_ASSERT(!sem_take(&sem, OS_NO_WAIT));
    TEST_EQ_U32(0U, sem.count);
    TEST_EQ_U32(0U, kernel_host_block_count());
    TEST_ASSERT(dlist_empty(&sem.wait_list));
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_sem_give_increments_count_up_to_limit(void)
{
    sem_t sem;

    kernel_host_reset();
    sem_init(&sem, 1U, 2U);

    sem_give(&sem);
    TEST_EQ_U32(2U, sem.count);

    sem_give(&sem);
    TEST_EQ_U32(2U, sem.count);
    TEST_EQ_U32(0U, kernel_host_ready_count());
    TEST_ASSERT(dlist_empty(&sem.wait_list));
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_sem_give_wakes_blocked_waiters_in_fifo_order(void)
{
    sem_t sem;
    task_t first;
    task_t second;
    task_t third;
    uint8_t first_stack[64];
    uint8_t second_stack[64];
    uint8_t third_stack[64];

    kernel_host_reset();
    sem_init(&sem, 0U, 3U);
    test_task_make_(&first, "first", first_stack, sizeof(first_stack), 1U);
    test_task_make_(&second, "second", second_stack, sizeof(second_stack), 1U);
    test_task_make_(&third, "third", third_stack, sizeof(third_stack), 1U);

    test_prepare_sem_waiter_(&sem, &first);
    test_prepare_sem_waiter_(&sem, &second);
    test_prepare_sem_waiter_(&sem, &third);

    sem_give(&sem);

    TEST_EQ_U32(1U, kernel_host_ready_count());
    TEST_EQ_U32(0U, sem.count);
    TEST_EQ_U32(TASK_STATE_READY, first.state);
    TEST_EQ_U32(TASK_WAIT_NONE, first.wait_type);
    TEST_EQ_PTR(NULL, first.wait_object);
    TEST_ASSERT(first.sem_take_result);
    TEST_ASSERT(dlist_node_is_detached(&first.sched_node));
    TEST_EQ_PTR(&second.sched_node, dlist_peek_front(&sem.wait_list));

    sem_give(&sem);

    TEST_EQ_U32(2U, kernel_host_ready_count());
    TEST_EQ_U32(0U, sem.count);
    TEST_EQ_U32(TASK_STATE_READY, second.state);
    TEST_EQ_U32(TASK_WAIT_NONE, second.wait_type);
    TEST_EQ_PTR(NULL, second.wait_object);
    TEST_ASSERT(second.sem_take_result);
    TEST_ASSERT(dlist_node_is_detached(&second.sched_node));
    TEST_EQ_PTR(&third.sched_node, dlist_peek_front(&sem.wait_list));

    sem_give(&sem);

    TEST_EQ_U32(3U, kernel_host_ready_count());
    TEST_EQ_U32(0U, sem.count);
    TEST_EQ_U32(TASK_STATE_READY, third.state);
    TEST_EQ_U32(TASK_WAIT_NONE, third.wait_type);
    TEST_EQ_PTR(NULL, third.wait_object);
    TEST_ASSERT(third.sem_take_result);
    TEST_ASSERT(dlist_node_is_detached(&third.sched_node));
    TEST_ASSERT(dlist_empty(&sem.wait_list));

    sem_give(&sem);

    TEST_EQ_U32(1U, sem.count);
    TEST_EQ_U32(3U, kernel_host_ready_count());
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

void test_task_event_sem_run(void)
{
    TEST_RUN(test_task_init_and_getters_report_initial_state);
    TEST_RUN(test_task_current_uses_host_current_task);
    TEST_RUN(test_task_notify_wait_consumes_pending_without_blocking);
    TEST_RUN(test_task_notify_wait_no_wait_reports_absent_notification);
    TEST_RUN(test_task_notify_wakes_blocked_notify_waiter);
    TEST_RUN(test_eventset_init_set_clear_and_get_track_bits);
    TEST_RUN(test_eventset_wait_any_no_wait_returns_matching_bits);
    TEST_RUN(test_eventset_wait_all_no_wait_requires_full_mask);
    TEST_RUN(test_eventset_set_wakes_all_matching_blocked_waiters);
    TEST_RUN(test_sem_init_sets_count_limit_and_empty_wait_list);
    TEST_RUN(test_sem_take_no_wait_consumes_count_or_reports_empty);
    TEST_RUN(test_sem_give_increments_count_up_to_limit);
    TEST_RUN(test_sem_give_wakes_blocked_waiters_in_fifo_order);
}
