#include "tests/host/kernel_host_stub.h"

#include <stddef.h>

#include "arch/arch_context.h"
#include "kernel/scheduler_internal.h"

static task_t *kernel_host_current_;
static bool kernel_host_in_isr_;
static uint32_t kernel_host_ready_count_;
static uint32_t kernel_host_block_count_;
static uint32_t kernel_host_yield_count_;
static uint32_t kernel_host_switch_request_count_;
static uint32_t kernel_host_irq_lock_depth_;

void kernel_host_reset(void)
{
    kernel_host_current_ = NULL;
    kernel_host_in_isr_ = false;
    kernel_host_ready_count_ = 0U;
    kernel_host_block_count_ = 0U;
    kernel_host_yield_count_ = 0U;
    kernel_host_switch_request_count_ = 0U;
    kernel_host_irq_lock_depth_ = 0U;
}

void kernel_host_set_current(task_t *task)
{
    kernel_host_current_ = task;
}

void kernel_host_set_in_isr(bool in_isr)
{
    kernel_host_in_isr_ = in_isr;
}

uint32_t kernel_host_ready_count(void)
{
    return kernel_host_ready_count_;
}

uint32_t kernel_host_block_count(void)
{
    return kernel_host_block_count_;
}

uint32_t kernel_host_yield_count(void)
{
    return kernel_host_yield_count_;
}

uint32_t kernel_host_switch_request_count(void)
{
    return kernel_host_switch_request_count_;
}

uint32_t kernel_host_irq_lock_depth(void)
{
    return kernel_host_irq_lock_depth_;
}

uint32_t *arch_context_init(void *stack,
                            size_t stack_size,
                            arch_task_entry_t entry,
                            void *arg,
                            arch_task_exit_t exit)
{
    (void)entry;
    (void)arg;
    (void)exit;

    return (uint32_t *)((uint8_t *)stack + stack_size);
}

__NO_RETURN void arch_context_start(uint32_t *sp)
{
    (void)sp;

    for (;;) {
    }
}

void arch_context_switch_request(void)
{
    kernel_host_switch_request_count_++;
}

void arch_wait_for_interrupt(void)
{
}

uint32_t arch_irq_lock(void)
{
    kernel_host_irq_lock_depth_++;
    return 0U;
}

void arch_irq_unlock(uint32_t key)
{
    (void)key;

    if (kernel_host_irq_lock_depth_ != 0U) {
        kernel_host_irq_lock_depth_--;
    }
}

bool arch_in_isr(void)
{
    return kernel_host_in_isr_;
}

task_t *scheduler_current(void)
{
    return kernel_host_current_;
}

void scheduler_yield(void)
{
    kernel_host_yield_count_++;
}

__NO_RETURN void scheduler_exit_current(void)
{
    if (kernel_host_current_ != NULL) {
        kernel_host_current_->state = TASK_STATE_TERMINATED;
    }

    for (;;) {
    }
}

void scheduler_make_ready(task_t *task)
{
    scheduler_make_ready_locked(task);
}

void scheduler_make_ready_locked(task_t *task)
{
    task->state = TASK_STATE_READY;
    kernel_host_ready_count_++;
}

void scheduler_block_current(void)
{
    scheduler_block_current_locked();
}

void scheduler_block_current_locked(void)
{
    if (kernel_host_current_ != NULL) {
        kernel_host_current_->state = TASK_STATE_BLOCKED;
    }
    kernel_host_block_count_++;
}

uint32_t *arch_context_switch_callback(uint32_t *saved_sp)
{
    return saved_sp;
}
