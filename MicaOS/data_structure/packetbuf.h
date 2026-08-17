#ifndef PACKETBUF_H
#define PACKETBUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include "common/assert.h"

/*
 * Packet-oriented ring buffer with copy and zero-copy access, SPSC lock-free.
 *
 * Unlike bytebuf, which stores a byte stream, packetbuf stores complete
 * variable-length packets. Each successful write produces exactly one packet,
 * and each successful read consumes exactly one packet.
 *
 * Usage:
 *   1. Allocate the storage buffer yourself. The storage may be any byte count
 *      greater than 2 and no larger than UINT32_MAX / 2U.
 *   2. Initialize the control object with packetbuf_init(), or use
 *      packetbuf_static_init() for static-storage objects.
 *   3. Use packetbuf_put()/packetbuf_get() for ordinary packet copying.
 *      Writes are all-or-nothing: a packet is either fully stored or rejected.
 *   4. Use packetbuf_reserve()/packetbuf_commit() when the producer wants to
 *      write directly into the internal payload storage. A reserve must be
 *      finished by exactly one commit; committing 0 cancels the reserve.
 *   5. Use packetbuf_claim()/packetbuf_release() when the consumer wants to
 *      read the next packet directly from internal storage. claim() does not
 *      move the read cursor; release() consumes that packet.
 *
 * Usage notes:
 *   - Packet length is stored in a 16-bit internal header. User packet size
 *     must be 1..UINT16_MAX bytes.
 *   - Each packet is stored as one contiguous header + payload block. If the
 *     physical tail cannot hold the next complete packet, that tail space is
 *     used as internal padding and the packet starts again from the beginning
 *     of the storage buffer. This keeps zero-copy access simple, but may
 *     temporarily waste a small amount of capacity.
 *   - Parameter validity is not checked by default; callers must satisfy API
 *     preconditions.
 *   - Define PACKETBUF_DIAGNOSTIC_ENABLE to 1 when debugging misuse; it enables
 *     ASSERT-based parameter and state checks.
 *
 * Concurrency:
 *   This module does not provide any concurrency control. Callers in multi-threaded
 *   environments should protect the structure with appropriate synchronization
 *   mechanisms. SPSC use can be lock-free when producer and consumer operate on
 *   separate directions, cursor access is atomic on the target, and DMA/cache
 *   ordering is handled by the caller.
 *
 * API quick reference:
 *   - Initialization:
 *       packetbuf_static_init()
 *       packetbuf_init()
 *       packetbuf_reset()
 *
 *   - State queries:
 *       packetbuf_capacity()
 *       packetbuf_size()
 *       packetbuf_space()
 *       packetbuf_is_empty()
 *       packetbuf_is_full()
 *       packetbuf_peek_size()
 *
 *   - Copying operations:
 *       packetbuf_put()
 *       packetbuf_get()
 *       packetbuf_drop()
 *
 *   - Zero-copy operations:
 *       packetbuf_reserve()
 *       packetbuf_commit()
 *       packetbuf_claim()
 *       packetbuf_release()
 *
 * Examples:
 *   static uint8_t storage[128];
 *   static packetbuf_t packets = packetbuf_static_init(storage, sizeof(storage));
 *
 *   uint8_t frame[] = { 0x01, 0x02, 0x03 };
 *   packetbuf_put(&packets, frame, sizeof(frame));
 *
 *   uint8_t rx[16];
 *   uint16_t len = packetbuf_get(&packets, rx, sizeof(rx));
 *   if (len != 0U) {
 *       process_packet(rx, len);
 *   }
 *
 * Zero-copy producer example:
 *   uint8_t *payload = packetbuf_reserve(&packets, 20);
 *   if (payload != NULL) {
 *       fill_packet(payload, 20);
 *       packetbuf_commit(&packets, 20);
 *   }
 *
 * Zero-copy consumer example:
 *   uint16_t packet_len;
 *   const uint8_t *packet = packetbuf_claim(&packets, &packet_len);
 *   if (packet != NULL) {
 *       process_packet(packet, packet_len);
 *       packetbuf_release(&packets);
 *   }
 */

typedef struct packetbuf {
    uint8_t *buffer;
    uint32_t size;
    uint32_t read;
    uint32_t write;
    uint32_t read_base;
    uint32_t write_base;
    uint32_t put_claim_padding;
    uint32_t put_claim_size;
} packetbuf_t;

#ifndef PACKETBUF_DIAGNOSTIC_ENABLE
#define PACKETBUF_DIAGNOSTIC_ENABLE 0
#endif

#if PACKETBUF_DIAGNOSTIC_ENABLE
#define PACKETBUF_ASSERT(cond) ASSERT(cond)
#else
#define PACKETBUF_ASSERT(cond) ((void)sizeof(cond))
#endif

/*
 * Static initializer.
 *
 * Example:
 *   static uint8_t storage[128];
 *   static packetbuf_t packets = packetbuf_static_init(storage, sizeof(storage));
 */
#define packetbuf_static_init(buffer_, size_) \
    { (buffer_), (uint32_t)(size_), 0U, 0U, 0U, 0U, 0U, 0U }

/*
 * Initialize pb with caller-owned storage.
 *
 * Precondition:
 *   - pb and buffer are non-NULL.
 *   - size is greater than 2 and no larger than UINT32_MAX / 2U.
 */
void packetbuf_init(packetbuf_t *pb, uint8_t *buffer, uint32_t size);

/* Drop all buffered packets. */
void packetbuf_reset(packetbuf_t *pb);

/* Return the raw storage capacity in bytes. */
static inline uint32_t packetbuf_capacity(const packetbuf_t *pb)
{
    PACKETBUF_ASSERT(pb != NULL);
    return pb->size;
}

/* Return occupied bytes, including internal packet headers and padding. */
static inline uint32_t packetbuf_size(const packetbuf_t *pb)
{
    PACKETBUF_ASSERT(pb != NULL);
    return pb->write - pb->read;
}

/* Return free raw storage bytes. */
static inline uint32_t packetbuf_space(const packetbuf_t *pb)
{
    PACKETBUF_ASSERT(pb != NULL);
    return pb->size - (pb->write - pb->read);
}

/* Return true when no committed packet is available. */
static inline bool packetbuf_is_empty(const packetbuf_t *pb)
{
    PACKETBUF_ASSERT(pb != NULL);
    return pb->read == pb->write;
}

/*
 * Return true when a minimum-size packet cannot currently be stored.
 *
 * This accounts for internal wrap padding, so it may become true before raw
 * free space reaches zero.
 */
bool packetbuf_is_full(const packetbuf_t *pb);

/*
 * Return the payload size of the next packet without consuming it.
 *
 * Return 0 when the buffer is empty. Zero-length user packets are not supported,
 * so 0 is unambiguous.
 */
uint16_t packetbuf_peek_size(packetbuf_t *pb);

/*
 * Copy one packet into pb.
 *
 * Return true when the whole packet is stored. Return false when size is 0,
 * there is not enough space, or the packet is larger than the storage can hold.
 * This API must not be used while a reserve is outstanding.
 */
bool packetbuf_put(packetbuf_t *pb, const void *data, uint16_t size);

/*
 * Copy the next packet out of pb.
 *
 * Return the packet length on success. Return 0 when pb is empty or when size is
 * smaller than the next packet. In the latter case, the packet is not consumed.
 */
uint16_t packetbuf_get(packetbuf_t *pb, void *data, uint16_t size);

/*
 * Drop the next packet without copying it.
 *
 * Return true when a packet was consumed, false when pb was empty.
 */
bool packetbuf_drop(packetbuf_t *pb);

/*
 * Reserve payload storage for one packet.
 *
 * Return a writable payload pointer on success, or NULL when size is 0, there is
 * not enough space, or another reserve is already outstanding. The returned
 * storage is not visible to the consumer until packetbuf_commit() succeeds.
 */
void *packetbuf_reserve(packetbuf_t *pb, uint16_t size);

/*
 * Commit a previous reserve.
 *
 * size must be no larger than the size passed to packetbuf_reserve(). Passing
 * 0 cancels the reserve and publishes no packet. Return 0 on success, or -1 on
 * invalid size/state.
 */
int packetbuf_commit(packetbuf_t *pb, uint16_t size);

/*
 * Claim the next packet for direct reading.
 *
 * *size receives the payload size. Return NULL when pb is empty. This function
 * does not move the read cursor; call packetbuf_release() after the packet has
 * been processed. Repeated claims before release return the same packet.
 */
const void *packetbuf_claim(packetbuf_t *pb, uint16_t *size);

/*
 * Release the packet returned by packetbuf_claim().
 *
 * This consumes the current front packet. In normal zero-copy usage, call it
 * after processing the pointer returned by packetbuf_claim().
 */
void packetbuf_release(packetbuf_t *pb);

#ifdef __cplusplus
}
#endif

#endif /* PACKETBUF_H */
