#ifndef MSGQ_H
#define MSGQ_H

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
 * Message Queue
 *
 * Fixed-size message queue with blocking send/receive operations. Storage is
 * provided by the caller; this module does not allocate memory.
 *
 * Usage:
 *   1. Declare storage with msgq_storage() or another caller-owned buffer.
 *   2. Initialize with msgq_init().
 *   3. Producers call msgq_send().
 *   4. Consumers call msgq_recv().
 *
 * Messages are copied into and out of the queue. If send/receive blocks, the
 * user msg pointer is not stored; the task retries the copy after wakeup. ISR
 * context may use send/receive only with timeout set to OS_NO_WAIT.
 */

typedef struct msgq {
    uint8_t *buffer;
    uint16_t msg_size;
    uint16_t capacity;
    uint16_t read;
    uint16_t write;
    uint16_t count;
    dlist_t send_wait_list;
    dlist_t recv_wait_list;
} msgq_t;

/* Declare storage for a fixed-size message queue. */
#define msgq_storage(name, msg_type, capacity) \
    uint8_t name[sizeof(msg_type) * (capacity)]

/*
 * Initialize a fixed-size message queue.
 *
 * buffer must point to msg_size * capacity bytes of caller-owned storage.
 */
void msgq_init(msgq_t *q, void *buffer, uint16_t msg_size, uint16_t capacity);

/*
 * Send one fixed-size message.
 *
 * Return true when the message is copied into the queue. Return false when
 * timeout is OS_NO_WAIT and the queue is full, or when a finite timeout
 * expires. When timeout is OS_NO_WAIT, the function only checks once and never
 * blocks. Use OS_WAIT_FOREVER to wait without a timeout.
 *
 * If this call blocks, the msg pointer is not stored by the queue. The task
 * retries the copy after being woken.
 *
 * ISR context may call this function only with timeout set to OS_NO_WAIT.
 */
bool msgq_send(msgq_t *q, const void *msg, os_tick_t timeout);

/*
 * Receive one fixed-size message.
 *
 * Return true when a message is copied into msg. Return false when timeout is
 * OS_NO_WAIT and the queue is empty, or when a finite timeout expires. When
 * timeout is OS_NO_WAIT, the function only checks once and never blocks. Use
 * OS_WAIT_FOREVER to wait without a timeout.
 *
 * ISR context may call this function only with timeout set to OS_NO_WAIT.
 */
bool msgq_recv(msgq_t *q, void *msg, os_tick_t timeout);

/* Return the number of queued messages. */
uint16_t msgq_count(msgq_t *q);

/* Return the number of free message slots. */
uint16_t msgq_space(msgq_t *q);

#ifdef __cplusplus
}
#endif

#endif /* MSGQ_H */
