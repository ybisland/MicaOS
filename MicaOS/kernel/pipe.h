#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>
#include "data_structure/bytebuf.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pipe
 *
 * Strict SPSC byte stream with blocking read/write operations. Storage is
 * provided by the caller; this module does not allocate memory.
 *
 * Usage:
 *   1. Declare storage with pipe_storage() or another caller-owned buffer.
 *   2. Initialize with pipe_init().
 *   3. The single producer calls pipe_write().
 *   4. The single consumer calls pipe_read().
 *
 * This module intentionally supports one writer and one reader only. It is not
 * an MPMC pipe. ISR context may use read/write only with timeout set to
 * OS_NO_WAIT.
 *
 * Read and write return the number of bytes actually transferred. A successful
 * blocking wait only guarantees that at least one byte can be transferred.
 */

typedef struct pipe {
    bytebuf_t buf;
    task_t *reader_waiter;
    task_t *writer_waiter;
} pipe_t;

/* Declare storage for a pipe byte stream. */
#define pipe_storage(name, size) uint8_t name[(size)]

/*
 * Initialize an SPSC pipe.
 *
 * buffer must point to size bytes of caller-owned storage. size must be
 * nonzero and no larger than UINT16_MAX.
 */
void pipe_init(pipe_t *pipe, void *buffer, uint16_t size);

/*
 * Write bytes into the pipe.
 *
 * Return the number of bytes written. Return 0 when timeout is OS_NO_WAIT and
 * the pipe is full, or when a finite timeout expires. When timeout is
 * OS_NO_WAIT, the function only checks once and never blocks. Use
 * OS_WAIT_FOREVER to wait until at least one byte can be written.
 *
 * ISR context may call this function only with timeout set to OS_NO_WAIT.
 */
uint16_t pipe_write(pipe_t *pipe,
                    const void *data,
                    uint16_t len,
                    os_tick_t timeout);

/*
 * Read bytes from the pipe.
 *
 * Return the number of bytes read. Return 0 when timeout is OS_NO_WAIT and the
 * pipe is empty, or when a finite timeout expires. When timeout is OS_NO_WAIT,
 * the function only checks once and never blocks. Use OS_WAIT_FOREVER to wait
 * until at least one byte can be read.
 *
 * ISR context may call this function only with timeout set to OS_NO_WAIT.
 */
uint16_t pipe_read(pipe_t *pipe,
                   void *data,
                   uint16_t len,
                   os_tick_t timeout);

/* Return the number of readable bytes currently buffered. */
uint16_t pipe_count(pipe_t *pipe);

/* Return the number of writable bytes currently available. */
uint16_t pipe_space(pipe_t *pipe);

/*
 * Drop all buffered bytes and abort any pending reader/writer wait.
 *
 * Woken waiters return 0 from their read/write call. This is intended for
 * error recovery or protocol reset, not ordinary data transfer.
 */
void pipe_reset(pipe_t *pipe);

#ifdef __cplusplus
}
#endif

#endif /* PIPE_H */
