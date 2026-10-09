#ifndef KERNEL_HOST_STUB_H
#define KERNEL_HOST_STUB_H

#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <micaos/task.h>

#ifdef __cplusplus
extern "C" {
#endif

void kernel_host_reset(void);
void kernel_host_set_current(task_t *task);
void kernel_host_set_in_isr(bool in_isr);

uint32_t kernel_host_ready_count(void);
uint32_t kernel_host_block_count(void);
uint32_t kernel_host_yield_count(void);
uint32_t kernel_host_switch_request_count(void);
uint32_t kernel_host_irq_lock_depth(void);

#ifdef __cplusplus
}
#endif

#endif /* KERNEL_HOST_STUB_H */
