#define SLAB_ALLOC_FAILED_HOOK_ENABLE 0

#include "tests/test.h"
#include "memory/slab.h"

typedef struct test_slab_item {
    uint32_t id;
    uint32_t flags;
} test_slab_item_t;

typedef struct test_slab_aligned_item {
    uint8_t prefix;
    uint64_t value;
} test_slab_aligned_item_t;

static bool test_slab_ptr_is_one_of(const void *ptr,
                                    const void *a,
                                    const void *b,
                                    const void *c)
{
    return (ptr == a) || (ptr == b) || (ptr == c);
}

static void test_slab_init_sets_metadata_and_empty_state(void)
{
    slab_t slab;
    slab_storage(storage, test_slab_item_t, 3);

    slab_init(&slab, storage, "items");

    TEST_EQ_PTR("items", slab_name(&slab));
    TEST_EQ_PTR(storage, slab.buffer);
    TEST_EQ_SIZE(sizeof(storage[0]), slab.block_size);
    TEST_EQ_SIZE(3U, slab.block_count);
    TEST_EQ_SIZE(3U, slab.free_count);
    TEST_EQ_SIZE(3U, slab_capacity(&slab));
    TEST_EQ_SIZE(3U, slab_free_count(&slab));
    TEST_EQ_SIZE(0U, slab_used_count(&slab));
    TEST_ASSERT(slab_is_empty(&slab));
    TEST_ASSERT(!slab_is_full(&slab));
}

static void test_slab_alloc_returns_storage_blocks_in_initial_order(void)
{
    slab_t slab;
    slab_storage(storage, test_slab_item_t, 3);
    test_slab_item_t *first;
    test_slab_item_t *second;
    test_slab_item_t *third;

    slab_init(&slab, storage, "items");

    first = (test_slab_item_t *)slab_alloc(&slab);
    TEST_EQ_PTR(&storage[0].object_, first);
    TEST_EQ_SIZE(2U, slab_free_count(&slab));
    TEST_EQ_SIZE(1U, slab_used_count(&slab));

    second = (test_slab_item_t *)slab_alloc(&slab);
    TEST_EQ_PTR(&storage[1].object_, second);
    TEST_EQ_SIZE(1U, slab_free_count(&slab));
    TEST_EQ_SIZE(2U, slab_used_count(&slab));

    third = (test_slab_item_t *)slab_alloc(&slab);
    TEST_EQ_PTR(&storage[2].object_, third);
    TEST_EQ_SIZE(0U, slab_free_count(&slab));
    TEST_EQ_SIZE(3U, slab_used_count(&slab));
    TEST_ASSERT(!slab_is_empty(&slab));
    TEST_ASSERT(slab_is_full(&slab));
}

static void test_slab_exhaustion_returns_null_when_hook_is_disabled(void)
{
    slab_t slab;
    slab_storage(storage, test_slab_item_t, 1);
    void *block;

    TEST_EQ_U32(0U, SLAB_ALLOC_FAILED_HOOK_ENABLE);

    slab_init(&slab, storage, "single");

    block = slab_alloc(&slab);

    TEST_EQ_PTR(&storage[0].object_, block);
    TEST_EQ_PTR(NULL, slab_alloc(&slab));
    TEST_EQ_SIZE(0U, slab_free_count(&slab));
    TEST_EQ_SIZE(1U, slab_used_count(&slab));
    TEST_ASSERT(slab_is_full(&slab));
}

static void test_slab_free_makes_block_available_for_reallocation(void)
{
    slab_t slab;
    slab_storage(storage, test_slab_item_t, 3);
    test_slab_item_t *first;
    test_slab_item_t *second;
    test_slab_item_t *third;
    test_slab_item_t *again;

    slab_init(&slab, storage, "items");
    first = (test_slab_item_t *)slab_alloc(&slab);
    second = (test_slab_item_t *)slab_alloc(&slab);
    third = (test_slab_item_t *)slab_alloc(&slab);

    slab_free(&slab, second);
    again = (test_slab_item_t *)slab_alloc(&slab);

    TEST_EQ_PTR(second, again);
    TEST_EQ_PTR(&storage[0].object_, first);
    TEST_EQ_PTR(&storage[2].object_, third);
    TEST_EQ_SIZE(0U, slab_free_count(&slab));
    TEST_EQ_SIZE(3U, slab_used_count(&slab));
}

static void test_slab_free_order_is_lifo_for_reused_blocks(void)
{
    slab_t slab;
    slab_storage(storage, test_slab_item_t, 3);
    test_slab_item_t *first;
    test_slab_item_t *second;
    test_slab_item_t *third;

    slab_init(&slab, storage, "items");
    first = (test_slab_item_t *)slab_alloc(&slab);
    second = (test_slab_item_t *)slab_alloc(&slab);
    third = (test_slab_item_t *)slab_alloc(&slab);

    slab_free(&slab, first);
    slab_free(&slab, third);
    slab_free(&slab, second);

    TEST_EQ_SIZE(3U, slab_free_count(&slab));
    TEST_EQ_SIZE(0U, slab_used_count(&slab));
    TEST_ASSERT(slab_is_empty(&slab));
    TEST_ASSERT(!slab_is_full(&slab));

    TEST_EQ_PTR(second, slab_alloc(&slab));
    TEST_EQ_PTR(third, slab_alloc(&slab));
    TEST_EQ_PTR(first, slab_alloc(&slab));
    TEST_EQ_PTR(NULL, slab_alloc(&slab));
}

static void test_slab_all_blocks_can_be_reallocated_after_freeing_all(void)
{
    slab_t slab;
    slab_storage(storage, test_slab_item_t, 4);
    test_slab_item_t *blocks[4];
    size_t i;

    slab_init(&slab, storage, "items");

    for (i = 0U; i < 4U; i++) {
        blocks[i] = (test_slab_item_t *)slab_alloc(&slab);
        TEST_REQUIRE(blocks[i] != NULL);
        blocks[i]->id = (uint32_t)i;
        blocks[i]->flags = (uint32_t)(i + 10U);
    }

    slab_free(&slab, blocks[1]);
    slab_free(&slab, blocks[3]);
    slab_free(&slab, blocks[0]);
    slab_free(&slab, blocks[2]);

    TEST_ASSERT(slab_is_empty(&slab));
    TEST_EQ_SIZE(4U, slab_free_count(&slab));

    TEST_EQ_PTR(blocks[2], slab_alloc(&slab));
    TEST_EQ_PTR(blocks[0], slab_alloc(&slab));
    TEST_EQ_PTR(blocks[3], slab_alloc(&slab));
    TEST_EQ_PTR(blocks[1], slab_alloc(&slab));
    TEST_ASSERT(slab_is_full(&slab));
}

static void test_slab_storage_preserves_size_alignment_and_membership(void)
{
    slab_t slab;
    slab_storage(storage, test_slab_aligned_item_t, 3);
    test_slab_aligned_item_t *first;
    test_slab_aligned_item_t *second;
    test_slab_aligned_item_t *third;

    slab_init(&slab, storage, "aligned");

    TEST_ASSERT(sizeof(storage[0]) >= sizeof(test_slab_aligned_item_t));
    TEST_ASSERT(sizeof(storage[0]) >= sizeof(slist_node_t));
    TEST_EQ_SIZE(sizeof(storage[0]), slab.block_size);

    first = (test_slab_aligned_item_t *)slab_alloc(&slab);
    second = (test_slab_aligned_item_t *)slab_alloc(&slab);
    third = (test_slab_aligned_item_t *)slab_alloc(&slab);

    TEST_ASSERT(test_slab_ptr_is_one_of(first,
                                        &storage[0].object_,
                                        &storage[1].object_,
                                        &storage[2].object_));
    TEST_ASSERT(test_slab_ptr_is_one_of(second,
                                        &storage[0].object_,
                                        &storage[1].object_,
                                        &storage[2].object_));
    TEST_ASSERT(test_slab_ptr_is_one_of(third,
                                        &storage[0].object_,
                                        &storage[1].object_,
                                        &storage[2].object_));

    TEST_EQ_SIZE(0U, (uintptr_t)first % _Alignof(test_slab_aligned_item_t));
    TEST_EQ_SIZE(0U, (uintptr_t)second % _Alignof(test_slab_aligned_item_t));
    TEST_EQ_SIZE(0U, (uintptr_t)third % _Alignof(test_slab_aligned_item_t));

    first->prefix = 1U;
    second->value = 2U;
    third->value = first->prefix + second->value;

    TEST_EQ_U32(3U, (uint32_t)third->value);
}

void test_slab_run(void)
{
    TEST_RUN(test_slab_init_sets_metadata_and_empty_state);
    TEST_RUN(test_slab_alloc_returns_storage_blocks_in_initial_order);
    TEST_RUN(test_slab_exhaustion_returns_null_when_hook_is_disabled);
    TEST_RUN(test_slab_free_makes_block_available_for_reallocation);
    TEST_RUN(test_slab_free_order_is_lifo_for_reused_blocks);
    TEST_RUN(test_slab_all_blocks_can_be_reallocated_after_freeing_all);
    TEST_RUN(test_slab_storage_preserves_size_alignment_and_membership);
}
