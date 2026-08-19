#include "slab.h"

#include <stdint.h>

#if OS_DIAGNOSTIC_ENABLE
/* Check if the block belongs to the slab and points to a valid block boundary. */
static bool slab_contains_block_(const slab_t *slab, const void *block)
{
    uintptr_t begin = (uintptr_t)slab->buffer;
    uintptr_t end = begin + (slab->block_size * slab->block_count);
    uintptr_t addr = (uintptr_t)block;

    return (addr >= begin) && (addr < end) &&
           (((addr - begin) % slab->block_size) == 0U);
}

static bool slab_block_is_free_(const slab_t *slab, const void *block)
{
    slist_node_t *node;

    slist_for_each(node, &slab->free_list) {
        if ((const void *)node == block) {
            return true;
        }
    }

    return false;
}

static void slab_check_(const slab_t *slab)
{
    OS_DIAG_ASSERT(slab != NULL);
    OS_DIAG_ASSERT(slab->buffer != NULL);
    OS_DIAG_ASSERT(slab->block_size >= sizeof(slist_node_t));
    OS_DIAG_ASSERT(slab->block_count > 0U);
    OS_DIAG_ASSERT(slab->free_count <= slab->block_count);
}
#else
static void slab_check_(const slab_t *slab)
{
    (void)slab;
}
#endif

void slab_init_(slab_t *slab, const char *name, void *buffer, size_t block_size, size_t block_count)
{
    uint8_t *block;
    size_t i;

    OS_DIAG_ASSERT(slab != NULL);
    OS_DIAG_ASSERT(buffer != NULL);
    OS_DIAG_ASSERT(block_size >= sizeof(slist_node_t));
    OS_DIAG_ASSERT(block_count > 0U);

    slab->name = name;
    slab->buffer = buffer;
    slab->block_size = block_size;
    slab->block_count = block_count;
    slab->free_count = block_count;
    slist_init(&slab->free_list);

    block = (uint8_t *)buffer;
    for (i = 0; i < block_count; i++) {
        slist_push_back(&slab->free_list, (slist_node_t *)block);
        block += block_size;
    }
}

void *slab_alloc(slab_t *slab)
{
    slist_node_t *node;

    slab_check_(slab);

    node = slist_pop_front(&slab->free_list);
    if (node == NULL) {
#if SLAB_ALLOC_FAILED_HOOK_ENABLE
        slab_alloc_failed(slab);
#endif
        return NULL;
    }

    slab->free_count--;
    return node;
}

void slab_free(slab_t *slab, void *block)
{
    slab_check_(slab);
    OS_DIAG_ASSERT(block != NULL);
#if OS_DIAGNOSTIC_ENABLE
    OS_DIAG_ASSERT(slab_contains_block_(slab, block));
    OS_DIAG_ASSERT(!slab_block_is_free_(slab, block));
#endif

    slist_push_front(&slab->free_list, (slist_node_t *)block);
    slab->free_count++;
}

__NO_RETURN __WEAK void slab_alloc_failed(const slab_t *slab)
{
    (void)slab;
    OS_ASSERT(false);

    for (;;) {
    }
}
