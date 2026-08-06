/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "kernel/kernel.h"
#include "kernel/trace.h"
#include "service/bus/bus.h"
#include <string.h>

#ifdef MCU_TEST_FPU_STRESS
#ifndef MCU_TEST_FPU_REGS
#define MCU_TEST_FPU_REGS 1
#endif
#endif

#if defined(MCU_TEST_LONG_STABILITY) || defined(MCU_TEST_SCHED_STRESS) || defined(MCU_TEST_FPU_STRESS)
#define MCU_TEST_LONG_OR_SCHED_STRESS 1
#endif

#if (defined(MCU_TEST_LONG_OR_SCHED_STRESS) || defined(MCU_TEST_BUS_STRESS)) && defined(__GNUC__)
/*
 * The stress firmware and the one-shot functional firmware are selected by
 * compile-time switches but live in the same CubeMX main.c user sections. In
 * stress builds, unrelated one-shot test entries are intentionally unused.
 */
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum worker_cmd {
  WORKER_CMD_NONE,
  WORKER_CMD_EVENT_ANY,
  WORKER_CMD_SEM_TAKE,
  WORKER_CMD_MSGQ_RECV,
  WORKER_CMD_MSGQ_SEND,
  WORKER_CMD_PIPE_READ,
  WORKER_CMD_PIPE_WRITE,
  WORKER_CMD_DELAY
} worker_cmd_t;

typedef struct bus_test_event {
  uint32_t id;
  uint32_t value;
} bus_test_event_t;

typedef struct bus_test_state {
  uint32_t value;
} bus_test_state_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define YIELD_TEST_STEPS 9U
#define DELAY_TEST_TICKS 30U
#define NOTIFY_TEST_TICK 100U
#define WORKER_COUNT 2U
#define FPU_WORKER_COUNT 2U
#define FPU_STRESS_LOOPS 20U
#define LONG_MSGQ_CAPACITY 8U
#define LONG_PIPE_SIZE 64U
#define LONG_EVENT_ISR_BIT 0x00000001UL
#define LONG_EVENT_TIMER_BIT 0x00000002UL
#define LONG_REPORT_TICKS 60000U
#ifdef MCU_TEST_FPU_STRESS
#define STRESS_LOG_PREFIX "[G070-FPU-STRESS]"
#define LONG_NOTIFY_PERIOD_TICKS 1U
#define LONG_EVENT_ISR_PERIOD_TICKS 2U
#define LONG_SEM_PERIOD_TICKS 3U
#define LONG_TIMER_PERIOD_TICKS 3U
#elif defined(MCU_TEST_SCHED_STRESS)
#define STRESS_LOG_PREFIX "[G070-SCHED]"
#define LONG_NOTIFY_PERIOD_TICKS 1U
#define LONG_EVENT_ISR_PERIOD_TICKS 2U
#define LONG_SEM_PERIOD_TICKS 3U
#define LONG_TIMER_PERIOD_TICKS 3U
#else
#define STRESS_LOG_PREFIX "[G070-LONG]"
#define LONG_NOTIFY_PERIOD_TICKS 5U
#define LONG_EVENT_ISR_PERIOD_TICKS 7U
#define LONG_SEM_PERIOD_TICKS 11U
#define LONG_TIMER_PERIOD_TICKS 17U
#endif
#define BUS_STRESS_LOG_PREFIX "[G070-BUS]"
#define BUS_STRESS_EVENT_CAPACITY 32U
#define BUS_STRESS_REPORT_TICKS 60000U
#ifdef MCU_TEST_BUS_STRESS
#define BUS_KEY_EVENT_CAPACITY BUS_STRESS_EVENT_CAPACITY
#else
#define BUS_KEY_EVENT_CAPACITY 2U
#endif

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
static task_t notify_task_;
static task_t delay_task_;
static task_t yield_task_a_;
static task_t yield_task_b_;
static task_t yield_task_c_;
static task_t primitive_task_;
static task_t worker_task_1_;
static task_t worker_task_2_;
static task_t peer_task_;
static task_t return_task_;
#ifdef MCU_TEST_FPU_REGS
static task_t fpu_task_1_;
static task_t fpu_task_2_;
static task_t fpu_preempt_low_task_;
static task_t fpu_preempt_high_task_;
static task_t fpu_isr_task_;
#endif
#ifdef MCU_TEST_LONG_OR_SCHED_STRESS
static task_t long_control_task_;
static task_t long_notify_task_;
static task_t long_event_task_;
static task_t long_sem_task_;
static task_t long_msgq_tx_task_;
static task_t long_msgq_rx_task_;
static task_t long_pipe_tx_task_;
static task_t long_pipe_rx_task_;
static task_t long_delay_task_;
static task_t long_yield_task_a_;
static task_t long_yield_task_b_;
static task_t long_yield_task_c_;
#if OS_TIMER_ENABLE
static task_t long_timer_task_;
#endif
#endif
#ifdef MCU_TEST_BUS_STRESS
static task_t bus_control_task_;
static task_t bus_pub_task_;
static task_t bus_sub_a_task_;
static task_t bus_sub_b_task_;
#endif

static task_stack(notify_stack_, 1536);
static task_stack(delay_stack_, 1536);
static task_stack(yield_stack_a_, 1536);
static task_stack(yield_stack_b_, 1536);
static task_stack(yield_stack_c_, 1536);
static task_stack(primitive_stack_, 2048);
static task_stack(worker_stack_1_, 1536);
static task_stack(worker_stack_2_, 1536);
static task_stack(peer_stack_, 1024);
static task_stack(return_stack_, 512);
#ifdef MCU_TEST_FPU_REGS
static task_stack(fpu_stack_1_, 2048);
static task_stack(fpu_stack_2_, 2048);
static task_stack(fpu_preempt_low_stack_, 2048);
static task_stack(fpu_preempt_high_stack_, 2048);
static task_stack(fpu_isr_stack_, 2048);
#endif
#ifdef MCU_TEST_LONG_OR_SCHED_STRESS
static task_stack(long_control_stack_, 2048);
static task_stack(long_notify_stack_, 1024);
static task_stack(long_event_stack_, 1024);
static task_stack(long_sem_stack_, 1024);
static task_stack(long_msgq_tx_stack_, 1024);
static task_stack(long_msgq_rx_stack_, 1024);
static task_stack(long_pipe_tx_stack_, 1024);
static task_stack(long_pipe_rx_stack_, 1024);
static task_stack(long_delay_stack_, 1024);
static task_stack(long_yield_stack_a_, 1024);
static task_stack(long_yield_stack_b_, 1024);
static task_stack(long_yield_stack_c_, 1024);
#if OS_TIMER_ENABLE
static task_stack(long_timer_stack_, 1024);
#endif
#endif
#ifdef MCU_TEST_FPU_STRESS
static task_t fpu_stress_task_1_;
static task_t fpu_stress_task_2_;
static task_stack(fpu_stress_stack_1_, 2048);
static task_stack(fpu_stress_stack_2_, 2048);
#endif
#ifdef MCU_TEST_BUS_STRESS
static task_stack(bus_control_stack_, 2048);
static task_stack(bus_pub_stack_, 1536);
static task_stack(bus_sub_a_stack_, 1536);
static task_stack(bus_sub_b_stack_, 1536);
#endif

static eventset_t test_eventset_;
static sem_t test_sem_;
static msgq_t test_msgq_;
static msgq_storage(test_msgq_storage_, uint32_t, 2);
static pipe_t test_pipe_;
static pipe_storage(test_pipe_storage_, 4);

BUS_SUBSCRIBER_DEFINE(bus_ui_subscriber_);
BUS_SUBSCRIBER_DEFINE(bus_log_subscriber_);
BUS_EVENT_CHANNEL_DEFINE(bus_key_event_channel_,
                         bus_test_event_t,
                         BUS_KEY_EVENT_CAPACITY);
BUS_STATE_CHANNEL_DEFINE(bus_ui_state_channel_, bus_test_state_t);
#ifdef MCU_TEST_BUS_STRESS
BUS_EVENT_CHANNEL_DEFINE(bus_sensor_event_channel_,
                         bus_test_event_t,
                         BUS_STRESS_EVENT_CAPACITY);
BUS_STATE_CHANNEL_DEFINE(bus_system_state_channel_, bus_test_state_t);
#endif

#ifdef MCU_TEST_BUS_STRESS
BUS_CHANNELS_REGISTER(
  BUS_CHANNEL(bus_key_event_channel_),
  BUS_CHANNEL(bus_sensor_event_channel_),
  BUS_CHANNEL(bus_ui_state_channel_),
  BUS_CHANNEL(bus_system_state_channel_)
);
#else
BUS_CHANNELS_REGISTER(
  BUS_CHANNEL(bus_key_event_channel_),
  BUS_CHANNEL(bus_ui_state_channel_)
);
#endif

BUS_SUBSCRIBERS_REGISTER(
  BUS_SUBSCRIBER(bus_ui_subscriber_),
  BUS_SUBSCRIBER(bus_log_subscriber_)
);

#ifdef MCU_TEST_BUS_STRESS
BUS_SUBSCRIPTIONS_REGISTER(
  BUS_SUBSCRIBE(bus_key_event_channel_, bus_ui_subscriber_),
  BUS_SUBSCRIBE(bus_sensor_event_channel_, bus_ui_subscriber_),
  BUS_SUBSCRIBE(bus_ui_state_channel_, bus_ui_subscriber_),
  BUS_SUBSCRIBE(bus_system_state_channel_, bus_ui_subscriber_),
  BUS_SUBSCRIBE(bus_key_event_channel_, bus_log_subscriber_),
  BUS_SUBSCRIBE(bus_sensor_event_channel_, bus_log_subscriber_),
  BUS_SUBSCRIBE(bus_ui_state_channel_, bus_log_subscriber_),
  BUS_SUBSCRIBE(bus_system_state_channel_, bus_log_subscriber_)
);
#else
BUS_SUBSCRIPTIONS_REGISTER(
  BUS_SUBSCRIBE(bus_key_event_channel_, bus_ui_subscriber_),
  BUS_SUBSCRIBE(bus_key_event_channel_, bus_log_subscriber_),
  BUS_SUBSCRIBE(bus_ui_state_channel_, bus_ui_subscriber_),
  BUS_SUBSCRIBE(bus_ui_state_channel_, bus_log_subscriber_)
);
#endif
#if OS_TIMER_ENABLE
static soft_timer_t one_shot_timer_;
static soft_timer_t periodic_timer_;
static soft_timer_t order_timer_a_;
static soft_timer_t order_timer_b_;
#endif

static volatile bool test_tick_enabled_;
static volatile bool test_finished_;
static volatile bool primitive_passed_;
static volatile bool notify_sent_;
static volatile bool notify_passed_;
static volatile bool delay_passed_;
static volatile bool yield_passed_;
static volatile bool idle_seen_;
static volatile char yield_expected_ = 'A';
static volatile uint32_t yield_steps_;
static volatile worker_cmd_t worker_cmd_[WORKER_COUNT];
static volatile bool worker_done_[WORKER_COUNT];
static volatile bool worker_result_[WORKER_COUNT];
static volatile uint32_t worker_value_[WORKER_COUNT];
static volatile uint32_t worker_order_[WORKER_COUNT];
static volatile uint32_t worker_order_count_;
#if OS_TIMER_ENABLE
static volatile uint32_t one_shot_count_;
static volatile uint32_t periodic_count_;
static volatile uint32_t timer_order_[2];
static volatile uint32_t timer_order_count_;
#endif
static volatile bool return_task_ran_;
static volatile uint32_t bus_publish_fail_count_;
static volatile bus_channel_t *bus_publish_fail_channel_;
#ifdef MCU_TEST_BUS_STRESS
static volatile uint32_t bus_stress_event_pub_[2];
static volatile uint32_t bus_stress_event_rx_[2][2];
static volatile uint32_t bus_stress_state_pub_[2];
static volatile uint32_t bus_stress_state_rx_[2][2];
static volatile uint32_t bus_stress_ready_count_[2];
static volatile uint32_t bus_stress_timeout_count_[2];
static volatile uint32_t bus_stress_yield_count_;
static volatile uint32_t bus_stress_error_count_;
#endif
static volatile bool peer_ran_;
static volatile bool eventset_isr_pending_;
static volatile os_tick_t eventset_isr_tick_;
static volatile eventset_t *eventset_isr_target_;
static volatile eventset_bits_t eventset_isr_bits_;
static volatile bool sem_isr_pending_;
static volatile os_tick_t sem_isr_tick_;
static volatile sem_t *sem_isr_target_;
#ifdef MCU_TEST_FPU_REGS
static volatile bool fpu_worker_done_[FPU_WORKER_COUNT];
static volatile bool fpu_preempt_low_done_;
static volatile bool fpu_preempt_high_done_;
static volatile bool fpu_isr_done_;
static volatile bool fpu_isr_pending_;
static volatile os_tick_t fpu_isr_tick_;
#endif
#ifdef MCU_TEST_LONG_OR_SCHED_STRESS
static eventset_t long_eventset_;
static sem_t long_sem_;
static msgq_t long_msgq_;
static msgq_storage(long_msgq_storage_, uint32_t, LONG_MSGQ_CAPACITY);
static pipe_t long_pipe_;
static pipe_storage(long_pipe_storage_, LONG_PIPE_SIZE);
#if OS_TIMER_ENABLE
static soft_timer_t long_timer_;
#endif
static volatile uint32_t long_notify_count_;
static volatile uint32_t long_event_isr_count_;
static volatile uint32_t long_event_timer_count_;
static volatile uint32_t long_sem_count_;
static volatile uint32_t long_msgq_tx_count_;
static volatile uint32_t long_msgq_rx_count_;
static volatile uint32_t long_pipe_tx_count_;
static volatile uint32_t long_pipe_rx_count_;
static volatile uint32_t long_timer_count_;
static volatile uint32_t long_delay_count_;
static volatile uint32_t long_yield_count_;
static volatile uint8_t long_pipe_next_read_;
#endif
#ifdef MCU_TEST_FPU_STRESS
static volatile uint32_t fpu_stress_count_[FPU_WORKER_COUNT];
#endif
#if OS_TRACE_ENABLE
static volatile uint32_t trace_ready_count_;
static volatile uint32_t trace_block_count_;
static volatile uint32_t trace_switch_count_;
static volatile uint32_t trace_exit_count_;
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void uart_write(const char *s)
{
  (void)HAL_UART_Transmit(&huart3,
                          (const uint8_t *)s,
                          (uint16_t)strlen(s),
                          HAL_MAX_DELAY);
}

static void uart_write_u32(uint32_t value)
{
  char buf[11];
  uint32_t pos = 0U;

  if (value == 0U) {
    uart_write("0");
    return;
  }

  while (value != 0U) {
    buf[pos] = (char)('0' + (value % 10U));
    value /= 10U;
    pos++;
  }

  while (pos != 0U) {
    pos--;
    (void)HAL_UART_Transmit(&huart3,
                            (const uint8_t *)&buf[pos],
                            1U,
                            HAL_MAX_DELAY);
  }
}

static void test_fail(const char *reason)
{
  uart_write("[G070-FIRST] FAIL: ");
  uart_write(reason);
  uart_write("\r\n");
  __disable_irq();
  for (;;) {
  }
}

void bus_on_publish_fail(bus_channel_t *channel)
{
  bus_publish_fail_count_++;
  bus_publish_fail_channel_ = channel;
}

static void test_check_done(void)
{
  if (!test_finished_ && notify_passed_ && delay_passed_ && yield_passed_ && idle_seen_) {
    test_finished_ = true;
  }
}

static void wait_worker_done(uint32_t id)
{
  while (!worker_done_[id]) {
    if (!task_notify_wait(100U)) {
      test_fail("worker timeout");
    }
  }

  if (!worker_result_[id]) {
    test_fail("worker result");
  }
}

static void drain_task_notify(void)
{
  while (task_notify_wait(OS_NO_WAIT)) {
  }
}

#if OS_TRACE_ENABLE
void os_trace_task_ready(const task_t *task)
{
  (void)task;
  trace_ready_count_++;
}

void os_trace_task_block(const task_t *task)
{
  (void)task;
  trace_block_count_++;
}

void os_trace_task_switch(const task_t *from, const task_t *to)
{
  (void)from;
  (void)to;
  trace_switch_count_++;
}

void os_trace_task_exit(const task_t *task)
{
  (void)task;
  trace_exit_count_++;
}
#endif

#if OS_TIMER_ENABLE
static void one_shot_cb(void *arg)
{
  (void)arg;
  one_shot_count_++;
  task_notify(&primitive_task_);
}

static void periodic_cb(void *arg)
{
  (void)arg;
  periodic_count_++;
  task_notify(&primitive_task_);
}

static void order_timer_cb(void *arg)
{
  if (timer_order_count_ < 2U) {
    timer_order_[timer_order_count_] = (uint32_t)(uintptr_t)arg;
  }
  timer_order_count_++;
  task_notify(&primitive_task_);
}
#endif

bool mcu_test_tick_enabled(void)
{
  return test_tick_enabled_;
}

void mcu_test_on_tick(void)
{
#ifdef MCU_TEST_LONG_OR_SCHED_STRESS
  os_tick_t now = os_tick_get();

  if ((now % LONG_NOTIFY_PERIOD_TICKS) == 0U) {
    task_notify(&long_notify_task_);
  }

  if ((now % LONG_EVENT_ISR_PERIOD_TICKS) == 0U) {
    eventset_set(&long_eventset_, LONG_EVENT_ISR_BIT);
  }

  if ((now % LONG_SEM_PERIOD_TICKS) == 0U) {
    sem_give(&long_sem_);
  }
#else
  if (!notify_sent_ && (os_tick_get() >= NOTIFY_TEST_TICK)) {
    notify_sent_ = true;
    task_notify(&notify_task_);
  }

  if (eventset_isr_pending_ &&
      os_tick_after_eq(os_tick_get(), eventset_isr_tick_)) {
    eventset_isr_pending_ = false;
    eventset_set((eventset_t *)eventset_isr_target_, eventset_isr_bits_);
  }

  if (sem_isr_pending_ &&
      os_tick_after_eq(os_tick_get(), sem_isr_tick_)) {
    sem_isr_pending_ = false;
    sem_give((sem_t *)sem_isr_target_);
  }

#ifdef MCU_TEST_FPU_REGS
  if (fpu_isr_pending_ &&
      os_tick_after_eq(os_tick_get(), fpu_isr_tick_)) {
      fpu_isr_pending_ = false;
      task_notify(&fpu_isr_task_);
  }
#endif
#endif
}

void scheduler_idle_hook(void)
{
  idle_seen_ = true;
#ifndef MCU_TEST_LONG_OR_SCHED_STRESS
  test_check_done();
#endif
  /* Keep the G070 DAPLink debug port attachable; do not enter WFI here. */
}

void on_assert_failure(const char *expr, const char *file, int line)
{
  (void)line;

#ifdef MCU_TEST_LONG_OR_SCHED_STRESS
  uart_write(STRESS_LOG_PREFIX " ASSERT: ");
#elif defined(MCU_TEST_BUS_STRESS)
  uart_write(BUS_STRESS_LOG_PREFIX " ASSERT: ");
#else
  uart_write("[G070-FIRST] ASSERT: ");
#endif
  uart_write(expr);
  uart_write(" @ ");
  uart_write(file);
  uart_write("\r\n");

  __disable_irq();
  for (;;) {
  }
}

#ifdef MCU_TEST_LONG_OR_SCHED_STRESS
static void long_fail(const char *reason)
{
  uart_write(STRESS_LOG_PREFIX " FAIL: ");
  uart_write(reason);
  uart_write("\r\n");
  __disable_irq();
  for (;;) {
  }
}

#if OS_TIMER_ENABLE
static void long_timer_cb(void *arg)
{
  (void)arg;
  long_timer_count_++;
  eventset_set(&long_eventset_, LONG_EVENT_TIMER_BIT);
  task_notify(&long_timer_task_);
}
#endif

static void long_notify_entry(void *arg)
{
  (void)arg;

  for (;;) {
    if (!task_notify_wait(1000U)) {
      long_fail("notify timeout");
    }
    long_notify_count_++;
  }
}

static void long_event_entry(void *arg)
{
  (void)arg;

  for (;;) {
    eventset_bits_t bits;

    bits = eventset_wait_any(&long_eventset_,
                             LONG_EVENT_ISR_BIT | LONG_EVENT_TIMER_BIT,
                             1000U);
    if (bits == 0U) {
      long_fail("event timeout");
    }

    if ((bits & LONG_EVENT_ISR_BIT) != 0U) {
      long_event_isr_count_++;
      eventset_clear(&long_eventset_, LONG_EVENT_ISR_BIT);
    }

    if ((bits & LONG_EVENT_TIMER_BIT) != 0U) {
      long_event_timer_count_++;
      eventset_clear(&long_eventset_, LONG_EVENT_TIMER_BIT);
    }
  }
}

static void long_sem_entry(void *arg)
{
  (void)arg;

  for (;;) {
    if (!sem_take(&long_sem_, 1000U)) {
      long_fail("sem timeout");
    }
    long_sem_count_++;
  }
}

static void long_msgq_tx_entry(void *arg)
{
  uint32_t value = 1U;

  (void)arg;

  for (;;) {
    if (!msgq_send(&long_msgq_, &value, 1000U)) {
      long_fail("msgq send timeout");
    }
    long_msgq_tx_count_++;
    value++;
    if ((value & 0x0FU) == 0U) {
      task_delay(1U);
    }
  }
}

static void long_msgq_rx_entry(void *arg)
{
  uint32_t expected = 1U;

  (void)arg;

  for (;;) {
    uint32_t value = 0U;

    if (!msgq_recv(&long_msgq_, &value, 1000U)) {
      long_fail("msgq recv timeout");
    }
    if (value != expected) {
      long_fail("msgq sequence");
    }
    long_msgq_rx_count_++;
    expected++;
    if ((expected & 0x0FU) == 0U) {
      task_delay(1U);
    }
  }
}

static void long_pipe_tx_entry(void *arg)
{
  uint8_t value = 0U;

  (void)arg;

  for (;;) {
    if (pipe_write(&long_pipe_, &value, 1U, 1000U) != 1U) {
      long_fail("pipe write timeout");
    }
    long_pipe_tx_count_++;
    value++;
    if ((value & 0x1FU) == 0U) {
      task_delay(1U);
    }
  }
}

static void long_pipe_rx_entry(void *arg)
{
  (void)arg;

  for (;;) {
    uint8_t value = 0U;

    if (pipe_read(&long_pipe_, &value, 1U, 1000U) != 1U) {
      long_fail("pipe read timeout");
    }
    if (value != long_pipe_next_read_) {
      long_fail("pipe sequence");
    }
    long_pipe_rx_count_++;
    long_pipe_next_read_++;
    if ((long_pipe_next_read_ & 0x1FU) == 0U) {
      task_delay(1U);
    }
  }
}

static void long_delay_entry(void *arg)
{
  os_tick_t last;

  (void)arg;

  last = os_tick_get();
  for (;;) {
    task_delay(10U);
    if (!os_tick_elapsed(os_tick_get(), last, 10U)) {
      long_fail("delay elapsed");
    }
    last = os_tick_get();
    long_delay_count_++;
  }
}

static void long_yield_record(char id)
{
  (void)id;
  long_yield_count_++;
}

static void long_yield_a_entry(void *arg)
{
  (void)arg;

  for (;;) {
    long_yield_record('A');
    task_yield();
  }
}

static void long_yield_b_entry(void *arg)
{
  (void)arg;

  for (;;) {
    long_yield_record('B');
    task_yield();
  }
}

static void long_yield_c_entry(void *arg)
{
  (void)arg;

  for (;;) {
    long_yield_record('C');
    task_delay(1U);
  }
}

#if OS_TIMER_ENABLE
static void long_timer_entry(void *arg)
{
  (void)arg;

  for (;;) {
    if (!task_notify_wait(1000U)) {
      long_fail("timer notify timeout");
    }
  }
}
#endif

static void long_print_counter(const char *name, uint32_t value)
{
  uart_write(" ");
  uart_write(name);
  uart_write("=");
  uart_write_u32(value);
}

static void long_control_entry(void *arg)
{
  os_tick_t last_report;

  (void)arg;

#if OS_TIMER_ENABLE
  timer_init(&long_timer_, long_timer_cb, NULL);
  timer_start(&long_timer_, LONG_TIMER_PERIOD_TICKS, LONG_TIMER_PERIOD_TICKS);
#endif

  last_report = os_tick_get();
  uart_write(STRESS_LOG_PREFIX " PASS: started\r\n");

  for (;;) {
    task_delay(1000U);

    if (os_tick_elapsed(os_tick_get(), last_report, LONG_REPORT_TICKS)) {
      uint32_t tick_snapshot;
      uint32_t notify_snapshot;
      uint32_t event_isr_snapshot;
      uint32_t event_timer_snapshot;
      uint32_t sem_snapshot;
      uint32_t msgq_tx_snapshot;
      uint32_t msgq_rx_snapshot;
      uint32_t pipe_tx_snapshot;
      uint32_t pipe_rx_snapshot;
#if OS_TIMER_ENABLE
      uint32_t timer_snapshot;
#endif
      uint32_t delay_snapshot;
      uint32_t yield_snapshot;
#ifdef MCU_TEST_FPU_STRESS
      uint32_t fpu0_snapshot;
      uint32_t fpu1_snapshot;
#endif
#if TASK_STACK_WATERMARK_ENABLE
      uint32_t idle_used_snapshot;
      uint32_t idle_unused_snapshot;
#endif

      last_report = os_tick_get();
      HAL_GPIO_TogglePin(ST_LED1_GPIO_Port, ST_LED1_Pin);

      __disable_irq();
      tick_snapshot = os_tick_get();
      notify_snapshot = long_notify_count_;
      event_isr_snapshot = long_event_isr_count_;
      event_timer_snapshot = long_event_timer_count_;
      sem_snapshot = long_sem_count_;
      msgq_tx_snapshot = long_msgq_tx_count_;
      msgq_rx_snapshot = long_msgq_rx_count_;
      pipe_tx_snapshot = long_pipe_tx_count_;
      pipe_rx_snapshot = long_pipe_rx_count_;
#if OS_TIMER_ENABLE
      timer_snapshot = long_timer_count_;
#endif
      delay_snapshot = long_delay_count_;
      yield_snapshot = long_yield_count_;
#ifdef MCU_TEST_FPU_STRESS
      fpu0_snapshot = fpu_stress_count_[0];
      fpu1_snapshot = fpu_stress_count_[1];
#endif
#if TASK_STACK_WATERMARK_ENABLE
      idle_used_snapshot = (uint32_t)scheduler_idle_stack_used();
      idle_unused_snapshot = (uint32_t)scheduler_idle_stack_unused();
#endif
      __enable_irq();

      uart_write(STRESS_LOG_PREFIX " OK tick=");
      uart_write_u32(tick_snapshot);
      long_print_counter("notify", notify_snapshot);
      long_print_counter("event_isr", event_isr_snapshot);
      long_print_counter("event_timer", event_timer_snapshot);
      long_print_counter("sem", sem_snapshot);
      long_print_counter("msgq_tx", msgq_tx_snapshot);
      long_print_counter("msgq_rx", msgq_rx_snapshot);
      long_print_counter("pipe_tx", pipe_tx_snapshot);
      long_print_counter("pipe_rx", pipe_rx_snapshot);
#if OS_TIMER_ENABLE
      long_print_counter("timer", timer_snapshot);
#endif
      long_print_counter("delay", delay_snapshot);
      long_print_counter("yield", yield_snapshot);
#ifdef MCU_TEST_FPU_STRESS
      long_print_counter("fpu0", fpu0_snapshot);
      long_print_counter("fpu1", fpu1_snapshot);
#endif
#if TASK_STACK_WATERMARK_ENABLE
      uart_write(" idle_used=");
      uart_write_u32(idle_used_snapshot);
      uart_write(" idle_unused=");
      uart_write_u32(idle_unused_snapshot);
#endif
      uart_write(" err=0\r\n");
    }
  }
}
#endif

#ifdef MCU_TEST_BUS_STRESS
static void bus_stress_fail(const char *reason)
{
  bus_stress_error_count_++;
  uart_write(BUS_STRESS_LOG_PREFIX " FAIL: ");
  uart_write(reason);
  uart_write("\r\n");
  __disable_irq();
  for (;;) {
  }
}

static void bus_stress_print_counter(const char *name, uint32_t value)
{
  uart_write(" ");
  uart_write(name);
  uart_write("=");
  uart_write_u32(value);
}

static uint32_t bus_stress_event_payload_value(uint32_t channel_index,
                                               uint32_t seq)
{
  return seq ^ (0xA5A50000UL | channel_index);
}

static void bus_stress_publish_event(bus_channel_t *channel,
                                     uint32_t channel_index)
{
  bus_test_event_t event;
  uint32_t seq;

  seq = bus_stress_event_pub_[channel_index];
  bus_stress_event_pub_[channel_index]++;
  event.id = seq;
  event.value = bus_stress_event_payload_value(channel_index, seq);
  bus_publish(channel, &event);

  if (bus_publish_fail_count_ != 0U) {
    bus_stress_fail("event publish full");
  }
}

static void bus_stress_publish_state(bus_channel_t *channel,
                                     uint32_t channel_index)
{
  bus_test_state_t state;

  bus_stress_state_pub_[channel_index]++;
  state.value = bus_stress_state_pub_[channel_index];
  bus_publish(channel, &state);
}

static void bus_stress_pub_entry(void *arg)
{
  (void)arg;

  for (;;) {
    bus_stress_publish_event(&bus_key_event_channel_, 0U);
    bus_stress_publish_state(&bus_ui_state_channel_, 0U);
    bus_stress_publish_event(&bus_sensor_event_channel_, 1U);
    bus_stress_publish_state(&bus_system_state_channel_, 1U);

    bus_stress_yield_count_++;
    if ((bus_stress_yield_count_ & 0xFFU) == 0U) {
      task_delay(1U);
    } else {
      task_yield();
    }
  }
}

static void bus_stress_drain_event(bus_subscriber_t *subscriber,
                                   uint32_t subscriber_index,
                                   bus_channel_t *channel,
                                   uint32_t channel_index,
                                   uint32_t *expected_seq)
{
  bool read_any = false;

  for (;;) {
    bus_test_event_t event;
    uint32_t seq;

    if (!bus_event_read(subscriber, channel, &event, &seq)) {
      break;
    }

    read_any = true;
    if ((seq != *expected_seq) ||
        (event.id != seq) ||
        (event.value != bus_stress_event_payload_value(channel_index, seq))) {
      bus_stress_fail("event sequence");
    }

    (*expected_seq)++;
    bus_stress_event_rx_[subscriber_index][channel_index]++;
  }

  if (!read_any) {
    bus_stress_fail("event ready empty");
  }
}

static void bus_stress_read_state(bus_subscriber_t *subscriber,
                                  uint32_t subscriber_index,
                                  bus_channel_t *channel,
                                  uint32_t channel_index,
                                  uint32_t *last_generation)
{
  bus_test_state_t state;
  uint32_t generation;

  if (!bus_state_read(subscriber, channel, &state, &generation)) {
    bus_stress_fail("state read empty");
  }

  if ((generation == 0U) ||
      (generation < *last_generation) ||
      (state.value != generation)) {
    bus_stress_fail("state generation");
  }

  *last_generation = generation;
  bus_stress_state_rx_[subscriber_index][channel_index]++;
}

static void bus_stress_sub_entry(void *arg)
{
  uint32_t subscriber_index;
  bus_subscriber_t *subscriber;
  uint32_t expected_event_seq[2] = { 0U, 0U };
  uint32_t last_state_generation[2] = { 0U, 0U };

  subscriber_index = (uint32_t)(uintptr_t)arg;
  subscriber = (subscriber_index == 0U) ?
               &bus_ui_subscriber_ :
               &bus_log_subscriber_;

  for (;;) {
    bus_channel_t *channel;

    channel = bus_subscriber_recv(subscriber, 1000U);
    if (channel == NULL) {
      bus_stress_timeout_count_[subscriber_index]++;
      bus_stress_fail("subscriber timeout");
    }

    bus_stress_ready_count_[subscriber_index]++;

    if (channel == &bus_key_event_channel_) {
      bus_stress_drain_event(subscriber,
                             subscriber_index,
                             channel,
                             0U,
                             &expected_event_seq[0]);
    } else if (channel == &bus_sensor_event_channel_) {
      bus_stress_drain_event(subscriber,
                             subscriber_index,
                             channel,
                             1U,
                             &expected_event_seq[1]);
    } else if (channel == &bus_ui_state_channel_) {
      bus_stress_read_state(subscriber,
                            subscriber_index,
                            channel,
                            0U,
                            &last_state_generation[0]);
    } else if (channel == &bus_system_state_channel_) {
      bus_stress_read_state(subscriber,
                            subscriber_index,
                            channel,
                            1U,
                            &last_state_generation[1]);
    } else {
      bus_stress_fail("unknown ready channel");
    }
  }
}

static void bus_stress_control_entry(void *arg)
{
  os_tick_t last_report;
  uint32_t prev_event_pub_total = 0U;
  uint32_t prev_event_rx_total = 0U;
  uint32_t prev_state_rx_total = 0U;

  (void)arg;

  last_report = os_tick_get();
  uart_write(BUS_STRESS_LOG_PREFIX " PASS: started\r\n");

  for (;;) {
    task_delay(1000U);

    if (os_tick_elapsed(os_tick_get(), last_report, BUS_STRESS_REPORT_TICKS)) {
      uint32_t tick_snapshot;
      uint32_t event_pub0;
      uint32_t event_pub1;
      uint32_t event_rx00;
      uint32_t event_rx01;
      uint32_t event_rx10;
      uint32_t event_rx11;
      uint32_t state_pub0;
      uint32_t state_pub1;
      uint32_t state_rx00;
      uint32_t state_rx01;
      uint32_t state_rx10;
      uint32_t state_rx11;
      uint32_t ready0;
      uint32_t ready1;
      uint32_t timeout0;
      uint32_t timeout1;
      uint32_t yield_snapshot;
      uint32_t publish_fail_snapshot;
      uint32_t error_snapshot;
#if TASK_STACK_WATERMARK_ENABLE
      uint32_t idle_used_snapshot;
      uint32_t idle_unused_snapshot;
#endif
      uint32_t event_pub_total;
      uint32_t event_rx_total;
      uint32_t state_rx_total;

      last_report = os_tick_get();
      HAL_GPIO_TogglePin(ST_LED1_GPIO_Port, ST_LED1_Pin);

      __disable_irq();
      tick_snapshot = os_tick_get();
      event_pub0 = bus_stress_event_pub_[0];
      event_pub1 = bus_stress_event_pub_[1];
      event_rx00 = bus_stress_event_rx_[0][0];
      event_rx01 = bus_stress_event_rx_[0][1];
      event_rx10 = bus_stress_event_rx_[1][0];
      event_rx11 = bus_stress_event_rx_[1][1];
      state_pub0 = bus_stress_state_pub_[0];
      state_pub1 = bus_stress_state_pub_[1];
      state_rx00 = bus_stress_state_rx_[0][0];
      state_rx01 = bus_stress_state_rx_[0][1];
      state_rx10 = bus_stress_state_rx_[1][0];
      state_rx11 = bus_stress_state_rx_[1][1];
      ready0 = bus_stress_ready_count_[0];
      ready1 = bus_stress_ready_count_[1];
      timeout0 = bus_stress_timeout_count_[0];
      timeout1 = bus_stress_timeout_count_[1];
      yield_snapshot = bus_stress_yield_count_;
      publish_fail_snapshot = bus_publish_fail_count_;
      error_snapshot = bus_stress_error_count_;
#if TASK_STACK_WATERMARK_ENABLE
      idle_used_snapshot = (uint32_t)scheduler_idle_stack_used();
      idle_unused_snapshot = (uint32_t)scheduler_idle_stack_unused();
#endif
      __enable_irq();

      event_pub_total = event_pub0 + event_pub1;
      event_rx_total = event_rx00 + event_rx01 + event_rx10 + event_rx11;
      state_rx_total = state_rx00 + state_rx01 + state_rx10 + state_rx11;

      if ((publish_fail_snapshot != 0U) ||
          (error_snapshot != 0U) ||
          (event_pub_total == prev_event_pub_total) ||
          (event_rx_total == prev_event_rx_total) ||
          (state_rx_total == prev_state_rx_total)) {
        bus_stress_fail("progress");
      }

      prev_event_pub_total = event_pub_total;
      prev_event_rx_total = event_rx_total;
      prev_state_rx_total = state_rx_total;

      uart_write(BUS_STRESS_LOG_PREFIX " OK tick=");
      uart_write_u32(tick_snapshot);
      bus_stress_print_counter("ev0_pub", event_pub0);
      bus_stress_print_counter("ev0_rx0", event_rx00);
      bus_stress_print_counter("ev0_rx1", event_rx10);
      bus_stress_print_counter("ev1_pub", event_pub1);
      bus_stress_print_counter("ev1_rx0", event_rx01);
      bus_stress_print_counter("ev1_rx1", event_rx11);
      bus_stress_print_counter("st0_pub", state_pub0);
      bus_stress_print_counter("st0_rx0", state_rx00);
      bus_stress_print_counter("st0_rx1", state_rx10);
      bus_stress_print_counter("st1_pub", state_pub1);
      bus_stress_print_counter("st1_rx0", state_rx01);
      bus_stress_print_counter("st1_rx1", state_rx11);
      bus_stress_print_counter("ready0", ready0);
      bus_stress_print_counter("ready1", ready1);
      bus_stress_print_counter("timeout0", timeout0);
      bus_stress_print_counter("timeout1", timeout1);
      bus_stress_print_counter("yield", yield_snapshot);
      bus_stress_print_counter("publish_fail", publish_fail_snapshot);
#if TASK_STACK_WATERMARK_ENABLE
      uart_write(" idle_used=");
      uart_write_u32(idle_used_snapshot);
      uart_write(" idle_unused=");
      uart_write_u32(idle_unused_snapshot);
#endif
      uart_write(" err=0\r\n");
    }
  }
}
#endif

static void notify_entry(void *arg)
{
  bool ok;

  if (arg != (void *)0x12345678UL) {
    test_fail("task arg");
  }

  uart_write("[G070-FIRST] notify wait\r\n");
  ok = task_notify_wait(OS_WAIT_FOREVER);
  if (!ok) {
    test_fail("notify wait");
  }

  notify_passed_ = true;
  uart_write("[G070-FIRST] notify ok\r\n");

  task_notify(task_current());
  if (!task_notify_wait(OS_NO_WAIT)) {
    test_fail("notify pending");
  }

  if (task_notify_wait(OS_NO_WAIT)) {
    test_fail("notify no-wait");
  }

  if (task_notify_wait(5U)) {
    test_fail("notify timeout");
  }

  uart_write("[G070-FIRST] notify modes ok\r\n");
  test_check_done();

  for (;;) {
    task_delay(1000U);
  }
}

static void delay_entry(void *arg)
{
  os_tick_t start;
  os_tick_t end;

  (void)arg;

  start = os_tick_get();
  uart_write("[G070-FIRST] delay start\r\n");
  task_delay(DELAY_TEST_TICKS);
  end = os_tick_get();

  if (!os_tick_elapsed(end, start, DELAY_TEST_TICKS)) {
    test_fail("delay elapsed");
  }

  delay_passed_ = true;
  uart_write("[G070-FIRST] delay ok\r\n");
  test_check_done();

  for (;;) {
    task_delay(1000U);
  }
}

static void yield_record(char id)
{
  if (yield_passed_) {
    return;
  }

  if (yield_expected_ != id) {
    test_fail("yield fifo");
  }

  yield_steps_++;
  if (id == 'A') {
    yield_expected_ = 'B';
  } else if (id == 'B') {
    yield_expected_ = 'C';
  } else {
    yield_expected_ = 'A';
  }

  if (yield_steps_ >= YIELD_TEST_STEPS) {
    yield_passed_ = true;
    uart_write("[G070-FIRST] yield fifo ok\r\n");
    test_check_done();
  }
}

static void yield_a_entry(void *arg)
{
  (void)arg;

  for (;;) {
    if (test_finished_) {
      task_delay(1000U);
      continue;
    }

    if (yield_passed_) {
      task_delay(1000U);
      continue;
    }

    yield_record('A');
    task_yield();
  }
}

static void yield_b_entry(void *arg)
{
  (void)arg;

  for (;;) {
    if (test_finished_) {
      task_delay(1000U);
      continue;
    }

    if (yield_passed_) {
      task_delay(1000U);
      continue;
    }

    yield_record('B');
    task_yield();
  }
}

static void yield_c_entry(void *arg)
{
  (void)arg;

  for (;;) {
    if (test_finished_) {
      task_delay(1000U);
      continue;
    }

    if (yield_passed_) {
      task_delay(1000U);
      continue;
    }

    yield_record('C');
    task_yield();
  }
}

static void worker_entry(void *arg)
{
  uint32_t id = (uint32_t)(uintptr_t)arg;

  for (;;) {
    uint32_t value;
    worker_cmd_t cmd;

    (void)task_notify_wait(OS_WAIT_FOREVER);

    cmd = worker_cmd_[id];
    worker_result_[id] = false;

    if (cmd == WORKER_CMD_EVENT_ANY) {
      worker_result_[id] =
          (eventset_wait_any(&test_eventset_, 0x1U, OS_WAIT_FOREVER) == 0x1U);
    } else if (cmd == WORKER_CMD_SEM_TAKE) {
      worker_result_[id] = sem_take(&test_sem_, OS_WAIT_FOREVER);
      if (worker_result_[id] && (worker_order_count_ < WORKER_COUNT)) {
        worker_order_[worker_order_count_] = id + 1U;
        worker_order_count_++;
      }
    } else if (cmd == WORKER_CMD_MSGQ_RECV) {
      value = 0U;
      worker_result_[id] = msgq_recv(&test_msgq_, &value, OS_WAIT_FOREVER);
      worker_value_[id] = value;
    } else if (cmd == WORKER_CMD_MSGQ_SEND) {
      value = worker_value_[id];
      worker_result_[id] = msgq_send(&test_msgq_, &value, OS_WAIT_FOREVER);
    } else if (cmd == WORKER_CMD_PIPE_READ) {
      uint8_t data[2] = {0U, 0U};
      uint16_t read = pipe_read(&test_pipe_, data, 2U, OS_WAIT_FOREVER);
      worker_result_[id] = (read == 2U);
      worker_value_[id] = ((uint32_t)read << 16) |
                          ((uint32_t)data[0] << 8) |
                          (uint32_t)data[1];
    } else if (cmd == WORKER_CMD_PIPE_WRITE) {
      uint8_t data = (uint8_t)worker_value_[id];
      worker_result_[id] =
          (pipe_write(&test_pipe_, &data, 1U, OS_WAIT_FOREVER) == 1U);
    } else if (cmd == WORKER_CMD_DELAY) {
      task_delay((os_tick_t)worker_value_[id]);
      worker_result_[id] = true;
      if (worker_order_count_ < WORKER_COUNT) {
        worker_order_[worker_order_count_] = id + 1U;
        worker_order_count_++;
      }
    } else {
      worker_result_[id] = false;
    }

    worker_cmd_[id] = WORKER_CMD_NONE;
    worker_done_[id] = true;
    task_notify(&primitive_task_);
  }
}

static void return_entry(void *arg)
{
  (void)arg;
  return_task_ran_ = true;
}

static void peer_entry(void *arg)
{
  (void)arg;

  for (;;) {
    (void)task_notify_wait(OS_WAIT_FOREVER);
    peer_ran_ = true;
    task_notify(&primitive_task_);
  }
}

#ifdef MCU_TEST_FPU_REGS
static void fpu_touch_arithmetic(void)
{
  volatile float a = 1.25f;
  volatile float b = 2.50f;
  volatile float c;

  c = (a * b) + 0.5f;
  if ((c < 3.62f) || (c > 3.63f)) {
    test_fail("fpu arithmetic");
  }
}

static void fpu_load_low_(const uint32_t *values)
{
  __ASM volatile (
      "vldmia %0, {s0-s15}\n"
      :
      : "r" (values)
      : "memory");
}

static void fpu_store_low_(uint32_t *values)
{
  __ASM volatile (
      "vstmia %0, {s0-s15}\n"
      :
      : "r" (values)
      : "memory");
}

static void fpu_load_high_(const uint32_t *values)
{
  __ASM volatile (
      "vldmia %0, {s16-s31}\n"
      :
      : "r" (values)
      : "memory");
}

static void fpu_store_high_(uint32_t *values)
{
  __ASM volatile (
      "vstmia %0, {s16-s31}\n"
      :
      : "r" (values)
      : "memory");
}

static void fpu_set_fpscr_(uint32_t value)
{
  __ASM volatile (
      "vmsr fpscr, %0\n"
      :
      : "r" (value)
      : "memory");
}

static uint32_t fpu_get_fpscr_(void)
{
  uint32_t value;

  __ASM volatile (
      "vmrs %0, fpscr\n"
      : "=r" (value)
      :
      : "memory");

  return value;
}

static void fpu_make_pattern_(uint32_t *values, uint32_t base)
{
  uint32_t i;

  for (i = 0U; i < 16U; i++) {
    values[i] = base + i;
  }
}

static void fpu_write_pattern_(uint32_t base, uint32_t fpscr)
{
  uint32_t values[16];

  fpu_make_pattern_(values, base);
  fpu_load_low_(values);
  fpu_make_pattern_(values, base + 0x100U);
  fpu_load_high_(values);
  fpu_set_fpscr_(fpscr);
}

static void fpu_check_pattern_(uint32_t base, uint32_t fpscr, const char *reason)
{
  uint32_t values[16];
  uint32_t i;

  fpu_store_low_(values);
  for (i = 0U; i < 16U; i++) {
    if (values[i] != (base + i)) {
      test_fail(reason);
    }
  }

  fpu_store_high_(values);
  for (i = 0U; i < 16U; i++) {
    if (values[i] != (base + 0x100U + i)) {
      test_fail(reason);
    }
  }

  if ((fpu_get_fpscr_() & 0x0F000000UL) != fpscr) {
    test_fail(reason);
  }
}

#ifdef MCU_TEST_FPU_STRESS
static void fpu_write_high_pattern_(uint32_t base)
{
  uint32_t values[16];

  fpu_make_pattern_(values, base);
  fpu_load_high_(values);
}

static void fpu_check_high_pattern_(uint32_t base, const char *reason)
{
  uint32_t values[16];
  uint32_t i;

  fpu_store_high_(values);
  for (i = 0U; i < 16U; i++) {
    if (values[i] != (base + i)) {
      test_fail(reason);
    }
  }
}
#endif

static void fpu_worker_entry(void *arg)
{
  uint32_t id = (uint32_t)(uintptr_t)arg;
  uint32_t base = 0x3F800000UL + (id * 0x00010000UL);
  uint32_t fpscr = (id + 1U) << 24;
  uint32_t i;

  for (;;) {
    (void)task_notify_wait(OS_WAIT_FOREVER);

    fpu_touch_arithmetic();
    fpu_write_pattern_(base, fpscr);

    for (i = 0U; i < FPU_STRESS_LOOPS; i++) {
      fpu_check_pattern_(base, fpscr, "fpu worker before yield");
      task_yield();
      fpu_check_pattern_(base, fpscr, "fpu worker after yield");
      task_delay(1U);
      fpu_check_pattern_(base, fpscr, "fpu worker after delay");
    }

    fpu_worker_done_[id] = true;
    task_notify(&primitive_task_);
  }
}

static void fpu_preempt_high_entry(void *arg)
{
  (void)arg;

  for (;;) {
    (void)task_notify_wait(OS_WAIT_FOREVER);
    fpu_touch_arithmetic();
    fpu_write_pattern_(0x40400000UL, 0x03000000UL);
    fpu_check_pattern_(0x40400000UL, 0x03000000UL, "fpu preempt high");
    fpu_preempt_high_done_ = true;
    task_notify(&primitive_task_);
  }
}

static void fpu_preempt_low_entry(void *arg)
{
  (void)arg;

  for (;;) {
    (void)task_notify_wait(OS_WAIT_FOREVER);
    fpu_preempt_high_done_ = false;
    fpu_touch_arithmetic();
    fpu_write_pattern_(0x40800000UL, 0x04000000UL);
    task_notify(&fpu_preempt_high_task_);

    if (!fpu_preempt_high_done_) {
      test_fail("fpu high did not preempt");
    }

    fpu_check_pattern_(0x40800000UL, 0x04000000UL, "fpu preempt low");
    fpu_preempt_low_done_ = true;
    task_notify(&primitive_task_);
  }
}

static void fpu_isr_entry(void *arg)
{
  (void)arg;

  for (;;) {
    (void)task_notify_wait(OS_WAIT_FOREVER);

    fpu_touch_arithmetic();
    fpu_write_pattern_(0x40A00000UL, 0x05000000UL);
    fpu_isr_pending_ = true;
    fpu_isr_tick_ = os_tick_get() + 5U;
    if (!task_notify_wait(100U)) {
      test_fail("fpu isr wait");
    }
    fpu_check_pattern_(0x40A00000UL, 0x05000000UL, "fpu isr notify");

    eventset_init(&test_eventset_);
    fpu_write_pattern_(0x40C00000UL, 0x06000000UL);
    eventset_isr_target_ = &test_eventset_;
    eventset_isr_bits_ = 0x1U;
    eventset_isr_tick_ = os_tick_get() + 5U;
    eventset_isr_pending_ = true;
    if (eventset_wait_any(&test_eventset_, 0x1U, 100U) != 0x1U) {
      test_fail("fpu isr event wait");
    }
    fpu_check_pattern_(0x40C00000UL, 0x06000000UL, "fpu isr event");

    sem_init(&test_sem_, 0U, 1U);
    fpu_write_pattern_(0x40E00000UL, 0x07000000UL);
    sem_isr_target_ = &test_sem_;
    sem_isr_tick_ = os_tick_get() + 5U;
    sem_isr_pending_ = true;
    if (!sem_take(&test_sem_, 100U)) {
      test_fail("fpu isr sem wait");
    }
    fpu_check_pattern_(0x40E00000UL, 0x07000000UL, "fpu isr sem");

    fpu_isr_done_ = true;
    task_notify(&primitive_task_);
  }
}

#ifdef MCU_TEST_FPU_STRESS
static void fpu_stress_entry(void *arg)
{
  uint32_t id = (uint32_t)(uintptr_t)arg;
  uint32_t base = 0x41000000UL + (id * 0x00020000UL);

  fpu_touch_arithmetic();

  for (;;) {
    /*
     * Long-run stress checks S16-S31 only. These high FP registers are
     * callee-saved by the hard-float ABI and software-saved by the context
     * switch port. S0-S15 and FPSCR are already covered by the focused FPU
     * functional test; treating S0-S15 as persistent across arbitrary C calls
     * would violate the ABI because they are caller-saved registers.
     */
    fpu_write_high_pattern_(base);
    fpu_check_high_pattern_(base, "fpu stress write");

    task_yield();
    fpu_check_high_pattern_(base, "fpu stress yield");

    task_delay(1U);
    fpu_check_high_pattern_(base, "fpu stress delay");

    fpu_touch_arithmetic();
    fpu_check_high_pattern_(base, "fpu stress arithmetic");

    task_notify(task_current());
    if (!task_notify_wait(100U)) {
      test_fail("fpu stress notify");
    }
    fpu_check_high_pattern_(base, "fpu stress notify check");

    fpu_stress_count_[id]++;
    base += 0x10U;
    if ((base & 0x0000FFF0UL) == 0U) {
      base = 0x41000000UL + (id * 0x00020000UL);
    }
  }
}
#endif

static void primitive_wait_for_fpu_done_(volatile bool *flag, const char *reason)
{
  while (!*flag) {
    if (!task_notify_wait(1000U)) {
      test_fail(reason);
    }
  }
}

static void primitive_test_fpu_regs(void)
{
  fpu_worker_done_[0] = false;
  fpu_worker_done_[1] = false;

  task_notify(&fpu_task_1_);
  task_notify(&fpu_task_2_);
  while (!fpu_worker_done_[0] || !fpu_worker_done_[1]) {
    if (!task_notify_wait(1000U)) {
      test_fail("fpu workers timeout");
    }
  }

  fpu_preempt_low_done_ = false;
  fpu_preempt_high_done_ = false;
  task_notify(&fpu_preempt_low_task_);
  primitive_wait_for_fpu_done_(&fpu_preempt_low_done_, "fpu preempt timeout");

  fpu_isr_done_ = false;
  task_notify(&fpu_isr_task_);
  primitive_wait_for_fpu_done_(&fpu_isr_done_, "fpu isr timeout");

  drain_task_notify();
  uart_write("[G070-FPU] regs ok\r\n");
}
#endif

static void primitive_test_scheduler_extra(void)
{
  eventset_init(&test_eventset_);
  eventset_set(&test_eventset_, 0x1U);
  worker_done_[0] = false;
  worker_cmd_[0] = WORKER_CMD_EVENT_ANY;
  task_notify(&worker_task_1_);
  if (!worker_done_[0]) {
    test_fail("priority preempt");
  }
  drain_task_notify();

  peer_ran_ = false;
  task_notify(&peer_task_);
  if (peer_ran_) {
    test_fail("same priority preempt");
  }
  task_yield();
  if (!peer_ran_) {
    test_fail("same priority yield");
  }
  drain_task_notify();

  uart_write("[G070-PRIM] scheduler extra ok\r\n");
}

static void primitive_test_eventset(void)
{
  eventset_init(&test_eventset_);

  if (eventset_wait_any(&test_eventset_, 0x1U, OS_NO_WAIT) != 0U) {
    test_fail("event no-wait");
  }

  eventset_set(&test_eventset_, 0x1U);
  if (eventset_wait_any(&test_eventset_, 0x3U, OS_NO_WAIT) != 0x1U) {
    test_fail("event any");
  }
  if (eventset_wait_all(&test_eventset_, 0x3U, OS_NO_WAIT) != 0U) {
    test_fail("event all early");
  }
  eventset_set(&test_eventset_, 0x2U);
  if (eventset_wait_all(&test_eventset_, 0x3U, OS_NO_WAIT) != 0x3U) {
    test_fail("event all");
  }
  eventset_clear(&test_eventset_, 0x1U);
  if (eventset_get(&test_eventset_) != 0x2U) {
    test_fail("event clear");
  }
  eventset_clear(&test_eventset_, 0x2U);

  worker_done_[0] = false;
  worker_done_[1] = false;
  worker_cmd_[0] = WORKER_CMD_EVENT_ANY;
  worker_cmd_[1] = WORKER_CMD_EVENT_ANY;
  task_notify(&worker_task_1_);
  task_notify(&worker_task_2_);
  task_delay(2U);
  eventset_set(&test_eventset_, 0x1U);
  wait_worker_done(0U);
  wait_worker_done(1U);

  eventset_clear(&test_eventset_, 0x1U);
  worker_done_[0] = false;
  worker_cmd_[0] = WORKER_CMD_EVENT_ANY;
  task_notify(&worker_task_1_);
  task_delay(2U);
  eventset_isr_target_ = &test_eventset_;
  eventset_isr_bits_ = 0x1U;
  eventset_isr_tick_ = os_tick_get() + 5U;
  eventset_isr_pending_ = true;
  wait_worker_done(0U);

  uart_write("[G070-PRIM] eventset ok\r\n");
}

static void primitive_test_sem(void)
{
  sem_init(&test_sem_, 1U, 2U);
  if (!sem_take(&test_sem_, OS_NO_WAIT)) {
    test_fail("sem initial");
  }
  if (sem_take(&test_sem_, OS_NO_WAIT)) {
    test_fail("sem empty no-wait");
  }
  sem_give(&test_sem_);
  if (!sem_take(&test_sem_, OS_NO_WAIT)) {
    test_fail("sem give/take");
  }

  sem_init(&test_sem_, 0U, 1U);
  worker_done_[0] = false;
  worker_done_[1] = false;
  worker_order_count_ = 0U;
  worker_cmd_[0] = WORKER_CMD_SEM_TAKE;
  worker_cmd_[1] = WORKER_CMD_SEM_TAKE;
  task_notify(&worker_task_1_);
  task_notify(&worker_task_2_);
  task_delay(2U);
  sem_give(&test_sem_);
  wait_worker_done(0U);
  sem_give(&test_sem_);
  wait_worker_done(1U);
  if ((worker_order_count_ != 2U) ||
      (worker_order_[0] != 1U) ||
      (worker_order_[1] != 2U)) {
    test_fail("sem waiter order");
  }

  sem_init(&test_sem_, 0U, 1U);
  worker_done_[0] = false;
  worker_cmd_[0] = WORKER_CMD_SEM_TAKE;
  task_notify(&worker_task_1_);
  task_delay(2U);
  sem_isr_target_ = &test_sem_;
  sem_isr_tick_ = os_tick_get() + 5U;
  sem_isr_pending_ = true;
  wait_worker_done(0U);

  uart_write("[G070-PRIM] sem ok\r\n");
}

static void primitive_test_task_return(void)
{
  os_tick_t start;

  start = os_tick_get();
  while (!return_task_ran_ ||
         (task_get_state(&return_task_) != TASK_STATE_TERMINATED)) {
    if (os_tick_elapsed(os_tick_get(), start, 100U)) {
      test_fail("task return");
    }
    task_delay(1U);
  }

  uart_write("[G070-PRIM] task return ok\r\n");
}

static void primitive_test_delay_extra(void)
{
  os_tick_t start;

  start = os_tick_get();
  task_delay(0U);
  if (os_tick_elapsed(os_tick_get(), start, 5U)) {
    test_fail("delay zero");
  }

  worker_order_count_ = 0U;
  worker_done_[0] = false;
  worker_done_[1] = false;
  worker_value_[0] = 20U;
  worker_value_[1] = 10U;
  worker_cmd_[0] = WORKER_CMD_DELAY;
  worker_cmd_[1] = WORKER_CMD_DELAY;
  task_notify(&worker_task_1_);
  task_notify(&worker_task_2_);
  wait_worker_done(1U);
  wait_worker_done(0U);
  if ((worker_order_count_ != 2U) ||
      (worker_order_[0] != 2U) ||
      (worker_order_[1] != 1U)) {
    test_fail("delay order");
  }

  worker_order_count_ = 0U;
  worker_done_[0] = false;
  worker_done_[1] = false;
  worker_value_[0] = 10U;
  worker_value_[1] = 10U;
  worker_cmd_[0] = WORKER_CMD_DELAY;
  worker_cmd_[1] = WORKER_CMD_DELAY;
  task_notify(&worker_task_1_);
  task_notify(&worker_task_2_);
  wait_worker_done(0U);
  wait_worker_done(1U);
  if ((worker_order_count_ != 2U) ||
      (worker_order_[0] != 1U) ||
      (worker_order_[1] != 2U)) {
    test_fail("delay same tick fifo");
  }

  start = os_tick_get();
  task_delay(1000U);
  if (!os_tick_elapsed(os_tick_get(), start, 1000U)) {
    test_fail("delay 1s");
  }

  start = os_tick_get();
  task_delay(2000U);
  if (!os_tick_elapsed(os_tick_get(), start, 2000U)) {
    test_fail("delay 2s");
  }

  uart_write("[G070-PRIM] delay extra ok\r\n");
}

static void primitive_test_stack_watermark(void)
{
#if TASK_STACK_WATERMARK_ENABLE
  size_t used;
  size_t unused;
  size_t idle_used;
  size_t idle_unused;

  used = task_get_stack_used(&primitive_task_);
  unused = task_get_stack_unused(&primitive_task_);
  if ((used == 0U) || (unused == 0U) ||
      ((used + unused) != sizeof(primitive_stack_))) {
    test_fail("stack watermark");
  }

  idle_used = scheduler_idle_stack_used();
  idle_unused = scheduler_idle_stack_unused();
  if ((idle_used == 0U) || (idle_unused == 0U) ||
      ((idle_used + idle_unused) != SCHED_IDLE_STACK_SIZE)) {
    test_fail("idle stack watermark");
  }

  uart_write("[G070-PRIM] idle stack used=");
  uart_write_u32((uint32_t)idle_used);
  uart_write(" unused=");
  uart_write_u32((uint32_t)idle_unused);
  uart_write(" size=");
  uart_write_u32((uint32_t)SCHED_IDLE_STACK_SIZE);
  uart_write("\r\n");

  uart_write("[G070-PRIM] stack watermark ok\r\n");
#endif
}

static void primitive_test_trace(void)
{
#if OS_TRACE_ENABLE
  if ((trace_ready_count_ == 0U) ||
      (trace_block_count_ == 0U) ||
      (trace_switch_count_ == 0U) ||
      (trace_exit_count_ == 0U)) {
    test_fail("trace");
  }

  uart_write("[G070-PRIM] trace ok\r\n");
#endif
}

static void primitive_test_msgq(void)
{
  uint32_t value;

  msgq_init(&test_msgq_, test_msgq_storage_, sizeof(uint32_t), 2U);

  value = 11U;
  if (!msgq_send(&test_msgq_, &value, OS_NO_WAIT)) {
    test_fail("msgq send 11");
  }
  value = 22U;
  if (!msgq_send(&test_msgq_, &value, OS_NO_WAIT)) {
    test_fail("msgq send 22");
  }
  value = 33U;
  if (msgq_send(&test_msgq_, &value, OS_NO_WAIT)) {
    test_fail("msgq full");
  }
  value = 0U;
  if (!msgq_recv(&test_msgq_, &value, OS_NO_WAIT) || (value != 11U)) {
    test_fail("msgq recv 11");
  }
  if (!msgq_recv(&test_msgq_, &value, OS_NO_WAIT) || (value != 22U)) {
    test_fail("msgq recv 22");
  }
  if (msgq_recv(&test_msgq_, &value, OS_NO_WAIT)) {
    test_fail("msgq empty");
  }

  worker_done_[0] = false;
  worker_cmd_[0] = WORKER_CMD_MSGQ_RECV;
  task_notify(&worker_task_1_);
  task_delay(2U);
  value = 33U;
  if (!msgq_send(&test_msgq_, &value, OS_NO_WAIT)) {
    test_fail("msgq wake recv");
  }
  wait_worker_done(0U);
  if (worker_value_[0] != 33U) {
    test_fail("msgq worker recv");
  }

  msgq_init(&test_msgq_, test_msgq_storage_, sizeof(uint32_t), 2U);
  value = 1U;
  (void)msgq_send(&test_msgq_, &value, OS_NO_WAIT);
  value = 2U;
  (void)msgq_send(&test_msgq_, &value, OS_NO_WAIT);
  worker_done_[0] = false;
  worker_value_[0] = 44U;
  worker_cmd_[0] = WORKER_CMD_MSGQ_SEND;
  task_notify(&worker_task_1_);
  task_delay(2U);
  value = 0U;
  if (!msgq_recv(&test_msgq_, &value, OS_NO_WAIT) || (value != 1U)) {
    test_fail("msgq free sender");
  }
  wait_worker_done(0U);
  if (!msgq_recv(&test_msgq_, &value, OS_NO_WAIT) || (value != 2U)) {
    test_fail("msgq fifo 2");
  }
  if (!msgq_recv(&test_msgq_, &value, OS_NO_WAIT) || (value != 44U)) {
    test_fail("msgq fifo 44");
  }

  msgq_init(&test_msgq_, test_msgq_storage_, sizeof(uint32_t), 2U);
  worker_done_[0] = false;
  worker_done_[1] = false;
  worker_cmd_[0] = WORKER_CMD_MSGQ_RECV;
  worker_cmd_[1] = WORKER_CMD_MSGQ_RECV;
  task_notify(&worker_task_1_);
  task_notify(&worker_task_2_);
  task_delay(2U);
  value = 55U;
  if (!msgq_send(&test_msgq_, &value, OS_NO_WAIT)) {
    test_fail("msgq multi send 55");
  }
  value = 66U;
  if (!msgq_send(&test_msgq_, &value, OS_NO_WAIT)) {
    test_fail("msgq multi send 66");
  }
  wait_worker_done(0U);
  wait_worker_done(1U);
  if ((worker_value_[0] != 55U) || (worker_value_[1] != 66U)) {
    test_fail("msgq multi recv");
  }

  msgq_init(&test_msgq_, test_msgq_storage_, sizeof(uint32_t), 2U);
  value = 1U;
  (void)msgq_send(&test_msgq_, &value, OS_NO_WAIT);
  value = 2U;
  (void)msgq_send(&test_msgq_, &value, OS_NO_WAIT);
  worker_done_[0] = false;
  worker_done_[1] = false;
  worker_value_[0] = 77U;
  worker_value_[1] = 88U;
  worker_cmd_[0] = WORKER_CMD_MSGQ_SEND;
  worker_cmd_[1] = WORKER_CMD_MSGQ_SEND;
  task_notify(&worker_task_1_);
  task_notify(&worker_task_2_);
  task_delay(2U);
  if (!msgq_recv(&test_msgq_, &value, OS_NO_WAIT) || (value != 1U)) {
    test_fail("msgq multi free 1");
  }
  if (!msgq_recv(&test_msgq_, &value, OS_NO_WAIT) || (value != 2U)) {
    test_fail("msgq multi free 2");
  }
  wait_worker_done(0U);
  wait_worker_done(1U);
  if (!msgq_recv(&test_msgq_, &value, OS_NO_WAIT) || (value != 77U)) {
    test_fail("msgq multi send 77");
  }
  if (!msgq_recv(&test_msgq_, &value, OS_NO_WAIT) || (value != 88U)) {
    test_fail("msgq multi send 88");
  }

  uart_write("[G070-PRIM] msgq ok\r\n");
}

static void primitive_test_pipe(void)
{
  uint8_t out[4];
  uint16_t n;

  pipe_init(&test_pipe_, test_pipe_storage_, sizeof(test_pipe_storage_));

  if (pipe_write(&test_pipe_, "abc", 3U, OS_NO_WAIT) != 3U) {
    test_fail("pipe write abc");
  }
  n = pipe_read(&test_pipe_, out, 2U, OS_NO_WAIT);
  if ((n != 2U) || (out[0] != 'a') || (out[1] != 'b')) {
    test_fail("pipe read ab");
  }
  if (pipe_write(&test_pipe_, "def", 3U, OS_NO_WAIT) != 3U) {
    test_fail("pipe wrap write");
  }
  n = pipe_read(&test_pipe_, out, 4U, OS_NO_WAIT);
  if ((n != 4U) || (out[0] != 'c') || (out[1] != 'd') ||
      (out[2] != 'e') || (out[3] != 'f')) {
    test_fail("pipe wrap read");
  }

  worker_done_[0] = false;
  worker_cmd_[0] = WORKER_CMD_PIPE_READ;
  task_notify(&worker_task_1_);
  task_delay(2U);
  if (pipe_write(&test_pipe_, "xy", 2U, OS_NO_WAIT) != 2U) {
    test_fail("pipe wake reader");
  }
  wait_worker_done(0U);
  if (worker_value_[0] != ((2UL << 16) | ('x' << 8) | 'y')) {
    test_fail("pipe worker read");
  }

  pipe_init(&test_pipe_, test_pipe_storage_, sizeof(test_pipe_storage_));
  if (pipe_write(&test_pipe_, "1234", 4U, OS_NO_WAIT) != 4U) {
    test_fail("pipe fill");
  }
  worker_done_[0] = false;
  worker_value_[0] = 'Z';
  worker_cmd_[0] = WORKER_CMD_PIPE_WRITE;
  task_notify(&worker_task_1_);
  task_delay(2U);
  n = pipe_read(&test_pipe_, out, 1U, OS_NO_WAIT);
  if ((n != 1U) || (out[0] != '1')) {
    test_fail("pipe free writer");
  }
  wait_worker_done(0U);
  n = pipe_read(&test_pipe_, out, 4U, OS_NO_WAIT);
  if ((n != 4U) || (out[0] != '2') || (out[1] != '3') ||
      (out[2] != '4') || (out[3] != 'Z')) {
    test_fail("pipe writer data");
  }

  uart_write("[G070-PRIM] pipe ok\r\n");
}

#if OS_TIMER_ENABLE
static void primitive_test_timer(void)
{
  bool notified;

  drain_task_notify();

  timer_init(&one_shot_timer_, one_shot_cb, NULL);
  timer_init(&periodic_timer_, periodic_cb, NULL);
  timer_init(&order_timer_a_, order_timer_cb, (void *)1UL);
  timer_init(&order_timer_b_, order_timer_cb, (void *)2UL);

  one_shot_count_ = 0U;
  timer_start(&one_shot_timer_, 10U, 0U);
  notified = task_notify_wait(100U);
  if (!notified) {
    test_fail("timer one-shot wait");
  }
  if (one_shot_count_ != 1U) {
    test_fail("timer one-shot count");
  }
  if (timer_is_running(&one_shot_timer_)) {
    test_fail("timer one-shot running");
  }

  periodic_count_ = 0U;
  timer_start(&periodic_timer_, 5U, 5U);
  while (periodic_count_ < 3U) {
    if (!task_notify_wait(100U)) {
      test_fail("timer periodic wait");
    }
  }
  timer_stop(&periodic_timer_);
  if (timer_is_running(&periodic_timer_)) {
    test_fail("timer stop");
  }

  timer_order_count_ = 0U;
  timer_start(&order_timer_a_, 20U, 0U);
  timer_start(&order_timer_b_, 10U, 0U);
  while (timer_order_count_ < 2U) {
    if (!task_notify_wait(100U)) {
      test_fail("timer order wait");
    }
  }
  if ((timer_order_[0] != 2U) || (timer_order_[1] != 1U)) {
    test_fail("timer order");
  }

  uart_write("[G070-PRIM] timer ok\r\n");
}
#endif

static void primitive_entry(void *arg)
{
  os_tick_t last_heartbeat;

  (void)arg;

  while (!test_finished_) {
    task_delay(1U);
  }

  uart_write("[G070-FIRST] PASS\r\n");
  uart_write("[G070-PRIM] start\r\n");
  primitive_test_scheduler_extra();
#ifdef MCU_TEST_FPU_REGS
  primitive_test_fpu_regs();
#endif
  primitive_test_task_return();
  primitive_test_delay_extra();
  primitive_test_eventset();
  primitive_test_sem();
  primitive_test_msgq();
  primitive_test_pipe();
#if OS_TIMER_ENABLE
  primitive_test_timer();
#endif
  primitive_test_stack_watermark();
  primitive_test_trace();
  primitive_passed_ = true;
  uart_write("[G070-PRIM] PASS\r\n");

  last_heartbeat = os_tick_get();
  for (;;) {
    if (os_tick_elapsed(os_tick_get(), last_heartbeat, 1000U)) {
      last_heartbeat = os_tick_get();
      HAL_GPIO_TogglePin(ST_LED1_GPIO_Port, ST_LED1_Pin);
      uart_write("[G070-FIRST] heartbeat\r\n");
    }

    task_delay(100U);
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
#ifdef MCU_TEST_LONG_OR_SCHED_STRESS
  uart_write("\r\n" STRESS_LOG_PREFIX " boot\r\n");
#elif defined(MCU_TEST_BUS_STRESS)
  uart_write("\r\n" BUS_STRESS_LOG_PREFIX " boot\r\n");
#else
  uart_write("\r\n[G070-FIRST] boot\r\n");
#endif

#ifdef MCU_TEST_ASSERT_PROBE
  uart_write("[G070-ASSERT] start\r\n");
  HAL_Delay(1000U);
  uart_write("[G070-ASSERT] trigger\r\n");
  scheduler_add(NULL);
  test_fail("assert probe returned");
#endif

  scheduler_init();

#ifdef MCU_TEST_BUS_STRESS
  bus_init();

  task_init(&bus_control_task_,
            "bus_control",
            bus_stress_control_entry,
            NULL,
            bus_control_stack_,
            sizeof(bus_control_stack_),
            1U);
  task_init(&bus_pub_task_,
            "bus_pub",
            bus_stress_pub_entry,
            NULL,
            bus_pub_stack_,
            sizeof(bus_pub_stack_),
            3U);
  task_init(&bus_sub_a_task_,
            "bus_sub_a",
            bus_stress_sub_entry,
            (void *)0UL,
            bus_sub_a_stack_,
            sizeof(bus_sub_a_stack_),
            2U);
  task_init(&bus_sub_b_task_,
            "bus_sub_b",
            bus_stress_sub_entry,
            (void *)1UL,
            bus_sub_b_stack_,
            sizeof(bus_sub_b_stack_),
            2U);

  scheduler_add(&bus_control_task_);
  scheduler_add(&bus_pub_task_);
  scheduler_add(&bus_sub_a_task_);
  scheduler_add(&bus_sub_b_task_);

  test_tick_enabled_ = true;
  uart_write(BUS_STRESS_LOG_PREFIX " scheduler start\r\n");
  scheduler_start();
#elif defined(MCU_TEST_LONG_OR_SCHED_STRESS)
  eventset_init(&long_eventset_);
  sem_init(&long_sem_, 0U, 65535U);
  msgq_init(&long_msgq_,
            long_msgq_storage_,
            sizeof(uint32_t),
            LONG_MSGQ_CAPACITY);
  pipe_init(&long_pipe_, long_pipe_storage_, sizeof(long_pipe_storage_));

  task_init(&long_control_task_,
            "long_control",
            long_control_entry,
            NULL,
            long_control_stack_,
            sizeof(long_control_stack_),
            4U);
  task_init(&long_notify_task_,
            "long_notify",
            long_notify_entry,
            NULL,
            long_notify_stack_,
            sizeof(long_notify_stack_),
            2U);
  task_init(&long_event_task_,
            "long_event",
            long_event_entry,
            NULL,
            long_event_stack_,
            sizeof(long_event_stack_),
            2U);
  task_init(&long_sem_task_,
            "long_sem",
            long_sem_entry,
            NULL,
            long_sem_stack_,
            sizeof(long_sem_stack_),
            2U);
  task_init(&long_msgq_tx_task_,
            "long_msgq_tx",
            long_msgq_tx_entry,
            NULL,
            long_msgq_tx_stack_,
            sizeof(long_msgq_tx_stack_),
            3U);
  task_init(&long_msgq_rx_task_,
            "long_msgq_rx",
            long_msgq_rx_entry,
            NULL,
            long_msgq_rx_stack_,
            sizeof(long_msgq_rx_stack_),
            3U);
  task_init(&long_pipe_tx_task_,
            "long_pipe_tx",
            long_pipe_tx_entry,
            NULL,
            long_pipe_tx_stack_,
            sizeof(long_pipe_tx_stack_),
            3U);
  task_init(&long_pipe_rx_task_,
            "long_pipe_rx",
            long_pipe_rx_entry,
            NULL,
            long_pipe_rx_stack_,
            sizeof(long_pipe_rx_stack_),
            3U);
  task_init(&long_delay_task_,
            "long_delay",
            long_delay_entry,
            NULL,
            long_delay_stack_,
            sizeof(long_delay_stack_),
            3U);
  task_init(&long_yield_task_a_,
            "long_yield_a",
            long_yield_a_entry,
            NULL,
            long_yield_stack_a_,
            sizeof(long_yield_stack_a_),
            6U);
  task_init(&long_yield_task_b_,
            "long_yield_b",
            long_yield_b_entry,
            NULL,
            long_yield_stack_b_,
            sizeof(long_yield_stack_b_),
            6U);
  task_init(&long_yield_task_c_,
            "long_yield_c",
            long_yield_c_entry,
            NULL,
            long_yield_stack_c_,
            sizeof(long_yield_stack_c_),
            6U);
#if OS_TIMER_ENABLE
  task_init(&long_timer_task_,
            "long_timer",
            long_timer_entry,
            NULL,
            long_timer_stack_,
            sizeof(long_timer_stack_),
            2U);
#endif
#ifdef MCU_TEST_FPU_STRESS
  task_init(&fpu_stress_task_1_,
            "fpu_stress1",
            fpu_stress_entry,
            (void *)0UL,
            fpu_stress_stack_1_,
            sizeof(fpu_stress_stack_1_),
            2U);
  task_init(&fpu_stress_task_2_,
            "fpu_stress2",
            fpu_stress_entry,
            (void *)1UL,
            fpu_stress_stack_2_,
            sizeof(fpu_stress_stack_2_),
            2U);
#endif

  scheduler_add(&long_control_task_);
  scheduler_add(&long_notify_task_);
  scheduler_add(&long_event_task_);
  scheduler_add(&long_sem_task_);
  scheduler_add(&long_msgq_tx_task_);
  scheduler_add(&long_msgq_rx_task_);
  scheduler_add(&long_pipe_tx_task_);
  scheduler_add(&long_pipe_rx_task_);
  scheduler_add(&long_delay_task_);
  scheduler_add(&long_yield_task_a_);
  scheduler_add(&long_yield_task_b_);
  scheduler_add(&long_yield_task_c_);
#if OS_TIMER_ENABLE
  scheduler_add(&long_timer_task_);
#endif
#ifdef MCU_TEST_FPU_STRESS
  scheduler_add(&fpu_stress_task_1_);
  scheduler_add(&fpu_stress_task_2_);
#endif

  test_tick_enabled_ = true;
  uart_write(STRESS_LOG_PREFIX " scheduler start\r\n");
  scheduler_start();
#else
  task_init(&notify_task_,
            "notify",
            notify_entry,
            (void *)0x12345678UL,
            notify_stack_,
            sizeof(notify_stack_),
            1U);
  task_init(&delay_task_,
            "delay",
            delay_entry,
            NULL,
            delay_stack_,
            sizeof(delay_stack_),
            2U);
  task_init(&yield_task_a_,
            "yield_a",
            yield_a_entry,
            NULL,
            yield_stack_a_,
            sizeof(yield_stack_a_),
            3U);
  task_init(&yield_task_b_,
            "yield_b",
            yield_b_entry,
            NULL,
            yield_stack_b_,
            sizeof(yield_stack_b_),
            3U);
  task_init(&yield_task_c_,
            "yield_c",
            yield_c_entry,
            NULL,
            yield_stack_c_,
            sizeof(yield_stack_c_),
            3U);
  task_init(&worker_task_1_,
            "worker1",
            worker_entry,
            (void *)0UL,
            worker_stack_1_,
            sizeof(worker_stack_1_),
            2U);
  task_init(&worker_task_2_,
            "worker2",
            worker_entry,
            (void *)1UL,
            worker_stack_2_,
            sizeof(worker_stack_2_),
            2U);
  task_init(&primitive_task_,
            "primitive",
            primitive_entry,
            NULL,
            primitive_stack_,
            sizeof(primitive_stack_),
            4U);
  task_init(&peer_task_,
            "peer",
            peer_entry,
            NULL,
            peer_stack_,
            sizeof(peer_stack_),
            4U);
  task_init(&return_task_,
            "return",
            return_entry,
            NULL,
            return_stack_,
            sizeof(return_stack_),
            5U);
#ifdef MCU_TEST_FPU_REGS
  task_init(&fpu_task_1_,
            "fpu1",
            fpu_worker_entry,
            (void *)0UL,
            fpu_stack_1_,
            sizeof(fpu_stack_1_),
            2U);
  task_init(&fpu_task_2_,
            "fpu2",
            fpu_worker_entry,
            (void *)1UL,
            fpu_stack_2_,
            sizeof(fpu_stack_2_),
            2U);
  task_init(&fpu_preempt_low_task_,
            "fpu_low",
            fpu_preempt_low_entry,
            NULL,
            fpu_preempt_low_stack_,
            sizeof(fpu_preempt_low_stack_),
            3U);
  task_init(&fpu_preempt_high_task_,
            "fpu_high",
            fpu_preempt_high_entry,
            NULL,
            fpu_preempt_high_stack_,
            sizeof(fpu_preempt_high_stack_),
            1U);
  task_init(&fpu_isr_task_,
            "fpu_isr",
            fpu_isr_entry,
            NULL,
            fpu_isr_stack_,
            sizeof(fpu_isr_stack_),
            2U);
#endif

  scheduler_add(&notify_task_);
  scheduler_add(&delay_task_);
  scheduler_add(&worker_task_1_);
  scheduler_add(&worker_task_2_);
  scheduler_add(&yield_task_a_);
  scheduler_add(&yield_task_b_);
  scheduler_add(&yield_task_c_);
  scheduler_add(&peer_task_);
  scheduler_add(&primitive_task_);
  scheduler_add(&return_task_);
#ifdef MCU_TEST_FPU_REGS
  scheduler_add(&fpu_task_1_);
  scheduler_add(&fpu_task_2_);
  scheduler_add(&fpu_preempt_low_task_);
  scheduler_add(&fpu_preempt_high_task_);
  scheduler_add(&fpu_isr_task_);
#endif

  test_tick_enabled_ = true;
  uart_write("[G070-FIRST] scheduler start\r\n");
  scheduler_start();
#endif

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, ST_LED1_Pin|ST_LED2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : btn1_Pin */
  GPIO_InitStruct.Pin = btn1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(btn1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ST_LED1_Pin ST_LED2_Pin */
  GPIO_InitStruct.Pin = ST_LED1_Pin|ST_LED2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
