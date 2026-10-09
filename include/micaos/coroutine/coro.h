#ifndef CORO_H
#define CORO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

/*
 * Lightweight stackless coroutine.
 *
 * Usage:
 *   - Store one coro_t object in the owner object.
 *   - Call the coroutine step function repeatedly.
 *   - The step function must start with coro_begin() and end with coro_end().
 *   - Use coro_yield(), coro_wait_until(), coro_wait_while(), coro_exit(), and
 *     coro_restart() only between coro_begin() and coro_end().
 *
 * Example:
 *   typedef struct blink_coro {
 *       coro_t coro;
 *       bool ready;
 *       uint32_t count;
 *   } blink_coro_t;
 *
 *   static blink_coro_t blink0 = {
 *       .coro = coro_static_init(),
 *   };
 *
 *   static void blink1_init(blink_coro_t *self)
 *   {
 *       coro_init(&self->coro);
 *       self->ready = false;
 *       self->count = 0U;
 *   }
 *
 *   static coro_status_t blink_step(blink_coro_t *self)
 *   {
 *       coro_begin(&self->coro);
 *
 *       for (;;) {
 *           self->count++;
 *           coro_yield(&self->coro);
 *
 *           coro_wait_until(&self->coro, self->ready);
 *           self->ready = false;
 *       }
 *
 *       coro_end(&self->coro);
 *   }
 *
 * Constraints:
 *   - This module does not validate parameters. The caller shall pass valid
 *     coro_t objects.
 *   - Automatic local variables do not survive a yield or wait. Store
 *     persistent values in the owner object.
 *   - Put each yield/wait/restart/exit macro on its own source line. The
 *     implementation uses __LINE__ as the resume point.
 *   - Do not put yield/wait/restart/exit macros inside a user switch statement.
 *   - A single coro_t object must not be driven concurrently from multiple
 *     contexts.
 */

typedef enum coro_status {
    CORO_WAITING = 0,
    CORO_DONE = 1,
} coro_status_t;

typedef struct coro {
    uint32_t state;
} coro_t;

#define CORO_STATE_START_ 0U
#define CORO_STATE_DONE_  UINT32_MAX

#define coro_static_init() { CORO_STATE_START_ }

static inline void coro_init(coro_t *coro)
{
    coro->state = CORO_STATE_START_;
}

static inline void coro_reset(coro_t *coro)
{
    coro->state = CORO_STATE_START_;
}

static inline bool coro_is_done(const coro_t *coro)
{
    return coro->state == CORO_STATE_DONE_;
}

#define coro_begin(coro_)                                                                          \
    do {                                                                                           \
        if ((coro_)->state == CORO_STATE_DONE_) {                                                  \
            return CORO_DONE;                                                                      \
        }                                                                                          \
        switch ((coro_)->state) {                                                                  \
        case CORO_STATE_START_:

#define coro_end(coro_)                                                                            \
    }                                                                                              \
    (coro_)->state = CORO_STATE_DONE_;                                                             \
    return CORO_DONE;                                                                              \
    } while (0)

#define coro_yield(coro_)                                                                          \
    do {                                                                                           \
        (coro_)->state = (uint32_t)__LINE__;                                                       \
        return CORO_WAITING;                                                                       \
    case (uint32_t)__LINE__:                                                                       \
        break;                                                                                     \
    } while (0)

#define coro_wait_until(coro_, condition_)                                                         \
    do {                                                                                           \
        (coro_)->state = (uint32_t)__LINE__;                                                       \
    case (uint32_t)__LINE__:                                                                       \
        if (!(condition_)) {                                                                       \
            return CORO_WAITING;                                                                   \
        }                                                                                          \
    } while (0)

#define coro_wait_while(coro_, condition_) coro_wait_until((coro_), !(condition_))

#define coro_exit(coro_)                                                                           \
    do {                                                                                           \
        (coro_)->state = CORO_STATE_DONE_;                                                         \
        return CORO_DONE;                                                                          \
    } while (0)

#define coro_restart(coro_)                                                                        \
    do {                                                                                           \
        (coro_)->state = CORO_STATE_START_;                                                        \
        return CORO_WAITING;                                                                       \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* CORO_H */
