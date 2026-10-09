#ifndef SEM_H
#define SEM_H

#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <micaos/data_structure/dlist.h>
#include <micaos/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Semaphore
 *
 * Lightweight counting semaphore. A semaphore represents tokens. sem_give()
 * produces one token or wakes one waiting task. sem_take() consumes one
 * token or waits for one to become available.
 *
 * Typical uses:
 *   - counting available buffers or packets
 *   - task/ISR producer to task consumer signaling
 *   - multi-producer or multi-consumer token arbitration
 *
 * When multiple tasks are already waiting on the same semaphore, waiters are
 * woken in FIFO order. After a waiter is woken, scheduler priority determines
 * when it actually runs.
 */

typedef struct sem {
    uint16_t count;
    uint16_t limit;
    dlist_t wait_list;
} sem_t;

/*
 * Initialize a counting semaphore.
 *
 * initial is the starting token count. limit is the maximum token count.
 */
void sem_init(sem_t *sem, uint16_t initial, uint16_t limit);

/*
 * Give one token to the semaphore.
 *
 * This function may be called from task or ISR context. If tasks are waiting,
 * the oldest waiter is woken. Otherwise count is incremented up to limit.
 * Giving when count already equals limit leaves the semaphore saturated.
 */
void sem_give(sem_t *sem);

/*
 * Take one token from the semaphore.
 *
 * Return true when a token is acquired. Return false when timeout is
 * OS_NO_WAIT and no token is available, or when a finite timeout expires. When
 * timeout is OS_NO_WAIT, the function only checks once and never blocks. Use
 * OS_WAIT_FOREVER to wait without a timeout.
 */
bool sem_take(sem_t *sem, os_tick_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* SEM_H */
