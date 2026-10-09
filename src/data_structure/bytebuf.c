#include <micaos/data_structure/bytebuf.h>

#include <string.h>

static inline uint32_t bytebuf_min(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

static inline uint32_t write_offset(const bytebuf_t *bb)
{
    return bb->write - bb->write_base;
}

static inline uint32_t read_offset(const bytebuf_t *bb)
{
    return bb->read - bb->read_base;
}

static inline uint32_t write_contig_space(const bytebuf_t *bb)
{
    return bytebuf_min(bytebuf_space(bb), bb->size - write_offset(bb));
}

static inline uint32_t read_contig_size(const bytebuf_t *bb)
{
    return bytebuf_min(bytebuf_size(bb), bb->size - read_offset(bb));
}

static inline void commit_write(bytebuf_t *bb, uint32_t size)
{
    uint32_t offset;

    bb->write += size;

    offset = bb->write - bb->write_base;
    if (offset >= bb->size) {
        bb->write_base += bb->size;
    }
}

static inline void commit_read(bytebuf_t *bb, uint32_t size)
{
    uint32_t offset;

    bb->read += size;

    offset = bb->read - bb->read_base;
    if (offset >= bb->size) {
        bb->read_base += bb->size;
    }
}

void bytebuf_init(bytebuf_t *bb, uint8_t *buffer, uint32_t size)
{
    OS_DIAG_ASSERT(bb != NULL);
    OS_DIAG_ASSERT(buffer != NULL);
    OS_DIAG_ASSERT(size > 0U);
    OS_DIAG_ASSERT(size <= (UINT32_MAX / 2U));

    bb->buffer = buffer;
    bb->size = size;
    bytebuf_reset(bb);
}

void bytebuf_reset(bytebuf_t *bb)
{
    OS_DIAG_ASSERT(bb != NULL);

    bb->read = 0U;
    bb->write = 0U;
    bb->read_base = 0U;
    bb->write_base = 0U;
}

bool bytebuf_put_u8(bytebuf_t *bb, uint8_t byte)
{
    bytebuf_idx_t offset;

    OS_DIAG_ASSERT(bb != NULL);
    OS_DIAG_ASSERT(bb->buffer != NULL);

    if (bytebuf_space(bb) == 0U) {
        return false;
    }

    offset = write_offset(bb);
    bb->buffer[offset] = byte;
    bb->write++;

    offset = write_offset(bb);
    if (offset == bb->size) {
        bb->write_base = bb->write;
    }

    return true;
}

bool bytebuf_get_u8(bytebuf_t *bb, uint8_t *byte)
{
    bytebuf_idx_t offset;

    OS_DIAG_ASSERT(bb != NULL);
    OS_DIAG_ASSERT(bb->buffer != NULL);
    OS_DIAG_ASSERT(byte != NULL);

    if (bytebuf_size(bb) == 0U) {
        return false;
    }

    offset = read_offset(bb);
    *byte = bb->buffer[offset];
    bb->read++;

    offset = read_offset(bb);
    if (offset == bb->size) {
        bb->read_base = bb->read;
    }

    return true;
}

uint32_t bytebuf_put(bytebuf_t *bb, const uint8_t *data, uint32_t size)
{
    uint32_t len, offset, first;

    OS_DIAG_ASSERT(bb != NULL);
    OS_DIAG_ASSERT(bb->buffer != NULL);
    OS_DIAG_ASSERT(data != NULL || size == 0U);

    len = bytebuf_min(size, bytebuf_space(bb));
    if (len == 0U) {
        return 0U;
    }

    offset = write_offset(bb);
    first = bytebuf_min(len, bb->size - offset);

    memcpy(&bb->buffer[offset], data, first);
    if (len > first) {
        memcpy(bb->buffer, data + first, len - first);
    }

    commit_write(bb, len);

    return len;
}

uint32_t bytebuf_get(bytebuf_t *bb, uint8_t *data, uint32_t size)
{
    uint32_t len, offset, first;

    OS_DIAG_ASSERT(bb != NULL);
    OS_DIAG_ASSERT(bb->buffer != NULL);
    OS_DIAG_ASSERT(data != NULL || size == 0U);

    len = bytebuf_min(size, bytebuf_size(bb));
    if (len == 0U) {
        return 0U;
    }

    offset = read_offset(bb);
    first = bytebuf_min(len, bb->size - offset);

    memcpy(data, &bb->buffer[offset], first);
    if (len > first) {
        memcpy(data + first, bb->buffer, len - first);
    }

    commit_read(bb, len);

    return len;
}

uint32_t bytebuf_peek(bytebuf_t *bb, uint8_t *data, uint32_t size)
{
    uint32_t len, offset, first;

    OS_DIAG_ASSERT(bb != NULL);
    OS_DIAG_ASSERT(bb->buffer != NULL);
    OS_DIAG_ASSERT(data != NULL || size == 0U);

    len = bytebuf_min(size, bytebuf_size(bb));
    if (len == 0U) {
        return 0U;
    }

    offset = read_offset(bb);
    first = bytebuf_min(len, bb->size - offset);

    memcpy(data, &bb->buffer[offset], first);
    if (len > first) {
        memcpy(data + first, bb->buffer, len - first);
    }

    return len;
}

uint32_t bytebuf_skip(bytebuf_t *bb, uint32_t size)
{
    uint32_t len;

    OS_DIAG_ASSERT(bb != NULL);

    len = bytebuf_min(size, bytebuf_size(bb));
    commit_read(bb, len);

    return len;
}

void bytebuf_skip_all(bytebuf_t *bb)
{
    OS_DIAG_ASSERT(bb != NULL);

    commit_read(bb, bytebuf_size(bb));
}

uint32_t bytebuf_put_claim(bytebuf_t *bb, uint8_t **data, uint32_t size)
{
    uint32_t len, offset;

    OS_DIAG_ASSERT(bb != NULL);
    OS_DIAG_ASSERT(bb->buffer != NULL);
    OS_DIAG_ASSERT(data != NULL);

    len = bytebuf_min(size, write_contig_space(bb));
    if (len == 0U) {
        *data = NULL;
        return 0U;
    }

    offset = write_offset(bb);
    *data = &bb->buffer[offset];

    return len;
}

int bytebuf_put_finish(bytebuf_t *bb, uint32_t size)
{
    OS_DIAG_ASSERT(bb != NULL);

    if (size > write_contig_space(bb)) {
        return -1;
    }

    commit_write(bb, size);

    return 0;
}

uint32_t bytebuf_get_claim(bytebuf_t *bb, uint8_t **data, uint32_t size)
{
    uint32_t len, offset;

    OS_DIAG_ASSERT(bb != NULL);
    OS_DIAG_ASSERT(bb->buffer != NULL);
    OS_DIAG_ASSERT(data != NULL);

    len = bytebuf_min(size, read_contig_size(bb));
    if (len == 0U) {
        *data = NULL;
        return 0U;
    }

    offset = read_offset(bb);
    *data = &bb->buffer[offset];

    return len;
}

int bytebuf_get_finish(bytebuf_t *bb, uint32_t size)
{
    OS_DIAG_ASSERT(bb != NULL);

    if (size > read_contig_size(bb)) {
        return -1;
    }

    commit_read(bb, size);

    return 0;
}
