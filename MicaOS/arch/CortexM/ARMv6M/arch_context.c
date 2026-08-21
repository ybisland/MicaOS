#include "config.h"

#if MICAOS_ARCH_PORT == MICAOS_ARCH_PORT_ARMV6M

#include "arch/arch_context.h"
#include "kernel/scheduler_internal.h"
#include "common/assert.h"

/*
 * ARMv6-M Cortex-M context switch implementation.
 *
 * Thread tasks use PSP; interrupt handlers use MSP. The first task is started
 * through SVC, then cooperative context switches are performed by the
 * lowest-priority PendSV exception so interrupt handlers can finish before
 * task switching.
 */

#define ARCH_XPSR_T_BIT              0x01000000UL /* default Thumb state bit */
#define ARCH_SCB_ICSR                (*(volatile uint32_t *)0xE000ED04UL)
#define ARCH_SCB_SHPR3               (*(volatile uint32_t *)0xE000ED20UL)
#define ARCH_SCB_ICSR_PENDSVSET      (1UL << 28)
#define ARCH_SCB_SHPR3_PENDSV_MASK   (0xFFUL << 16)

#define ARCH_STACK_ALIGN             8U
#define ARCH_CONTEXT_FRAME_SLOTS     16U   /* the number of 32-bit slots(registers to save) in the context frame */
#define ARCH_CONTEXT_FRAME_SIZE      (ARCH_CONTEXT_FRAME_SLOTS * sizeof(uint32_t))

static uint32_t *volatile arch_start_sp_;

/*
 * Saved exception context. The stack grows toward lower addresses and
 * task->sp points at offset 0:
 *
 *     Higher address
 *     +64  Previous PSP: task stack resumes here
 *     +60  xPSR           -
 *     +56  PC             |
 *     +52  LR             |
 *     +48  R12            |
 *     +44  R3             |
 *     +40  R2             |
 *     +36  R1             |
 *     +32  R0             - <- hardware exception frame
 *     +28  R11            -
 *     +24  R10            |
 *     +20  R9             |
 *     +16  R8             |
 *     +12  R7             |
 *     +8   R6             |
 *     +4   R5             |
 *     +0   R4             - <- task->sp, software-saved frame
 *     Lower address
 *
 * Exception entry and return automatically save and restore R0-R3, R12, LR,
 * PC, and xPSR. SVC/PendSV software saves and restores R4-R11.
 */

uint32_t *arch_context_init(void *stack,
                            size_t stack_size,
                            arch_task_entry_t entry,
                            void *arg,
                            arch_task_exit_t exit)
{
    /*
     * Build the initial saved context by fully unrolling the stores.
     *
     * The layout must match the PendSV save/restore code exactly:
     *   sp[0..7]   : software-saved R4-R11
     *   sp[8..15]  : hardware exception frame
     *
     * On exception return(when cpu switches to this context), hardware restores:
     *   R0   = arg
     *   LR   = exit
     *   PC   = entry
     *   xPSR = Thumb state bit set
     * Then execution starts at entry(arg). If entry returns, the CPU branches to exit().
     *
     */
    uint32_t *stack_top;
    uint32_t *sp;

    static_assert(sizeof(uint32_t) == sizeof(uintptr_t), "ARMv6-M requires 32-bit pointers");
    OS_ASSERT((stack != NULL) && (entry != NULL) && (exit != NULL));
    OS_ASSERT((((uintptr_t)stack & (ARCH_STACK_ALIGN - 1U)) == 0U) &&  /* stack base must be 8-byte aligned. */
           ((stack_size & (ARCH_STACK_ALIGN - 1U)) == 0U) &&           /* stack size must preserve 8-byte alignment. */
           (stack_size <= (size_t)(UINTPTR_MAX - (uintptr_t)stack)) && /* stack top address must not overflow. */
           (stack_size >= ARCH_CONTEXT_FRAME_SIZE));                   /* initial context frame must fit in the stack. */

    stack_top = (uint32_t *)((uint8_t *)stack + stack_size);
    sp = stack_top - ARCH_CONTEXT_FRAME_SLOTS;

    /* Software-saved frame: R4-R11. */
    sp[0] = 0U;
    sp[1] = 0U;
    sp[2] = 0U;
    sp[3] = 0U;
    sp[4] = 0U;
    sp[5] = 0U;
    sp[6] = 0U;
    sp[7] = 0U;

    /* Hardware exception frame: R0-R3, R12, LR, PC, xPSR. */
    sp[8] = (uint32_t)(uintptr_t)arg;
    sp[9] = 0U;
    sp[10] = 0U;
    sp[11] = 0U;
    sp[12] = 0U;
    sp[13] = (uint32_t)(uintptr_t)exit;
    sp[14] = (uint32_t)(uintptr_t)entry;
    sp[15] = ARCH_XPSR_T_BIT;

    return sp;
}

__NO_RETURN void arch_context_start(uint32_t *sp)
{
    /*
     * Start the first task.
     *   1. Store the first task's initial stack pointer for SVC to load into PSP.
     *   2. Set PendSV to the lowest priority for later context switches.
     *   3. Trigger SVC to restore the first task and return to Thread mode.
     */
    OS_ASSERT(sp != NULL);

    arch_start_sp_ = sp;

    // Set PendSV to the lowest priority
    ARCH_SCB_SHPR3 = (ARCH_SCB_SHPR3 & ~ARCH_SCB_SHPR3_PENDSV_MASK) |
                     ARCH_SCB_SHPR3_PENDSV_MASK;

    __ASM volatile ("svc 0" ::: "memory");

    for (;;) {
    }
}

__NO_INLINE __NAKED void SVC_Handler(void)
{
    __ASM volatile (
        ".syntax unified\n"
        /* Load the first task's stack pointer from @arch_start_sp_ . */
        "ldr r0, =arch_start_sp_\n"
        "ldr r0, [r0]\n"

        /* Restore R8-R11. */
        "adds r0, #16\n"
        "ldmia r0!, {r4-r7}\n"
        "mov r8, r4\n"
        "mov r9, r5\n"
        "mov r10, r6\n"
        "mov r11, r7\n"

        /* Restore R4-R7 and point PSP at the hardware frame. */
        "subs r0, #32\n"
        "ldmia r0!, {r4-r7}\n"
        "adds r0, #16\n"
        "msr psp, r0\n"

        /* Exception-return to privileged Thread mode using PSP. */
        "movs r0, #2\n"
        "msr control, r0\n"
        "isb\n"
        "ldr r0, =0xFFFFFFFD\n"
        "mov lr, r0\n"
        "bx lr\n");
}

/* Request a context switch */
void arch_context_switch_request(void)
{
    // Set PendSV to pending to request a context switch.
    ARCH_SCB_ICSR = ARCH_SCB_ICSR_PENDSVSET;
    __ASM volatile ("dsb" ::: "memory");
    __ASM volatile ("isb" ::: "memory");
}

void arch_wait_for_interrupt(void)
{
    __ASM volatile ("wfi" ::: "memory");
}

__NO_INLINE __NAKED void PendSV_Handler(void)
{
    __ASM volatile (
        ".syntax unified\n"
        /*
         * Hardware has already stacked R0-R3, R12, LR, PC, and xPSR on the
         * current PSP. Add the software-saved R4-R11 frame below it.
         */
        "mrs r0, psp\n"
        "subs r0, #32\n"
        "stmia r0!, {r4-r7}\n"
        "mov r4, r8\n"
        "mov r5, r9\n"
        "mov r6, r10\n"
        "mov r7, r11\n"
        "stmia r0!, {r4-r7}\n"
        "subs r0, #32\n"

        /* Select the prepared next task and obtain its software-frame SP. */
        "push {r3, lr}\n"
        "bl arch_context_switch_callback\n"
        "pop {r2, r3}\n"
        "mov lr, r3\n"

        /* Restore R8-R11 through the low registers. */
        "adds r0, #16\n"
        "ldmia r0!, {r4-r7}\n"
        "mov r8, r4\n"
        "mov r9, r5\n"
        "mov r10, r6\n"
        "mov r11, r7\n"

        /* Restore R4-R7 and point PSP at the hardware frame. */
        "subs r0, #32\n"
        "ldmia r0!, {r4-r7}\n"
        "adds r0, #16\n"
        "msr psp, r0\n"
        "bx lr\n");
}

uint32_t arch_irq_lock(void)
{
    uint32_t key;

    __ASM volatile (
        "mrs %0, primask\n"
        "cpsid i"
        : "=r" (key)
        :
        : "memory");

    return key;
}

void arch_irq_unlock(uint32_t key)
{
    __ASM volatile (
        "msr primask, %0"
        :
        : "r" (key)
        : "memory");
}

bool arch_in_isr(void)
{
    uint32_t ipsr;

    __ASM volatile (
        "mrs %0, ipsr"
        : "=r" (ipsr)
        :
        : "memory");

    return ipsr != 0U;
}

#endif /* MICAOS_ARCH_PORT == MICAOS_ARCH_PORT_ARMV6M */
