#ifndef ARCH_CONTEXT_H
#define ARCH_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include "common/compiler.h"

/*
 * Architecture context switch contract.
 *
 * This header is the boundary between the portable Kernel layer and the
 * architecture layer. task.c and scheduler.c depend only on the semantics
 * described here; they do not know how registers are saved, which exception
 * or trap is used, or how the initial stack frame is arranged.
 *
 * Porting contract:
 *   - arch_context_init() must build a saved context that can be restored by
 *     arch_context_start() or by a later context switch.
 *   - When that context is first restored, execution must start at entry(arg).
 *   - If entry(arg) returns, execution must continue at exit(); exit() must
 *     not return.
 *   - A context switch implementation must save the current task context,
 *     pass its saved stack pointer to arch_context_switch_callback(), then
 *     restore the stack pointer returned by that callback.
 *   - arch_context_switch_request() requests a deferred cooperative switch.
 *     The switch does not have to happen before the function returns.
 *   - arch_in_isr() must report whether the CPU is currently running in an
 *     interrupt, exception, or trap context.
 */

/*
 * Task entry function type.
 *
 * The architecture layer places this function address into the initial
 * context frame. When the task is first restored, execution starts at
 * entry(arg).
 */
typedef void (*arch_task_entry_t)(void *arg);

/*
 * Task exit function type.
 *
 * The architecture layer places this function address into the initial
 * context frame as the task entry return target. If entry(arg) returns, the
 * CPU branches to exit(). It is not a hook that runs before every context
 * switch.
 */
typedef void (*arch_task_exit_t)(void);

/*
 * Build an initial task context and return its saved SP.
 *
 * Preconditions:
 *   - stack points to the first byte (low address) of caller-owned stack
 *     storage.
 *   - stack is aligned as required by the target architecture ABI. Cortex-M
 *     ports require 8-byte alignment.
 *   - stack_size is in bytes and preserves the required stack alignment,
 *     and must be 8-byte aligned for Cortex-M ports.
 *   - stack + stack_size is the initial stack top address.
 *   - stack_size is large enough for the architecture context frame plus the
 *     task's runtime stack usage.
 *   - entry is the task entry function and must not be NULL.
 *   - arg is passed to entry as its first argument and may be NULL.
 *   - exit is used as the task entry return address, must not be NULL, and
 *     must not return.
 *
 * The implementation does not adjust invalid stack alignment. The caller must
 * provide a correctly aligned stack.
 *
 * The returned pointer is an architecture-private saved SP. The Kernel may
 * store it in task_t and pass it back to this interface, but must not interpret
 * the frame layout behind it.
 */
uint32_t *arch_context_init(void *stack,
                            size_t stack_size,
                            arch_task_entry_t entry,
                            void *arg,
                            arch_task_exit_t exit);

/*
 * Start the first task from a saved SP returned by arch_context_init().
 *
 * This function does not return.
 */
__NO_RETURN void arch_context_start(uint32_t *sp);

/*
 * Request a deferred cooperative context switch.
 *
 * The actual switch may happen later, for example after interrupts are
 * re-enabled.
 */
void arch_context_switch_request(void);

/* Wait until an interrupt or architecture event becomes pending. */
void arch_wait_for_interrupt(void);

/*
 * Disable maskable interrupts and return the previous interrupt state.
 *
 * The returned key must later be passed to arch_irq_unlock().
 */
uint32_t arch_irq_lock(void);

/*
 * Restore a previous interrupt state returned by arch_irq_lock().
 *
 * This restores the saved state; it is not simply "enable interrupts".
 */
void arch_irq_unlock(uint32_t key);

/* Return true when the CPU is currently running in interrupt/exception context. */
bool arch_in_isr(void);

#ifdef __cplusplus
}
#endif

#endif /* ARCH_CONTEXT_H */
