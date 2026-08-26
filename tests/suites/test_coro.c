#include "coroutine/coro.h"
#include "tests/test.h"

typedef struct test_counter_coro {
    coro_t coro;
    uint32_t value;
} test_counter_coro_t;

typedef struct test_wait_coro {
    coro_t coro;
    bool ready;
    uint32_t step;
} test_wait_coro_t;

static coro_status_t counter_step(test_counter_coro_t *self)
{
    coro_begin(&self->coro);

    self->value = 1U;
    coro_yield(&self->coro);

    self->value = 2U;
    coro_yield(&self->coro);

    self->value = 3U;

    coro_end(&self->coro);
}

static coro_status_t wait_until_step(test_wait_coro_t *self)
{
    coro_begin(&self->coro);

    self->step = 1U;
    coro_wait_until(&self->coro, self->ready);

    self->step = 2U;

    coro_end(&self->coro);
}

static coro_status_t wait_while_step(test_wait_coro_t *self)
{
    coro_begin(&self->coro);

    self->step = 1U;
    coro_wait_while(&self->coro, !self->ready);

    self->step = 2U;

    coro_end(&self->coro);
}

static coro_status_t exit_step(test_counter_coro_t *self)
{
    coro_begin(&self->coro);

    self->value = 1U;
    coro_exit(&self->coro);

    self->value = 2U;

    coro_end(&self->coro);
}

static coro_status_t restart_step(test_counter_coro_t *self)
{
    coro_begin(&self->coro);

    self->value++;
    coro_restart(&self->coro);

    self->value += 100U;

    coro_end(&self->coro);
}

static void test_coro_init_and_static_init_start_ready(void)
{
    coro_t dynamic;
    coro_t static_coro = coro_static_init();

    dynamic.state = 123U;
    coro_init(&dynamic);

    TEST_ASSERT(!coro_is_done(&dynamic));
    TEST_ASSERT(!coro_is_done(&static_coro));
    TEST_EQ_U32(0U, dynamic.state);
    TEST_EQ_U32(0U, static_coro.state);
}

static void test_coro_yield_resumes_from_next_statement(void)
{
    test_counter_coro_t self = { coro_static_init(), 0U };

    TEST_EQ_U32(CORO_WAITING, counter_step(&self));
    TEST_EQ_U32(1U, self.value);

    TEST_EQ_U32(CORO_WAITING, counter_step(&self));
    TEST_EQ_U32(2U, self.value);

    TEST_EQ_U32(CORO_DONE, counter_step(&self));
    TEST_EQ_U32(3U, self.value);
    TEST_ASSERT(coro_is_done(&self.coro));
}

static void test_coro_done_stays_done_until_reset(void)
{
    test_counter_coro_t self = { coro_static_init(), 0U };

    TEST_EQ_U32(CORO_WAITING, counter_step(&self));
    TEST_EQ_U32(CORO_WAITING, counter_step(&self));
    TEST_EQ_U32(CORO_DONE, counter_step(&self));

    self.value = 99U;
    TEST_EQ_U32(CORO_DONE, counter_step(&self));
    TEST_EQ_U32(99U, self.value);

    coro_reset(&self.coro);
    TEST_EQ_U32(CORO_WAITING, counter_step(&self));
    TEST_EQ_U32(1U, self.value);
}

static void test_coro_wait_until_blocks_until_condition_is_true(void)
{
    test_wait_coro_t self = { coro_static_init(), false, 0U };

    TEST_EQ_U32(CORO_WAITING, wait_until_step(&self));
    TEST_EQ_U32(1U, self.step);

    TEST_EQ_U32(CORO_WAITING, wait_until_step(&self));
    TEST_EQ_U32(1U, self.step);

    self.ready = true;
    TEST_EQ_U32(CORO_DONE, wait_until_step(&self));
    TEST_EQ_U32(2U, self.step);
}

static void test_coro_wait_while_blocks_while_condition_is_true(void)
{
    test_wait_coro_t self = { coro_static_init(), false, 0U };

    TEST_EQ_U32(CORO_WAITING, wait_while_step(&self));
    TEST_EQ_U32(1U, self.step);

    self.ready = true;
    TEST_EQ_U32(CORO_DONE, wait_while_step(&self));
    TEST_EQ_U32(2U, self.step);
}

static void test_coro_exit_marks_done_without_running_following_code(void)
{
    test_counter_coro_t self = { coro_static_init(), 0U };

    TEST_EQ_U32(CORO_DONE, exit_step(&self));
    TEST_EQ_U32(1U, self.value);
    TEST_ASSERT(coro_is_done(&self.coro));

    TEST_EQ_U32(CORO_DONE, exit_step(&self));
    TEST_EQ_U32(1U, self.value);
}

static void test_coro_restart_runs_from_beginning_on_next_call(void)
{
    test_counter_coro_t self = { coro_static_init(), 0U };

    TEST_EQ_U32(CORO_WAITING, restart_step(&self));
    TEST_EQ_U32(1U, self.value);

    TEST_EQ_U32(CORO_WAITING, restart_step(&self));
    TEST_EQ_U32(2U, self.value);
}

void test_coro_run(void)
{
    TEST_RUN(test_coro_init_and_static_init_start_ready);
    TEST_RUN(test_coro_yield_resumes_from_next_statement);
    TEST_RUN(test_coro_done_stays_done_until_reset);
    TEST_RUN(test_coro_wait_until_blocks_until_condition_is_true);
    TEST_RUN(test_coro_wait_while_blocks_while_condition_is_true);
    TEST_RUN(test_coro_exit_marks_done_without_running_following_code);
    TEST_RUN(test_coro_restart_runs_from_beginning_on_next_call);
}
