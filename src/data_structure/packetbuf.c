#include <micaos/data_structure/packetbuf.h>

#include <string.h>

#define PACKETBUF_HEADER_SIZE 2U

static inline uint32_t packetbuf_min(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

static inline uint32_t write_offset(const packetbuf_t *pb)
{
    return pb->write - pb->write_base;
}

static inline uint32_t read_offset(const packetbuf_t *pb)
{
    return pb->read - pb->read_base;
}

static inline uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static inline void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static inline void commit_write(packetbuf_t *pb, uint32_t size)
{
    uint32_t offset;

    pb->write += size;

    offset = pb->write - pb->write_base;
    if (offset >= pb->size) {
        pb->write_base += pb->size;
    }
}

static inline void commit_read(packetbuf_t *pb, uint32_t size)
{
    uint32_t offset;

    pb->read += size;

    offset = pb->read - pb->read_base;
    if (offset >= pb->size) {
        pb->read_base += pb->size;
    }
}

static inline void reset_cursors_if_empty(packetbuf_t *pb)
{
    if (pb->read == pb->write) {
        pb->read = 0U;
        pb->write = 0U;
        pb->read_base = 0U;
        pb->write_base = 0U;
    }
}

static bool reserve_layout(const packetbuf_t *pb, uint16_t size,
                           uint32_t *padding, uint32_t *offset)
{
    uint32_t packet_size;
    uint32_t space;
    uint32_t tail;
    uint32_t write_pos;
    uint32_t needed;

    if (size == 0U) {
        return false;
    }

    packet_size = PACKETBUF_HEADER_SIZE + (uint32_t)size;
    if (packet_size > pb->size) {
        return false;
    }

    space = packetbuf_space(pb);
    write_pos = packetbuf_is_empty(pb) ? 0U : write_offset(pb);
    tail = pb->size - write_pos;

    if (tail >= packet_size) {
        needed = packet_size;
        *padding = 0U;
        *offset = write_pos;
    } else {
        needed = tail + packet_size;
        *padding = tail;
        *offset = 0U;
    }

    return needed <= space;
}

static void skip_internal_padding(packetbuf_t *pb)
{
    uint32_t available;
    uint32_t offset;
    uint32_t tail;
    uint16_t length;

    while (!packetbuf_is_empty(pb)) {
        available = packetbuf_size(pb);
        offset = read_offset(pb);
        tail = pb->size - offset;

        if (tail < PACKETBUF_HEADER_SIZE) {
            commit_read(pb, packetbuf_min(tail, available));
            continue;
        }

        if (available < PACKETBUF_HEADER_SIZE) {
            OS_DIAG_ASSERT(false);
            return;
        }

        length = read_u16(&pb->buffer[offset]);
        if (length != 0U) {
            return;
        }

        commit_read(pb, tail);
    }

    reset_cursors_if_empty(pb);
}

void packetbuf_init(packetbuf_t *pb, uint8_t *buffer, uint32_t size)
{
    OS_DIAG_ASSERT(pb != NULL);
    OS_DIAG_ASSERT(buffer != NULL);
    OS_DIAG_ASSERT(size > PACKETBUF_HEADER_SIZE);
    OS_DIAG_ASSERT(size <= (UINT32_MAX / 2U));

    pb->buffer = buffer;
    pb->size = size;
    packetbuf_reset(pb);
}

void packetbuf_reset(packetbuf_t *pb)
{
    OS_DIAG_ASSERT(pb != NULL);

    pb->read = 0U;
    pb->write = 0U;
    pb->read_base = 0U;
    pb->write_base = 0U;
    pb->put_claim_padding = 0U;
    pb->put_claim_size = 0U;
}

bool packetbuf_is_full(const packetbuf_t *pb)
{
    uint32_t padding;
    uint32_t offset;

    OS_DIAG_ASSERT(pb != NULL);
    OS_DIAG_ASSERT(pb->buffer != NULL);

    return !reserve_layout(pb, 1U, &padding, &offset);
}

uint16_t packetbuf_peek_size(packetbuf_t *pb)
{
    uint32_t offset;
    uint32_t available;
    uint16_t length;

    OS_DIAG_ASSERT(pb != NULL);
    OS_DIAG_ASSERT(pb->buffer != NULL);

    skip_internal_padding(pb);
    if (packetbuf_is_empty(pb)) {
        return 0U;
    }

    offset = read_offset(pb);
    available = packetbuf_size(pb);
    length = read_u16(&pb->buffer[offset]);

    OS_DIAG_ASSERT(length != 0U);
    OS_DIAG_ASSERT((uint32_t)length + PACKETBUF_HEADER_SIZE <= available);

    return length;
}

bool packetbuf_put(packetbuf_t *pb, const void *data, uint16_t size)
{
    void *payload;

    OS_DIAG_ASSERT(pb != NULL);
    OS_DIAG_ASSERT(data != NULL || size == 0U);

    payload = packetbuf_reserve(pb, size);
    if (payload == NULL) {
        return false;
    }

    memcpy(payload, data, size);
    return packetbuf_commit(pb, size) == 0;
}

uint16_t packetbuf_get(packetbuf_t *pb, void *data, uint16_t size)
{
    const void *payload;
    uint16_t packet_size;

    OS_DIAG_ASSERT(pb != NULL);
    OS_DIAG_ASSERT(data != NULL || size == 0U);

    payload = packetbuf_claim(pb, &packet_size);
    if (payload == NULL) {
        return 0U;
    }

    if (size < packet_size) {
        return 0U;
    }

    memcpy(data, payload, packet_size);
    packetbuf_release(pb);

    return packet_size;
}

bool packetbuf_drop(packetbuf_t *pb)
{
    uint16_t packet_size;

    packet_size = packetbuf_peek_size(pb);
    if (packet_size == 0U) {
        return false;
    }

    commit_read(pb, PACKETBUF_HEADER_SIZE + (uint32_t)packet_size);
    reset_cursors_if_empty(pb);

    return true;
}

void *packetbuf_reserve(packetbuf_t *pb, uint16_t size)
{
    uint32_t padding;
    uint32_t offset;

    OS_DIAG_ASSERT(pb != NULL);
    OS_DIAG_ASSERT(pb->buffer != NULL);

    if (pb->put_claim_size != 0U) {
        return NULL;
    }

    reset_cursors_if_empty(pb);

    if (!reserve_layout(pb, size, &padding, &offset)) {
        return NULL;
    }

    pb->put_claim_padding = padding;
    pb->put_claim_size = size;

    return &pb->buffer[offset + PACKETBUF_HEADER_SIZE];
}

int packetbuf_commit(packetbuf_t *pb, uint16_t size)
{
    uint32_t header_offset;

    OS_DIAG_ASSERT(pb != NULL);
    OS_DIAG_ASSERT(pb->buffer != NULL);

    if (pb->put_claim_size == 0U) {
        return -1;
    }

    if (size == 0U) {
        pb->put_claim_padding = 0U;
        pb->put_claim_size = 0U;
        return 0;
    }

    if (size > pb->put_claim_size) {
        return -1;
    }

    if (pb->put_claim_padding >= PACKETBUF_HEADER_SIZE) {
        write_u16(&pb->buffer[write_offset(pb)], 0U);
    }

    header_offset = (pb->put_claim_padding == 0U) ? write_offset(pb) : 0U;
    write_u16(&pb->buffer[header_offset], size);

    commit_write(pb, pb->put_claim_padding + PACKETBUF_HEADER_SIZE + size);

    pb->put_claim_padding = 0U;
    pb->put_claim_size = 0U;

    return 0;
}

const void *packetbuf_claim(packetbuf_t *pb, uint16_t *size)
{
    uint32_t offset;
    uint16_t packet_size;

    OS_DIAG_ASSERT(pb != NULL);
    OS_DIAG_ASSERT(pb->buffer != NULL);
    OS_DIAG_ASSERT(size != NULL);

    skip_internal_padding(pb);
    if (packetbuf_is_empty(pb)) {
        *size = 0U;
        return NULL;
    }

    offset = read_offset(pb);
    packet_size = read_u16(&pb->buffer[offset]);
    OS_DIAG_ASSERT(packet_size != 0U);
    OS_DIAG_ASSERT((uint32_t)packet_size + PACKETBUF_HEADER_SIZE <=
                     packetbuf_size(pb));

    *size = packet_size;
    return &pb->buffer[offset + PACKETBUF_HEADER_SIZE];
}

void packetbuf_release(packetbuf_t *pb)
{
    (void)packetbuf_drop(pb);
}
