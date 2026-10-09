#ifndef BYTEBUF_H
#define BYTEBUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <micaos/common/assert.h>

/**
 * Byte-oriented RingBuffer with copy and zero-copy access, SPSC lock-free.
 *
 * Usage:
 *   1. Allocate the storage buffer yourself. The capacity may be any non-zero
 *      byte count up to UINT32_MAX / 2U.
 *   2. Initialize the control object with bytebuf_init().
 *   3. Use bytebuf_put()/bytebuf_get() for ordinary byte-stream copying.
 *      Use bytebuf_put_u8()/bytebuf_get_u8() for hot single-byte paths.
 *   4. Use bytebuf_put_claim()/bytebuf_put_finish() when a producer wants to
 *      write directly into the internal storage, for example DMA RX.
 *   5. Use bytebuf_get_claim()/bytebuf_get_finish() when a consumer wants to
 *      read directly from the internal storage, for example DMA TX.
 *   6. claim() and finish() are one-to-one in each direction (read/write). Do
 *      not make another claim in the same direction before finishing or
 *      cancelling the previous one. Read/write cursors are not moved until
 *      finish() is called.
 *
 * Usage notes:
 *   - Parameter validity is not checked by default; callers must satisfy API
 *     preconditions.
 *   - Define OS_DIAGNOSTIC_ENABLE to 1 when debugging misuse; it enables
 *     parameter and state checks.
 *
 * Concurrency:
 *   This module does not provide any concurrency control. Callers in multi-threaded
 *   environments should protect the structure with appropriate synchronization
 *   mechanisms (e.g., mutexes, semaphores) to ensure thread safety.
 *   SPSC use can be lock-free when producer and consumer operate on separate
 *   directions, cursor access is atomic on the target, and DMA/cache ordering
 *   is handled by the caller.
 *
 * API quick reference:
 *   - Initialization:
 *       bytebuf_static_init()
 *       bytebuf_init()
 *       bytebuf_reset()
 *
 *   - State queries:
 *       bytebuf_capacity()
 *       bytebuf_size()
 *       bytebuf_space()
 *       bytebuf_is_empty()
 *       bytebuf_is_full()
 *
 *   - Copying operations:
 *       bytebuf_put_u8()
 *       bytebuf_get_u8()
 *       bytebuf_put()
 *       bytebuf_get()
 *       bytebuf_peek()
 *       bytebuf_skip()
 *       bytebuf_skip_all()
 *
 *   - Zero-copy operations:
 *       bytebuf_put_claim()
 *       bytebuf_put_finish()
 *       bytebuf_get_claim()
 *       bytebuf_get_finish()
 *
 * Examples:
 *   static uint8_t rx_storage[128];
 *   static bytebuf_t rx_bytes = bytebuf_static_init(rx_storage,
 *                                                    sizeof(rx_storage));
 *
 *   bytebuf_put_u8(&rx_bytes, byte_from_isr);
 *
 *   uint8_t byte;
 *   if (bytebuf_get_u8(&rx_bytes, &byte)) {
 *       process(byte);
 *   }
 *
 * DMA RX example:
 *   // If 20 bytes are expected but the physical buffer has less contiguous
 *   // space before wraparound, claim returns only the first contiguous part.
 *   uint8_t *dst;
 *   uint32_t remaining = 20;
 *   uint32_t len = bytebuf_put_claim(&rx_bytes, &dst, remaining);
 *   start_dma_rx(dst, len);
 *
 *   // DMA completion callback: publish exactly this claim.
 *   bytebuf_put_finish(&rx_bytes, len);
 *   remaining -= len;
 *
 *   if (remaining != 0U) {
 *       len = bytebuf_put_claim(&rx_bytes, &dst, remaining);
 *       start_dma_rx(dst, len);
 *
 *       // DMA completion callback: finish the second claim.
 *       bytebuf_put_finish(&rx_bytes, len);
 *       remaining -= len;
 *   }
 */

typedef uint32_t bytebuf_idx_t;

typedef struct bytebuf {
    uint8_t *buffer;
    uint32_t size;
    bytebuf_idx_t read;
    bytebuf_idx_t write;
    bytebuf_idx_t read_base;
    bytebuf_idx_t write_base;
} bytebuf_t;

/*
 * Static initializer.
 *
 * Example:
 *   static uint8_t rx_storage[128];
 *   static bytebuf_t rx_bytes = bytebuf_static_init(rx_storage, sizeof(rx_storage));
 */
#define bytebuf_static_init(buffer_, size_) \
    { (buffer_), (uint32_t)(size_), 0U, 0U, 0U, 0U }

/*
 * Initialize bb with caller-owned storage.
 *
 * Precondition:
 *   - bb and buffer are non-NULL.
 *   - size is greater than 0 and no larger than UINT32_MAX / 2U.
 */
void bytebuf_init(bytebuf_t *bb, uint8_t *buffer, uint32_t size);

/*
 * Drop all buffered data.
 *
 * The storage content is not cleared.
 */
void bytebuf_reset(bytebuf_t *bb);

/* Return the capacity in bytes. */
static inline uint32_t bytebuf_capacity(const bytebuf_t *bb)
{
    OS_DIAG_ASSERT(bb != NULL);
    return bb->size;
}

/* Return committed readable bytes. */
static inline uint32_t bytebuf_size(const bytebuf_t *bb)
{
    OS_DIAG_ASSERT(bb != NULL);
    return bb->write - bb->read;
}

/* Return writable bytes. */
static inline uint32_t bytebuf_space(const bytebuf_t *bb)
{
    OS_DIAG_ASSERT(bb != NULL);
    return bb->size - (bb->write - bb->read);
}

/* Return true when no committed readable bytes are available. */
static inline bool bytebuf_is_empty(const bytebuf_t *bb)
{
    OS_DIAG_ASSERT(bb != NULL);
    return bb->read == bb->write;
}

/* Return true when no writable space is available. */
static inline bool bytebuf_is_full(const bytebuf_t *bb)
{
    OS_DIAG_ASSERT(bb != NULL);
    return bb->write == (bb->read + bb->size);
}

/*
 * Fast-path single-byte write.
 *
 * Return true on success, false when the buffer is full. This API must not be
 * used while a put claim is outstanding.
 */
bool bytebuf_put_u8(bytebuf_t *bb, uint8_t byte);

/*
 * Fast-path single-byte read.
 *
 * Return true on success, false when the buffer is empty. This API must not be
 * used while a get claim is outstanding.
 */
bool bytebuf_get_u8(bytebuf_t *bb, uint8_t *byte);

/*
 * Copy data into bb.
 *
 * Return the number of bytes written, which may be smaller than size when
 * there is not enough space. This API must not be used while a put claim is
 * outstanding.
 */
uint32_t bytebuf_put(bytebuf_t *bb, const uint8_t *data, uint32_t size);

/*
 * Copy data out of bb.
 *
 * Return the number of bytes read, which may be smaller than size when there
 * is not enough data. This API must not be used while a get claim is
 * outstanding.
 */
uint32_t bytebuf_get(bytebuf_t *bb, uint8_t *data, uint32_t size);

/*
 * Copy data out of bb without consuming it.
 *
 * Return the number of bytes copied. This API must not be used while a get
 * claim is outstanding.
 */
uint32_t bytebuf_peek(bytebuf_t *bb, uint8_t *data, uint32_t size);

/*
 * Consume up to size bytes without copying them.
 *
 * Return the number of bytes consumed. This API must not be used while a get
 * claim is outstanding.
 */
uint32_t bytebuf_skip(bytebuf_t *bb, uint32_t size);

/*
 * Consume all currently readable bytes.
 *
 * This API must not be used while a get claim is outstanding.
 */
void bytebuf_skip_all(bytebuf_t *bb);

/*
 * Claim the current contiguous writable area.
 *
 * *data receives the start of the contiguous area. Return the claimed byte
 * count, which may be smaller than size because claims stop at the physical end
 * of the storage buffer. This function does not move the write cursor.
 */
uint32_t bytebuf_put_claim(bytebuf_t *bb, uint8_t **data, uint32_t size);

/*
 * Finish a previous write claim.
 *
 * size must be no larger than the bytes returned by the previous put claim.
 * Return 0 on success, or -1 when size is larger than the currently writable
 * contiguous area.
 */
int bytebuf_put_finish(bytebuf_t *bb, uint32_t size);

/*
 * Claim the current contiguous readable area.
 *
 * *data receives the start of the contiguous area. Return the claimed byte
 * count, which may be smaller than size because claims stop at the physical end
 * of the storage buffer. This function does not move the read cursor.
 */
uint32_t bytebuf_get_claim(bytebuf_t *bb, uint8_t **data, uint32_t size);

/*
 * Finish a previous read claim.
 *
 * size must be no larger than the bytes returned by the previous get claim.
 * Return 0 on success, or -1 when size is larger than the currently readable
 * contiguous area.
 */
int bytebuf_get_finish(bytebuf_t *bb, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* BYTEBUF_H */
