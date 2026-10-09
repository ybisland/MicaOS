#include <micaos_config.h>

#if MICAOS_ARCH_PORT == MICAOS_ARCH_PORT_ARMV7M_FPU

#include "internal/arch/arch_context.h"
#include "internal/kernel/scheduler_internal.h"
#include <micaos/common/assert.h>

/*
 * ARMv7-M Cortex-M context switch implementation with hardware FPU support.
 *
 * Thread tasks use PSP; interrupt handlers use MSP. The first task is started
 * through SVC, then cooperative context switches are performed by the
 * lowest-priority PendSV exception so interrupt handlers can finish before
 * task switching.
 *
 * This port is intended for Cortex-M4F/M7 projects compiled with hardware FPU
 * instructions enabled. It saves the integer context for every task and saves
 * S16-S31 only when EXC_RETURN says the interrupted task owns an extended FP
 * hardware frame. S0-S15 and FPSCR are saved and restored by exception entry
 * and return.
 */

#if !defined(__ARM_FP) && !defined(__VFP_FP__)
# error "ARMv7M_FPU arch_context requires hardware FPU compiler support"
#endif

#define ARCH_XPSR_T_BIT              0x01000000UL /* default Thumb state bit */
#define ARCH_EXC_RETURN_THREAD_PSP   0xFFFFFFFDUL

#define ARCH_SCB_ICSR                (*(volatile uint32_t *)0xE000ED04UL)
#define ARCH_SCB_SHPR3               (*(volatile uint32_t *)0xE000ED20UL)
#define ARCH_SCB_CPACR               (*(volatile uint32_t *)0xE000ED88UL)
#define ARCH_FPU_FPCCR               (*(volatile uint32_t *)0xE000EF34UL)
#define ARCH_SCB_ICSR_PENDSVSET      (1UL << 28)
#define ARCH_SCB_SHPR3_PENDSV_MASK   (0xFFUL << 16)
#define ARCH_SCB_CPACR_FPU_ENABLE    (0xFUL << 20)
#define ARCH_FPU_FPCCR_ASPEN         (1UL << 31)
#define ARCH_FPU_FPCCR_LSPEN         (1UL << 30)

#define ARCH_STACK_ALIGN             8U
#define ARCH_SOFTWARE_FRAME_SLOTS    9U    /* R4-R11 plus EXC_RETURN */
#define ARCH_HARDWARE_FRAME_SLOTS    8U    /* R0-R3, R12, LR, PC, xPSR */
#define ARCH_CONTEXT_FRAME_SLOTS     (ARCH_SOFTWARE_FRAME_SLOTS + ARCH_HARDWARE_FRAME_SLOTS)
#define ARCH_CONTEXT_FRAME_SIZE      (ARCH_CONTEXT_FRAME_SLOTS * sizeof(uint32_t))

static uint32_t *volatile arch_start_sp_;

/*
 * Basic saved context. The stack grows toward lower addresses and task->sp
 * points at offset 0:
 *
 *     Higher address
 *     +68  Previous PSP: task stack resumes here
 *     +64  xPSR           -
 *     +60  PC             |
 *     +56  LR             |
 *     +52  R12            |
 *     +48  R3             |
 *     +44  R2             |
 *     +40  R1             |
 *     +36  R0             - <- hardware exception frame
 *     +32  EXC_RETURN     - <- tells hardware whether an FP frame exists
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
 * If the task has active FP state, S16-S31 are placed between EXC_RETURN and
 * the hardware frame. Hardware places S0-S15 and FPSCR in the extended
 * exception frame and uses EXC_RETURN bit[4] to describe that layout.
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
     * A new task starts without an extended FP frame, so EXC_RETURN bit[4] is
     * set in ARCH_EXC_RETURN_THREAD_PSP. If the task later uses the FPU, the
     * hardware exception mechanism and PendSV save path extend its saved frame
     * on demand.
     */
    uint32_t *stack_top;
    uint32_t *sp;

    static_assert(sizeof(uint32_t) == sizeof(uintptr_t), "ARMv7-M FPU port requires 32-bit pointers");
    OS_ASSERT((stack != NULL) && (entry != NULL) && (exit != NULL));
    OS_ASSERT((((uintptr_t)stack & (ARCH_STACK_ALIGN - 1U)) == 0U) &&  /* stack base must be 8-byte aligned. */
           ((stack_size & (ARCH_STACK_ALIGN - 1U)) == 0U) &&           /* stack size must preserve 8-byte alignment. */
           (stack_size <= (size_t)(UINTPTR_MAX - (uintptr_t)stack)) && /* stack top address must not overflow. */
           (stack_size >= ARCH_CONTEXT_FRAME_SIZE));                   /* initial context frame must fit in the stack. */

    stack_top = (uint32_t *)((uint8_t *)stack + stack_size);
    sp = stack_top - ARCH_CONTEXT_FRAME_SLOTS;

    /* Software-saved frame: R4-R11, EXC_RETURN. */
    sp[0] = 0U;
    sp[1] = 0U;
    sp[2] = 0U;
    sp[3] = 0U;
    sp[4] = 0U;
    sp[5] = 0U;
    sp[6] = 0U;
    sp[7] = 0U;
    sp[8] = ARCH_EXC_RETURN_THREAD_PSP;

    /* Hardware exception frame: R0-R3, R12, LR, PC, xPSR. */
    sp[9] = (uint32_t)(uintptr_t)arg;
    sp[10] = 0U;
    sp[11] = 0U;
    sp[12] = 0U;
    sp[13] = 0U;
    sp[14] = (uint32_t)(uintptr_t)exit;
    sp[15] = (uint32_t)(uintptr_t)entry;
    sp[16] = ARCH_XPSR_T_BIT;

    return sp;
}

__NO_RETURN void arch_context_start(uint32_t *sp)
{
    /*
     * Enable CP10/CP11 before any task can execute FP instructions. SVC then
     * restores the first task and exception-returns to Thread mode using PSP.
     */
    OS_ASSERT(sp != NULL);

    arch_start_sp_ = sp;

    ARCH_SCB_CPACR |= ARCH_SCB_CPACR_FPU_ENABLE;
    __ASM volatile ("dsb" ::: "memory");
    __ASM volatile ("isb" ::: "memory");

    ARCH_FPU_FPCCR |= ARCH_FPU_FPCCR_ASPEN | ARCH_FPU_FPCCR_LSPEN;
    __ASM volatile ("dsb" ::: "memory");
    __ASM volatile ("isb" ::: "memory");

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
        /* Load the first task's saved SP. */
        "ldr r0, =arch_start_sp_\n"
        "ldr r0, [r0]\n"

        /* Restore R4-R11, load EXC_RETURN, and point PSP at the hardware frame. */
        "ldmia r0!, {r4-r11, lr}\n"
        "msr psp, r0\n"

        /* Exception-return to privileged Thread mode using PSP. */
        "movs r0, #2\n"
        "msr control, r0\n"
        "isb\n"
        "bx lr\n");
}

void arch_context_switch_request(void)
{
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
         * Hardware has already stacked the basic exception frame on PSP. If
         * EXC_RETURN bit[4] is clear, hardware also owns an extended FP frame
         * containing S0-S15 and FPSCR.
         */
        "mrs r0, psp\n"
        "tst lr, #0x10\n"
        "it eq\n"
        "vstmdbeq r0!, {s16-s31}\n"
        "stmdb r0!, {r4-r11, lr}\n"

        /*
         * Keep MSP 8-byte aligned while calling C. The callback receives the
         * saved SP in R0 and returns the next task's saved SP in R0.
         */
        "push {r3, lr}\n"
        "bl arch_context_switch_callback\n"
        "pop {r2, r3}\n"

        /*
         * Restore the next task. EXC_RETURN bit[4] decides whether S16-S31
         * were saved between the software frame and the hardware frame.
         */
        "ldmia r0!, {r4-r11, lr}\n"
        "tst lr, #0x10\n"
        "it eq\n"
        "vldmiaeq r0!, {s16-s31}\n"
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

#endif /* MICAOS_ARCH_PORT == MICAOS_ARCH_PORT_ARMV7M_FPU */
