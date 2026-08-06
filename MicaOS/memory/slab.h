#ifndef SLAB_H
#define SLAB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include "common/compiler.h"
#include "common/assert.h"
#include "data_structure/slist.h"

/*
 * Fixed-size block/object pool.
 *
 * Usage:
 *   1. Use slab_storage() to declare storage for a fixed number of objects.
 *   2. Initialize the slab with slab_init() and provide a diagnostic name.
 *   3. Use slab_alloc() to allocate an object.
 *   4. Return objects with slab_free().
 *
 * Design notes:
 *   - The slab never allocates memory. Storage lifetime is owned by the caller.
 *   - Each slab manages one fixed-size object/block type.
 *   - Parameter validity is not checked by default; callers must satisfy API
 *     preconditions.
 *   - A freed object is used internally as a free-list node, so its previous
 *     contents are overwritten after slab_free().
 *
 * Debug:
 *   - Define SLAB_DIAGNOSTIC_ENABLE to 1 when debugging misuse; it enables
 *     ASSERT-based parameter, range, alignment, and double-free checks.
 *   - SLAB_ALLOC_FAILED_HOOK_ENABLE is enabled by default. When slab_alloc()
 *     runs out of objects, it calls slab_alloc_failed() so capacity problems
 *     are exposed early during development. Thus caller have no need to check
 *     if slab_alloc() returns NULL during development.
 *   - Define SLAB_ALLOC_FAILED_HOOK_ENABLE to 0 if slab_alloc() should return
 *     NULL on exhaustion.
 *   - slab_alloc_failed() is weak, so projects may override it to log the slab
 *     name, enter a fault handler, or halt in a project-specific way.
 *
 * Concurrency:
 *   This module does not provide any concurrency control. Callers in concurrent
 *   environments should protect the structure with appropriate synchronization.
 *
 * API quick reference:
 *   - Storage and initialization:
 *       slab_storage()
 *       slab_init()
 *
 *   - Allocation:
 *       slab_alloc()                allocate an object; exhaustion behavior is macro-controlled
 *       slab_free()
 *       slab_alloc_failed()         weak hook for fatal allocation failure
 *
 *   - State queries:
 *       slab_name()
 *       slab_capacity()
 *       slab_free_count()
 *       slab_used_count()
 *       slab_is_empty()
 *       slab_is_full()
 *
 * Examples:
 *   typedef struct task {
 *       int priority;
 *   } task_t;
 *
 *   static slab_storage(task_storage, task_t, 16);
 *   static slab_t task_slab;
 *
 *   slab_init(&task_slab, task_storage, "task");
 *
 *   task_t *task = (task_t *)slab_alloc(&task_slab);
 *   slab_free(&task_slab, task);
 *
 * Implementation:
 *   slab_storage() declares each block as a union of slist_node_t and the user
 *   object type. When a block is free, slab uses it as a free-list node. When
 *   the block is allocated, the caller uses the same storage as the object.
 *   slab_free() converts the object storage back to an slist_node_t and pushes
 *   it into the internal free list. This time-shares the block memory and also
 *   gives each block the required size and alignment for both roles.
 */

typedef struct slab {
    const char *name;
    void *buffer;
    size_t block_size;   /* Size in bytes of each block/object slot. */
    size_t block_count;  /* Total number of blocks managed by this slab. */
    size_t free_count;   /* Number of blocks currently available. */
    slist_t free_list;
} slab_t;

#ifndef SLAB_DIAGNOSTIC_ENABLE
#define SLAB_DIAGNOSTIC_ENABLE 0
#endif

#if SLAB_DIAGNOSTIC_ENABLE
#define SLAB_ASSERT(cond) ASSERT(cond)
#else
#define SLAB_ASSERT(cond) ((void)sizeof(cond))
#endif

#ifndef SLAB_ALLOC_FAILED_HOOK_ENABLE
#define SLAB_ALLOC_FAILED_HOOK_ENABLE 1
#endif

/*
 * Declare storage for slab.
 *
 * Example:
 *   static slab_storage(task_storage, task_t, 16);
 */
#define slab_storage(name, type, count) \
    union { slist_node_t node_; type object_; } name[(count)]

/*
 * Initialize slab from storage and use name as the slab diagnostic name.
 *
 * Example:
 *   slab_init(&task_slab, task_storage, "task");
 */
#define slab_init(slab, storage, name)                                       \
    slab_init_((slab), (name), (storage), sizeof((storage)[0]),              \
               sizeof(storage) / sizeof((storage)[0]))

void slab_init_(slab_t *slab,
                const char *name,
                void *buffer,
                size_t block_size,
                size_t block_count);

void *slab_alloc(slab_t *slab);
void slab_free(slab_t *slab, void *block);

__NO_RETURN void slab_alloc_failed(const slab_t *slab);

/* Return the diagnostic name associated with slab. */
static inline const char *slab_name(const slab_t *slab)
{
    SLAB_ASSERT(slab != NULL);
    return slab->name;
}

/* Return the total number of objects managed by slab. */
static inline size_t slab_capacity(const slab_t *slab)
{
    SLAB_ASSERT(slab != NULL);
    return slab->block_count;
}

/* Return the number of currently free objects. */
static inline size_t slab_free_count(const slab_t *slab)
{
    SLAB_ASSERT(slab != NULL);
    return slab->free_count;
}

/* Return the number of currently allocated objects. */
static inline size_t slab_used_count(const slab_t *slab)
{
    SLAB_ASSERT(slab != NULL);
    return slab->block_count - slab->free_count;
}

/* Return true when every object is free. */
static inline bool slab_is_empty(const slab_t *slab)
{
    SLAB_ASSERT(slab != NULL);
    return slab->free_count == slab->block_count;
}

/* Return true when no object is available for allocation. */
static inline bool slab_is_full(const slab_t *slab)
{
    SLAB_ASSERT(slab != NULL);
    return slab->free_count == 0U;
}

#ifdef __cplusplus
}
#endif

#endif /* SLAB_H */
