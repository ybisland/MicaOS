#ifndef EVENTSET_H
#define EVENTSET_H

#include <stdint.h>
#include <micaos/data_structure/dlist.h>
#include <micaos/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Eventset
 *
 * Event flag object for named conditions. One eventset stores 32 bits of
 * state. Tasks can wait until any selected bit is set or until all selected
 * bits are set.
 *
 * Usage:
 *   1. Initialize with eventset_init().
 *   2. Producers call eventset_set() from task or ISR context.
 *   3. Consumers call eventset_wait_any() or eventset_wait_all().
 *   4. Clear consumed state explicitly with eventset_clear().
 *
 * Waiting does not automatically clear bits. This keeps eventset as shared
 * state and avoids hiding a condition from other waiters.
 */

typedef uint32_t eventset_bits_t;

typedef struct eventset {
    eventset_bits_t bits;
    dlist_t wait_list;
} eventset_t;

/* Initialize an eventset object before first use. */
void eventset_init(eventset_t *eventset);

/*
 * Set event bits and wake every waiting task whose condition becomes true.
 *
 * This function may be called from task or ISR context. bits must be nonzero.
 */
void eventset_set(eventset_t *eventset, eventset_bits_t bits);

/*
 * Clear event bits explicitly.
 *
 * Waiting does not automatically clear bits, so users decide when a condition
 * has been consumed.
 */
void eventset_clear(eventset_t *eventset, eventset_bits_t bits);

/* Return the current event bits. */
eventset_bits_t eventset_get(eventset_t *eventset);

/*
 * Wait until any bit in mask is set.
 *
 * Return the matching bits. Return 0 when timeout is OS_NO_WAIT and no
 * matching bit is set, or when a finite timeout expires. When timeout is
 * OS_NO_WAIT, the function only checks once and never blocks. Use
 * OS_WAIT_FOREVER to wait without a timeout.
 *
 * Waiting does not clear returned bits; call eventset_clear() explicitly when
 * a bit should be consumed.
 */
eventset_bits_t eventset_wait_any(eventset_t *eventset,
                                  eventset_bits_t mask,
                                  os_tick_t timeout);

/*
 * Wait until all bits in mask are set.
 *
 * Return mask when all requested bits are set. Return 0 when timeout is
 * OS_NO_WAIT and the condition is not met, or when a finite timeout expires.
 *
 * Waiting does not clear returned bits; call eventset_clear() explicitly when
 * a bit should be consumed.
 */
eventset_bits_t eventset_wait_all(eventset_t *eventset,
                                  eventset_bits_t mask,
                                  os_tick_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* EVENTSET_H */
