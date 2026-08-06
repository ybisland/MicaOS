#include "tests/test.h"
#include "data_structure/packetbuf.h"

#include <string.h>

static void test_packetbuf_expect_bytes(const uint8_t *expected,
                                        const uint8_t *actual,
                                        uint16_t size)
{
    TEST_ASSERT(memcmp(expected, actual, size) == 0);
}

static void test_packetbuf_init_makes_empty_buffer(void)
{
    uint8_t storage[16];
    packetbuf_t pb;
    uint8_t out[4] = { 0U };

    packetbuf_init(&pb, storage, sizeof(storage));

    TEST_EQ_U32(sizeof(storage), packetbuf_capacity(&pb));
    TEST_EQ_U32(0U, packetbuf_size(&pb));
    TEST_EQ_U32(sizeof(storage), packetbuf_space(&pb));
    TEST_ASSERT(packetbuf_is_empty(&pb));
    TEST_ASSERT(!packetbuf_is_full(&pb));
    TEST_EQ_U32(0U, packetbuf_peek_size(&pb));
    TEST_EQ_U32(0U, packetbuf_get(&pb, out, sizeof(out)));
    TEST_ASSERT(!packetbuf_drop(&pb));
}

static void test_packetbuf_static_init_makes_empty_buffer(void)
{
    uint8_t storage[8];
    packetbuf_t pb = packetbuf_static_init(storage, sizeof(storage));

    TEST_EQ_U32(sizeof(storage), packetbuf_capacity(&pb));
    TEST_EQ_U32(0U, packetbuf_size(&pb));
    TEST_EQ_U32(sizeof(storage), packetbuf_space(&pb));
    TEST_ASSERT(packetbuf_is_empty(&pb));
    TEST_ASSERT(!packetbuf_is_full(&pb));
    TEST_EQ_U32(0U, packetbuf_peek_size(&pb));
}

static void test_packetbuf_reset_drops_all_packets(void)
{
    uint8_t storage[16];
    const uint8_t first[] = { 1U, 2U, 3U };
    const uint8_t second[] = { 4U, 5U };
    packetbuf_t pb;

    packetbuf_init(&pb, storage, sizeof(storage));
    TEST_ASSERT(packetbuf_put(&pb, first, sizeof(first)));
    TEST_ASSERT(packetbuf_put(&pb, second, sizeof(second)));

    packetbuf_reset(&pb);

    TEST_ASSERT(packetbuf_is_empty(&pb));
    TEST_EQ_U32(0U, packetbuf_size(&pb));
    TEST_EQ_U32(sizeof(storage), packetbuf_space(&pb));
    TEST_EQ_U32(0U, packetbuf_peek_size(&pb));
}

static void test_packetbuf_put_get_preserves_fifo_packet_boundaries(void)
{
    uint8_t storage[32];
    const uint8_t first[] = { 0x11U };
    const uint8_t second[] = { 0x22U, 0x23U, 0x24U };
    const uint8_t third[] = { 0x31U, 0x32U, 0x33U, 0x34U, 0x35U, 0x36U };
    uint8_t out[8] = { 0U };
    packetbuf_t pb;

    packetbuf_init(&pb, storage, sizeof(storage));

    TEST_ASSERT(packetbuf_put(&pb, first, sizeof(first)));
    TEST_ASSERT(packetbuf_put(&pb, second, sizeof(second)));
    TEST_ASSERT(packetbuf_put(&pb, third, sizeof(third)));

    TEST_EQ_U32(16U, packetbuf_size(&pb));
    TEST_EQ_U32(sizeof(first), packetbuf_peek_size(&pb));
    TEST_EQ_U32(sizeof(first), packetbuf_get(&pb, out, sizeof(out)));
    test_packetbuf_expect_bytes(first, out, sizeof(first));

    memset(out, 0, sizeof(out));
    TEST_EQ_U32(sizeof(second), packetbuf_peek_size(&pb));
    TEST_EQ_U32(sizeof(second), packetbuf_get(&pb, out, sizeof(out)));
    test_packetbuf_expect_bytes(second, out, sizeof(second));

    memset(out, 0, sizeof(out));
    TEST_EQ_U32(sizeof(third), packetbuf_peek_size(&pb));
    TEST_EQ_U32(sizeof(third), packetbuf_get(&pb, out, sizeof(out)));
    test_packetbuf_expect_bytes(third, out, sizeof(third));

    TEST_ASSERT(packetbuf_is_empty(&pb));
}

static void test_packetbuf_get_with_small_output_leaves_packet_available(void)
{
    uint8_t storage[16];
    const uint8_t packet[] = { 7U, 8U, 9U, 10U };
    uint8_t too_small[3] = { 0U };
    uint8_t out[4] = { 0U };
    packetbuf_t pb;

    packetbuf_init(&pb, storage, sizeof(storage));
    TEST_ASSERT(packetbuf_put(&pb, packet, sizeof(packet)));

    TEST_EQ_U32(0U, packetbuf_get(&pb, too_small, sizeof(too_small)));
    TEST_EQ_U32(sizeof(packet), packetbuf_peek_size(&pb));
    TEST_ASSERT(!packetbuf_is_empty(&pb));

    TEST_EQ_U32(sizeof(packet), packetbuf_get(&pb, out, sizeof(out)));
    test_packetbuf_expect_bytes(packet, out, sizeof(packet));
    TEST_ASSERT(packetbuf_is_empty(&pb));
}

static void test_packetbuf_drop_consumes_one_packet_without_copying(void)
{
    uint8_t storage[16];
    const uint8_t first[] = { 1U, 2U };
    const uint8_t second[] = { 3U, 4U, 5U };
    uint8_t out[4] = { 0U };
    packetbuf_t pb;

    packetbuf_init(&pb, storage, sizeof(storage));
    TEST_ASSERT(packetbuf_put(&pb, first, sizeof(first)));
    TEST_ASSERT(packetbuf_put(&pb, second, sizeof(second)));

    TEST_ASSERT(packetbuf_drop(&pb));
    TEST_EQ_U32(sizeof(second), packetbuf_peek_size(&pb));
    TEST_EQ_U32(sizeof(second), packetbuf_get(&pb, out, sizeof(out)));
    test_packetbuf_expect_bytes(second, out, sizeof(second));
    TEST_ASSERT(!packetbuf_drop(&pb));
}

static void test_packetbuf_rejects_zero_length_and_oversized_packets(void)
{
    uint8_t storage[8];
    const uint8_t packet[] = { 1U, 2U, 3U, 4U, 5U, 6U, 7U };
    packetbuf_t pb;

    packetbuf_init(&pb, storage, sizeof(storage));

    TEST_ASSERT(!packetbuf_put(&pb, packet, 0U));
    TEST_EQ_PTR(NULL, packetbuf_reserve(&pb, 0U));
    TEST_ASSERT(!packetbuf_put(&pb, packet, sizeof(packet)));
    TEST_EQ_PTR(NULL, packetbuf_reserve(&pb, sizeof(packet)));
    TEST_ASSERT(packetbuf_is_empty(&pb));
}

static void test_packetbuf_reports_full_when_minimum_packet_cannot_fit(void)
{
    uint8_t storage[5];
    const uint8_t full_packet[] = { 0xA0U, 0xA1U, 0xA2U };
    const uint8_t extra[] = { 0xEEU };
    uint8_t out[3] = { 0U };
    packetbuf_t pb;

    packetbuf_init(&pb, storage, sizeof(storage));

    TEST_ASSERT(packetbuf_put(&pb, full_packet, sizeof(full_packet)));
    TEST_EQ_U32(sizeof(storage), packetbuf_size(&pb));
    TEST_EQ_U32(0U, packetbuf_space(&pb));
    TEST_ASSERT(packetbuf_is_full(&pb));
    TEST_ASSERT(!packetbuf_put(&pb, extra, sizeof(extra)));

    TEST_EQ_U32(sizeof(full_packet), packetbuf_get(&pb, out, sizeof(out)));
    test_packetbuf_expect_bytes(full_packet, out, sizeof(full_packet));
    TEST_ASSERT(packetbuf_is_empty(&pb));
    TEST_ASSERT(!packetbuf_is_full(&pb));
}

static void test_packetbuf_rejects_packet_when_free_space_is_insufficient(void)
{
    uint8_t storage[8];
    const uint8_t first[] = { 1U, 2U, 3U };
    const uint8_t second[] = { 4U, 5U };
    uint8_t out[3] = { 0U };
    packetbuf_t pb;

    packetbuf_init(&pb, storage, sizeof(storage));

    TEST_ASSERT(packetbuf_put(&pb, first, sizeof(first)));
    TEST_ASSERT(!packetbuf_put(&pb, second, sizeof(second)));
    TEST_EQ_U32(sizeof(first), packetbuf_peek_size(&pb));
    TEST_EQ_U32(sizeof(first), packetbuf_get(&pb, out, sizeof(out)));
    test_packetbuf_expect_bytes(first, out, sizeof(first));
    TEST_ASSERT(packetbuf_is_empty(&pb));
}

static void test_packetbuf_reserve_commit_publishes_zero_copy_packet(void)
{
    uint8_t storage[16];
    const uint8_t packet[] = { 9U, 8U, 7U };
    uint8_t out[4] = { 0U };
    uint8_t *payload;
    packetbuf_t pb;

    packetbuf_init(&pb, storage, sizeof(storage));

    payload = packetbuf_reserve(&pb, 5U);
    TEST_REQUIRE(payload != NULL);
    TEST_EQ_PTR(NULL, packetbuf_reserve(&pb, 1U));
    memcpy(payload, packet, sizeof(packet));

    TEST_ASSERT(packetbuf_is_empty(&pb));
    TEST_EQ_U32(0U, packetbuf_commit(&pb, sizeof(packet)));

    TEST_EQ_U32(sizeof(packet), packetbuf_peek_size(&pb));
    TEST_EQ_U32(sizeof(packet), packetbuf_get(&pb, out, sizeof(out)));
    test_packetbuf_expect_bytes(packet, out, sizeof(packet));
    TEST_ASSERT(packetbuf_is_empty(&pb));
}

static void test_packetbuf_commit_zero_cancels_reserve(void)
{
    uint8_t storage[16];
    uint8_t *payload;
    packetbuf_t pb;

    packetbuf_init(&pb, storage, sizeof(storage));

    payload = packetbuf_reserve(&pb, 4U);
    TEST_REQUIRE(payload != NULL);
    payload[0] = 0x55U;

    TEST_EQ_U32(0U, packetbuf_commit(&pb, 0U));
    TEST_ASSERT(packetbuf_is_empty(&pb));
    TEST_EQ_U32(0U, packetbuf_peek_size(&pb));
}

static void test_packetbuf_claim_repeats_until_release_consumes_packet(void)
{
    uint8_t storage[16];
    const uint8_t first[] = { 0x10U, 0x11U };
    const uint8_t second[] = { 0x20U, 0x21U, 0x22U };
    const uint8_t *claimed;
    const uint8_t *claimed_again;
    uint16_t size = 0U;
    packetbuf_t pb;

    packetbuf_init(&pb, storage, sizeof(storage));
    TEST_ASSERT(packetbuf_put(&pb, first, sizeof(first)));
    TEST_ASSERT(packetbuf_put(&pb, second, sizeof(second)));

    claimed = packetbuf_claim(&pb, &size);
    TEST_REQUIRE(claimed != NULL);
    TEST_EQ_U32(sizeof(first), size);
    test_packetbuf_expect_bytes(first, claimed, size);

    claimed_again = packetbuf_claim(&pb, &size);
    TEST_EQ_PTR(claimed, claimed_again);
    TEST_EQ_U32(sizeof(first), size);

    packetbuf_release(&pb);
    claimed = packetbuf_claim(&pb, &size);
    TEST_REQUIRE(claimed != NULL);
    TEST_EQ_U32(sizeof(second), size);
    test_packetbuf_expect_bytes(second, claimed, size);

    packetbuf_release(&pb);
    claimed = packetbuf_claim(&pb, &size);
    TEST_EQ_PTR(NULL, claimed);
    TEST_EQ_U32(0U, size);
}

static void test_packetbuf_wrap_padding_keeps_packets_in_fifo_order(void)
{
    uint8_t storage[12];
    const uint8_t first[] = { 1U, 2U, 3U, 4U };
    const uint8_t second[] = { 5U, 6U };
    const uint8_t wrapped[] = { 7U, 8U, 9U, 10U };
    uint8_t out[4] = { 0U };
    packetbuf_t pb;

    packetbuf_init(&pb, storage, sizeof(storage));
    TEST_ASSERT(packetbuf_put(&pb, first, sizeof(first)));
    TEST_ASSERT(packetbuf_put(&pb, second, sizeof(second)));

    TEST_EQ_U32(sizeof(first), packetbuf_get(&pb, out, sizeof(out)));
    test_packetbuf_expect_bytes(first, out, sizeof(first));

    TEST_ASSERT(packetbuf_put(&pb, wrapped, sizeof(wrapped)));
    TEST_ASSERT(packetbuf_is_full(&pb));

    memset(out, 0, sizeof(out));
    TEST_EQ_U32(sizeof(second), packetbuf_get(&pb, out, sizeof(out)));
    test_packetbuf_expect_bytes(second, out, sizeof(second));

    memset(out, 0, sizeof(out));
    TEST_EQ_U32(sizeof(wrapped), packetbuf_peek_size(&pb));
    TEST_EQ_U32(sizeof(wrapped), packetbuf_get(&pb, out, sizeof(out)));
    test_packetbuf_expect_bytes(wrapped, out, sizeof(wrapped));
    TEST_ASSERT(packetbuf_is_empty(&pb));
}

void test_packetbuf_run(void)
{
    TEST_RUN(test_packetbuf_init_makes_empty_buffer);
    TEST_RUN(test_packetbuf_static_init_makes_empty_buffer);
    TEST_RUN(test_packetbuf_reset_drops_all_packets);
    TEST_RUN(test_packetbuf_put_get_preserves_fifo_packet_boundaries);
    TEST_RUN(test_packetbuf_get_with_small_output_leaves_packet_available);
    TEST_RUN(test_packetbuf_drop_consumes_one_packet_without_copying);
    TEST_RUN(test_packetbuf_rejects_zero_length_and_oversized_packets);
    TEST_RUN(test_packetbuf_reports_full_when_minimum_packet_cannot_fit);
    TEST_RUN(test_packetbuf_rejects_packet_when_free_space_is_insufficient);
    TEST_RUN(test_packetbuf_reserve_commit_publishes_zero_copy_packet);
    TEST_RUN(test_packetbuf_commit_zero_cancels_reserve);
    TEST_RUN(test_packetbuf_claim_repeats_until_release_consumes_packet);
    TEST_RUN(test_packetbuf_wrap_padding_keeps_packets_in_fifo_order);
}
