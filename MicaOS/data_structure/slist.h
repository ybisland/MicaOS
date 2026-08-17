#ifndef SLIST_H
#define SLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include "common/assert.h"

/*
 * Intrusive singly linked list.
 *
 * Usage:
 *   1. Define an slist_t as the list object. It stores only the front and back
 *      node pointers; user data lives in owner objects.
 *   2. Embed one slist_node_t in each owner object for each list membership.
 *      If an object may be linked into two lists at the same time, give it two
 *      independent slist_node_t members.
 *   3. Initialize list objects with slist_init(), or use slist_static_init()
 *      for static-storage lists. Nodes can be cleared with slist_node_init().
 *   4. Use slist_push_front()/slist_pop_front() for stack-style access.
 *      Use slist_push_back()/slist_pop_front() for FIFO queue-style access.
 *   5. Use slist_insert_after() when inserting relative to an existing node.
 *      Use slist_remove_after() when the previous node is known, or
 *      slist_remove() when only the target node is known.
 *   6. Recover the owner object with slist_entry(), or iterate owner objects
 *      directly with slist_for_each_entry().
 *
 * Usage notes:
 *   - This module requires C99 or later with GNU extensions (uses __typeof__ to
 *     infer types).
 *     Recommended: C11 for better static_assert messages.
 *   - The list never allocates memory. Node lifetime is owned by the caller.
 *   - Parameter validity is not checked by default; callers must satisfy API
 *     preconditions.
 *   - Define SLIST_DIAGNOSTIC_ENABLE to 1 when debugging list misuse; it enables
 *     ASSERT-based parameter and head/tail consistency checks.
 *   - A singly linked node does not know its previous node. Removing a known
 *     node with slist_remove() is O(n); prefer slist_remove_after() when the
 *     previous node is already available.
 *
 * Concurrency:
 *   This module does not provide any concurrency control. Callers in multi-threaded
 *   environments should protect the structure with appropriate synchronization
 *   mechanisms (e.g., mutexes, semaphores) to ensure thread safety.
 *
 * API quick reference:
 *   - Initialization:
 *       slist_static_init()
 *       slist_init()
 *       slist_node_init()
 *
 *   - Stack/FIFO operations:
 *       slist_push_front()
 *       slist_push_back()
 *       slist_peek_front()
 *       slist_peek_back()
 *       slist_pop_front()
 *
 *   - Position-based operations:
 *       slist_insert_after()
 *       slist_remove_after()         O(1), remove front when prev is NULL
 *       slist_remove()               O(n), prefer remove_after on hot paths
 *
 *   - State queries:
 *       slist_empty()                return true when a list contains no nodes
 *       slist_has_one_node()         return true when a list contains exactly one node
 *       slist_has_multiple_nodes()   return true when a list contains two or more nodes
 *       slist_is_head()              return true when node is the front node of a list
 *       slist_is_tail()              return true when node is the back node of a list
 *       slist_peek_next()            return the next node, or NULL at the end
 *       slist_count()                O(n), prefer caller-maintained counters on hot paths
 *
 *   - Owner recovery and iteration:
 *       slist_entry()
 *       slist_first_entry()
 *       slist_last_entry()
 *       slist_for_each()             iterate over raw slist_node_t nodes; deleting pos is not allowed
 *       slist_for_each_safe()        iterate over raw nodes while allowing deletion of pos
 *       slist_for_each_entry()       iterate over owner objects; deleting pos is not allowed
 *       slist_for_each_entry_safe()  iterate over owner objects while allowing deletion of pos
 *
 * Examples:
 *   struct block {
 *       slist_node_t link;
 *   };
 *
 *   slist_t free_list;
 *   struct block block;
 *
 *   slist_init(&free_list);
 *   slist_node_init(&block.link);
 *   slist_push_back(&free_list, &block.link);
 *
 *   slist_node_t *node = slist_pop_front(&free_list);
 *   if (node != NULL) {
 *       struct block *free_block = slist_entry(node, struct block, link);
 *       (void)free_block;
 *   }
 *
 *   struct block *pos, *next;
 *   slist_for_each_entry_safe(pos, next, &free_list, link) {
 *       slist_remove(&free_list, &pos->link);
 *   }
 *
 *   static slist_t static_free_list = slist_static_init();
 */

typedef struct slist_node {
    struct slist_node *next;
} slist_node_t;

typedef struct slist {
    slist_node_t *head;
    slist_node_t *tail;
} slist_t;

#ifndef SLIST_DIAGNOSTIC_ENABLE
#define SLIST_DIAGNOSTIC_ENABLE 0
#endif

#if SLIST_DIAGNOSTIC_ENABLE
#define SLIST_ASSERT(cond) ASSERT(cond)
#else
#define SLIST_ASSERT(cond) ((void)sizeof(cond))
#endif

/*
 * Static initializer for an empty list.
 *
 * Example:
 *   static slist_t free_list = slist_static_init();
 */
#define slist_static_init() { NULL, NULL }

/*
 * Runtime initializer for an empty list.
 *
 * Example:
 *   slist_t free_list;
 *
 *   slist_init(&free_list);
 */
static inline void slist_init(slist_t *list)
{
    SLIST_ASSERT(list != NULL);
    list->head = NULL;
    list->tail = NULL;
}

/*
 * Clear a standalone node.
 *
 * A singly linked tail node also has next == NULL, so this state alone cannot
 * prove whether a node is linked. Callers must still avoid inserting the same
 * node into more than one list at the same time.
 */
static inline void slist_node_init(slist_node_t *node)
{
    SLIST_ASSERT(node != NULL);
    node->next = NULL;
}

#if SLIST_DIAGNOSTIC_ENABLE
/* Validate basic head/tail invariants. */
static inline void slist_diagnostic_check(const slist_t *list)
{
    SLIST_ASSERT(list != NULL);
    SLIST_ASSERT((list->head == NULL) == (list->tail == NULL));
    if (list->tail != NULL) {
        SLIST_ASSERT(list->tail->next == NULL);
    }
}
#else
static inline void slist_diagnostic_check(const slist_t *list)
{
    (void)list;
}
#endif

/* Return true when list contains no user nodes. */
static inline bool slist_empty(const slist_t *list)
{
    SLIST_ASSERT(list != NULL);
    return list->head == NULL;
}

/* Return true when exactly one user node is linked in list. */
static inline bool slist_has_one_node(const slist_t *list)
{
    SLIST_ASSERT(list != NULL);
    return (list->head != NULL) && (list->head == list->tail);
}

/* Return true when two or more user nodes are linked in list. */
static inline bool slist_has_multiple_nodes(const slist_t *list)
{
    SLIST_ASSERT(list != NULL);
    return (list->head != NULL) && (list->head != list->tail);
}

/*
 * Return true when node is the front node of list.
 */
static inline bool slist_is_head(const slist_t *list, const slist_node_t *node)
{
    SLIST_ASSERT(list != NULL);
    SLIST_ASSERT(node != NULL);
    return list->head == node;
}

/*
 * Return true when node is the back node of list.
 */
static inline bool slist_is_tail(const slist_t *list, const slist_node_t *node)
{
    SLIST_ASSERT(list != NULL);
    SLIST_ASSERT(node != NULL);
    return list->tail == node;
}

/*
 * Return the front node without removing it.
 *
 * Return NULL when list is empty.
 */
static inline slist_node_t *slist_peek_front(const slist_t *list)
{
    SLIST_ASSERT(list != NULL);
    return list->head;
}

/*
 * Return the back node without removing it.
 *
 * Return NULL when list is empty.
 */
static inline slist_node_t *slist_peek_back(const slist_t *list)
{
    SLIST_ASSERT(list != NULL);
    return list->tail;
}

/*
 * Return the node after node.
 *
 * Return NULL when node is NULL or when node is the back node.
 */
static inline slist_node_t *slist_peek_next(const slist_node_t *node)
{
    return node == NULL ? NULL : node->next;
}

/*
 * Push new_node to the front of list.
 *
 * Repeated calls create LIFO order. new_node must not already be linked in
 * another list.
 */
static inline void slist_push_front(slist_t *list, slist_node_t *new_node)
{
    SLIST_ASSERT(list != NULL);
    SLIST_ASSERT(new_node != NULL);
    slist_diagnostic_check(list);

    new_node->next = list->head;
    list->head = new_node;
    if (list->tail == NULL) {
        list->tail = new_node;
    }
}

/*
 * Push new_node to the back of list.
 *
 * Repeated calls create FIFO order when paired with slist_pop_front().
 * new_node must not already be linked in another list.
 */
static inline void slist_push_back(slist_t *list, slist_node_t *new_node)
{
    SLIST_ASSERT(list != NULL);
    SLIST_ASSERT(new_node != NULL);
    slist_diagnostic_check(list);

    new_node->next = NULL;
    if (list->tail == NULL) {
        list->head = new_node;
    } else {
        list->tail->next = new_node;
    }
    list->tail = new_node;
}

/*
 * Insert new_node right after pos.
 *
 * pos must be a node already linked in list. new_node must not already be
 * linked in another list.
 */
static inline void slist_insert_after(slist_t *list,
                                      slist_node_t *pos,
                                      slist_node_t *new_node)
{
    SLIST_ASSERT(list != NULL);
    SLIST_ASSERT(pos != NULL);
    SLIST_ASSERT(new_node != NULL);
    SLIST_ASSERT(pos != new_node);
    slist_diagnostic_check(list);

    new_node->next = pos->next;
    pos->next = new_node;
    if (list->tail == pos) {
        list->tail = new_node;
    }
}

/*
 * Pop and return the front node from list.
 *
 * The returned node is removed from the list and its next pointer is cleared.
 * Return NULL when list is empty.
 */
static inline slist_node_t *slist_pop_front(slist_t *list)
{
    slist_node_t *node;

    SLIST_ASSERT(list != NULL);
    slist_diagnostic_check(list);

    node = list->head;
    if (node == NULL) {
        return NULL;
    }

    list->head = node->next;
    if (list->head == NULL) {
        list->tail = NULL;
    }
    node->next = NULL;
    return node;
}

/*
 * Remove and return the node after prev.
 *
 * When prev is NULL, this removes the front node. The returned node has its
 * next pointer cleared. Return NULL when no node can be removed.
 */
static inline slist_node_t *slist_remove_after(slist_t *list,
                                               slist_node_t *prev)
{
    slist_node_t *node;

    SLIST_ASSERT(list != NULL);
    slist_diagnostic_check(list);

    if (prev == NULL) {
        return slist_pop_front(list);
    }

    node = prev->next;
    if (node == NULL) {
        return NULL;
    }

    prev->next = node->next;
    if (list->tail == node) {
        list->tail = prev;
    }
    node->next = NULL;
    return node;
}

/*
 * Remove node from list.
 *
 * This is an O(n) traversal because singly linked nodes do not store previous
 * pointers. Prefer slist_remove_after() when the previous node is already
 * known. Return true when node was found and removed.
 */
static inline bool slist_remove(slist_t *list, slist_node_t *node)
{
    slist_node_t *prev;

    SLIST_ASSERT(list != NULL);
    SLIST_ASSERT(node != NULL);
    slist_diagnostic_check(list);

    if (list->head == node) {
        (void)slist_pop_front(list);
        return true;
    }

    for (prev = list->head; prev != NULL; prev = prev->next) {
        if (prev->next == node) {
            (void)slist_remove_after(list, prev);
            return true;
        }
    }

    return false;
}

/*
 * Count user nodes in list.
 *
 * This is an O(n) traversal. It is useful for diagnostics, assertions, and
 * low-frequency inspection code. For hot paths, ISRs, schedulers, or queues
 * that need the length frequently, prefer maintaining a separate counter in
 * the owner object instead of calling slist_count().
 */
static inline size_t slist_count(const slist_t *list)
{
    const slist_node_t *node;
    size_t count = 0;

    SLIST_ASSERT(list != NULL);
    for (node = list->head; node != NULL; node = node->next) {
        count++;
    }

    return count;
}

#ifndef slist_container_of

/*
 * Recover the owner object from a pointer to one of its members.
 *
 * ptr must point to type.member. The GNU type check catches accidental member
 * pointer mismatches at compile time.
 */
#define slist_container_of(ptr, type, member) ({                                 \
    __typeof__(ptr) __mptr = (ptr);                                              \
    static_assert(                                                               \
        __builtin_types_compatible_p(__typeof__(*(ptr)),                         \
                                     __typeof__(((type *)0)->member)) ||         \
        __builtin_types_compatible_p(__typeof__(*(ptr)), void),                  \
        "pointer type mismatch in container_of()");                              \
    ((type *)((char *)__mptr - offsetof(type, member)));                         \
})
#endif

/*
 * Recover the owner object from an embedded slist_node_t pointer.
 *
 * Example:
 *   task_t *task = slist_entry(node, task_t, link);
 */
#define slist_entry(ptr, type, member) \
    slist_container_of(ptr, type, member)

/* Return the first owner object in list. list must not be empty. */
#define slist_first_entry(list, type, member) \
    slist_entry((list)->head, type, member)

/* Return the last owner object in list. list must not be empty. */
#define slist_last_entry(list, type, member) \
    slist_entry((list)->tail, type, member)

/*
 * Iterate over raw slist_node_t nodes.
 *
 * pos must be an slist_node_t * variable declared by the caller. Do not delete
 * pos inside the loop body; use slist_for_each_safe() for deletion.
 */
#define slist_for_each(pos, list) \
    for ((pos) = (list)->head; (pos) != NULL; (pos) = (pos)->next)

/*
 * Safely iterate over raw nodes while allowing deletion of pos.
 *
 * pos and n must be slist_node_t * variables declared by the caller.
 */
#define slist_for_each_safe(pos, n, list) \
    for ((pos) = (list)->head; \
         (pos) != NULL && ((n) = (pos)->next, 1); \
         (pos) = (n))

/*
 * Return the owner object after pos.
 *
 * pos must be a pointer to the owner type. member is the embedded slist_node_t
 * field inside that owner type. Return NULL when pos is the last entry.
 */
#define slist_next_entry(pos, member) \
    ((pos)->member.next == NULL ? NULL : \
     slist_entry((pos)->member.next, __typeof__(*(pos)), member))

/*
 * Iterate over owner objects in list.
 *
 * pos must be a pointer variable of the owner type. list must be an slist_t *.
 * Do not delete pos inside the loop body; use slist_for_each_entry_safe() for
 * deletion.
 *
 * Example:
 *   task_t *task;
 *   slist_for_each_entry(task, &ready_list, link) {
 *       run_task(task);
 *   }
 */
#define slist_for_each_entry(pos, list, member)                             \
    for ((pos) = ((list)->head == NULL ? NULL :                             \
                  slist_first_entry(list, __typeof__(*(pos)), member));     \
         (pos) != NULL;                                                     \
         (pos) = slist_next_entry(pos, member))

/*
 * Safely iterate over owner objects while allowing deletion of pos.
 *
 * pos and n must be pointer variables of the owner type.
 *
 * Example:
 *   task_t *task, *next;
 *   slist_for_each_entry_safe(task, next, &ready_list, link) {
 *       slist_remove(&ready_list, &task->link);
 *   }
 */
#define slist_for_each_entry_safe(pos, n, list, member)                     \
    for ((pos) = ((list)->head == NULL ? NULL :                             \
                  slist_first_entry(list, __typeof__(*(pos)), member));     \
         (pos) != NULL && ((n) = slist_next_entry(pos, member), 1);         \
         (pos) = (n))

#ifdef __cplusplus
}
#endif

#endif /* SLIST_H */
