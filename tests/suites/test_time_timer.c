#include "tests/test.h"

#include "kernel/scheduler_internal.h"
#include "kernel/task.h"
#include "kernel/time.h"
#include "kernel/timer.h"
#include "tests/host/kernel_host_stub.h"

#if !OS_TIMER_ENABLE
#error "test_time_timer.c requires OS_TIMER_ENABLE=1"
#endif

static void test_task_entry(void *arg)
{
    (void)arg;
}

static void test_reset_kernel_time(void)
{
    kernel_host_reset();
    kernel_time_init();
}

static void test_init_task(task_t *task, uint8_t *stack, size_t stack_size)
{
    task_init(task, "test", test_task_entry, NULL, stack, stack_size, 1U);
}

static void test_prepare_delayed_task(task_t *task)
{
    task->state = TASK_STATE_BLOCKED;
    task->wait_type = TASK_WAIT_DELAY;
}

static void test_advance_ticks(os_tick_t ticks)
{
    os_tick_t i;

    for (i = 0U; i < ticks; i++) {
        os_tick_advance();
    }
}

static void test_tick_get_and_advance_increments_count(void)
{
    test_reset_kernel_time();

    TEST_EQ_U32(0U, os_tick_get());

    os_tick_advance();
    TEST_EQ_U32(1U, os_tick_get());

    os_tick_advance();
    os_tick_advance();
    TEST_EQ_U32(3U, os_tick_get());
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_tick_after_eq_handles_equal_before_after_and_wrap(void)
{
    TEST_ASSERT(os_tick_after_eq(10U, 10U));
    TEST_ASSERT(os_tick_after_eq(12U, 10U));
    TEST_ASSERT(!os_tick_after_eq(8U, 10U));

    TEST_ASSERT(os_tick_after_eq(1U, UINT32_MAX - 1U));
    TEST_ASSERT(!os_tick_after_eq(UINT32_MAX - 1U, 1U));
}

static void test_tick_elapsed_handles_duration_and_wrap(void)
{
    TEST_ASSERT(os_tick_elapsed(15U, 10U, 5U));
    TEST_ASSERT(os_tick_elapsed(16U, 10U, 5U));
    TEST_ASSERT(!os_tick_elapsed(14U, 10U, 5U));

    TEST_ASSERT(os_tick_elapsed(1U, UINT32_MAX - 1U, 3U));
    TEST_ASSERT(!os_tick_elapsed(1U, UINT32_MAX - 1U, 4U));
}

static void test_timeout_start_wakes_delay_task_at_deadline(void)
{
    task_t task;
    task_stack(stack, 128);

    test_reset_kernel_time();
    test_init_task(&task, stack, sizeof(stack));
    test_prepare_delayed_task(&task);

    kernel_timeout_start_locked(&task, 3U);

    test_advance_ticks(2U);
    TEST_EQ_U32(TASK_STATE_BLOCKED, task.state);
    TEST_EQ_U32(TASK_WAIT_DELAY, task.wait_type);
    TEST_EQ_U32(0U, kernel_host_ready_count());

    os_tick_advance();
    TEST_EQ_U32(TASK_STATE_READY, task.state);
    TEST_EQ_U32(TASK_WAIT_NONE, task.wait_type);
    TEST_EQ_U32(1U, kernel_host_ready_count());
}

static void test_timeout_start_wakes_multiple_delay_tasks_on_same_tick(void)
{
    task_t first;
    task_t second;
    task_stack(first_stack, 128);
    task_stack(second_stack, 128);

    test_reset_kernel_time();
    test_init_task(&first, first_stack, sizeof(first_stack));
    test_init_task(&second, second_stack, sizeof(second_stack));
    test_prepare_delayed_task(&first);
    test_prepare_delayed_task(&second);

    kernel_timeout_start_locked(&first, 4U);
    kernel_timeout_start_locked(&second, 4U);

    test_advance_ticks(3U);
    TEST_EQ_U32(TASK_STATE_BLOCKED, first.state);
    TEST_EQ_U32(TASK_STATE_BLOCKED, second.state);
    TEST_EQ_U32(0U, kernel_host_ready_count());

    os_tick_advance();
    TEST_EQ_U32(TASK_STATE_READY, first.state);
    TEST_EQ_U32(TASK_STATE_READY, second.state);
    TEST_EQ_U32(TASK_WAIT_NONE, first.wait_type);
    TEST_EQ_U32(TASK_WAIT_NONE, second.wait_type);
    TEST_EQ_U32(2U, kernel_host_ready_count());
}

static void test_timer_callback_count(void *arg)
{
    uint32_t *count = (uint32_t *)arg;

    (*count)++;
}

static void test_soft_timer_one_shot_runs_once_and_stops(void)
{
    soft_timer_t timer;
    uint32_t count = 0U;

    test_reset_kernel_time();
    timer_init(&timer, test_timer_callback_count, &count);
    timer_start(&timer, 2U, 0U);

    os_tick_advance();
    TEST_EQ_U32(0U, count);
    TEST_ASSERT(timer_is_running(&timer));

    os_tick_advance();
    TEST_EQ_U32(1U, count);
    TEST_ASSERT(!timer_is_running(&timer));

    test_advance_ticks(3U);
    TEST_EQ_U32(1U, count);
}

static void test_soft_timer_periodic_runs_at_period_until_stopped(void)
{
    soft_timer_t timer;
    uint32_t count = 0U;

    test_reset_kernel_time();
    timer_init(&timer, test_timer_callback_count, &count);
    timer_start(&timer, 2U, 3U);

    test_advance_ticks(2U);
    TEST_EQ_U32(1U, count);
    TEST_ASSERT(timer_is_running(&timer));

    test_advance_ticks(2U);
    TEST_EQ_U32(1U, count);

    os_tick_advance();
    TEST_EQ_U32(2U, count);
    TEST_ASSERT(timer_is_running(&timer));

    timer_stop(&timer);
    test_advance_ticks(6U);
    TEST_EQ_U32(2U, count);
    TEST_ASSERT(!timer_is_running(&timer));
}

static void test_soft_timer_restart_moves_deadline_from_current_tick(void)
{
    soft_timer_t timer;
    uint32_t count = 0U;

    test_reset_kernel_time();
    timer_init(&timer, test_timer_callback_count, &count);
    timer_start(&timer, 5U, 0U);

    test_advance_ticks(2U);
    timer_start(&timer, 4U, 0U);

    test_advance_ticks(3U);
    TEST_EQ_U32(0U, count);
    TEST_ASSERT(timer_is_running(&timer));

    os_tick_advance();
    TEST_EQ_U32(1U, count);
    TEST_ASSERT(!timer_is_running(&timer));
}

static void test_soft_timer_stop_prevents_pending_callback(void)
{
    soft_timer_t timer;
    uint32_t count = 0U;

    test_reset_kernel_time();
    timer_init(&timer, test_timer_callback_count, &count);
    timer_start(&timer, 2U, 0U);
    timer_stop(&timer);

    TEST_ASSERT(!timer_is_running(&timer));

    test_advance_ticks(3U);
    TEST_EQ_U32(0U, count);

    timer_stop(&timer);
    TEST_ASSERT(!timer_is_running(&timer));
}

void test_time_timer_run(void)
{
    TEST_RUN(test_tick_get_and_advance_increments_count);
    TEST_RUN(test_tick_after_eq_handles_equal_before_after_and_wrap);
    TEST_RUN(test_tick_elapsed_handles_duration_and_wrap);
    TEST_RUN(test_timeout_start_wakes_delay_task_at_deadline);
    TEST_RUN(test_timeout_start_wakes_multiple_delay_tasks_on_same_tick);
    TEST_RUN(test_soft_timer_one_shot_runs_once_and_stops);
    TEST_RUN(test_soft_timer_periodic_runs_at_period_until_stopped);
    TEST_RUN(test_soft_timer_restart_moves_deadline_from_current_tick);
    TEST_RUN(test_soft_timer_stop_prevents_pending_callback);
}
