#ifndef KERNEL_TIME_H
#define KERNEL_TIME_H

#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * OS Tick
 *
 * The OS tick is a uint32_t counter advanced by os_tick_advance(), usually
 * from SysTick or another periodic timer ISR.
 *
 * Tick values wrap naturally. When comparing deadlines, use
 * os_tick_after_eq() or os_tick_elapsed() instead of plain < or >. The real
 * distance between compared times must be less than half of the counter range,
 * which is OS_TICK_MAX_DELAY.
 */

typedef uint32_t os_tick_t;

#define OS_TICK_MAX_DELAY ((os_tick_t)(INT32_MAX - 1))

/* Special timeout value for APIs that should check once without blocking. */
#define OS_NO_WAIT ((os_tick_t)0)

/* Special timeout value for APIs that support waiting without a deadline. */
#define OS_WAIT_FOREVER ((os_tick_t)UINT32_MAX)

/* Return the current OS tick count. */
os_tick_t os_tick_get(void);

/*
 * Return true when now has reached or passed deadline.
 *
 * The real distance between now and deadline must be less than half of the
 * os_tick_t range. This is the same wraparound rule used by task_delay().
 */
static inline bool os_tick_after_eq(os_tick_t now, os_tick_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

/*
 * Return true when duration ticks have elapsed since start.
 *
 * duration must be no larger than OS_TICK_MAX_DELAY.
 */
static inline bool os_tick_elapsed(os_tick_t now,
                                   os_tick_t start,
                                   os_tick_t duration)
{
    return (os_tick_t)(now - start) >= duration;
}

/*
 * Advance the OS tick by one.
 *
 * This function is intended to be called by the system tick interrupt handler.
 * It wakes tasks whose task_delay() timeout has expired. When OS_TIMER_ENABLE
 * is 1, it also runs expired soft timer callbacks.
 */
void os_tick_advance(void);

#ifdef __cplusplus
}
#endif

#endif /* KERNEL_TIME_H */
