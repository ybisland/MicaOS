#ifndef TIMER_H
#define TIMER_H

#include "kernel_config.h"
#include "time.h"

#if OS_TIMER_ENABLE

#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include "data_structure/dlist.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Soft Timer
 *
 * Lightweight software timer driven by os_tick_advance(). Storage is provided
 * by the caller; this module does not allocate memory.
 *
 * This module is optional. Define OS_TIMER_ENABLE to 1 from the build system
 * so all kernel source files see the same setting.
 *
 * Usage:
 *   1. Allocate one soft_timer_t for each timer.
 *   2. Initialize it with timer_init().
 *   3. Start it with timer_start() after scheduler_init().
 *   4. Stop it with timer_stop() when needed.
 *
 * timer_start(timer, delay, 0) starts a one-shot timer.
 * timer_start(timer, delay, period) starts a periodic timer.
 *
 * The callback runs from the OS tick processing path. It must be short and
 * must not call blocking APIs. Use it to notify a task, set event bits, give a
 * semaphore, or perform other small non-blocking work.
 */

typedef void (*timer_callback_t)(void *arg);

typedef struct soft_timer {
    dlist_node_t node;
    os_tick_t expiry;
    os_tick_t period;
    timer_callback_t callback;
    void *arg;
    bool running;
} soft_timer_t;

/*
 * Initialize a soft timer.
 *
 * callback must not be NULL. The timer is stopped after initialization.
 */
void timer_init(soft_timer_t *timer,
                timer_callback_t callback,
                void *arg);

/*
 * Start or restart a soft timer.
 *
 * delay is the first timeout and must be nonzero and no larger than
 * OS_TICK_MAX_DELAY. period selects the mode:
 *   - period == 0: one-shot timer
 *   - period != 0: periodic timer, period must be <= OS_TICK_MAX_DELAY
 */
void timer_start(soft_timer_t *timer,
                 os_tick_t delay,
                 os_tick_t period);

/* Stop a soft timer. It is safe to stop an already stopped timer. */
void timer_stop(soft_timer_t *timer);

/* Return true when the timer is currently in the timer list. */
bool timer_is_running(const soft_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif /* OS_TIMER_ENABLE */

#endif /* TIMER_H */
