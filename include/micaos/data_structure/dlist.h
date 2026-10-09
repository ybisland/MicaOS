#ifndef DLIST_H
#define DLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <micaos/common/assert.h>

/*
 * Intrusive circular doubly linked list.
 *
 * Usage:
 *   1. Define a dlist_t as the list sentinel. It does not store user data;
 *      it only marks the beginning/end of the circular list.
 *   2. Embed one dlist_node_t in each owner object for each list membership.
 *      If an object may be linked into two lists at the same time, give it two
 *      independent dlist_node_t members.
 *   3. Initialize both the list sentinel and detached nodes with dlist_init(),
 *      or use dlist_static_init() for static-storage objects.
 *   4. Use dlist_push_front()/dlist_push_back() and
 *      dlist_pop_front()/dlist_pop_back() for deque-style access. Use
 *      dlist_peek_front()/dlist_peek_back() when you only need to inspect the
 *      front/back node without removing it.
 *   5. Use dlist_insert_after()/dlist_insert_before() when inserting relative
 *      to an existing node, and dlist_remove() to detach a node.
 *   6. Use dlist_peek_next()/dlist_peek_prev() to walk around a list while
 *      converting the sentinel boundary to NULL.
 *   7. Recover the owner object with dlist_entry(), or iterate owner objects
 *      directly with dlist_for_each_entry().
 *
 * Usage notes:
 *   - This module requires C99 or later with GNU extensions (uses __typeof__ to
 *     infer types).
 *     Recommended: C11 for better static_assert messages.
 *   - The list never allocates memory. Node lifetime is owned by the caller.
 *   - Parameter validity is not checked by default; callers must satisfy API
 *     preconditions.
 *   - Define OS_DIAGNOSTIC_ENABLE to 1 when debugging list misuse; it
 *     enables parameter and link checks.
 *
 * Concurrency:
 *   This module does not provide any concurrency control. Callers in multi-threaded
 *   environments should protect the structure with appropriate synchronization
 *   mechanisms (e.g., mutexes, semaphores) to ensure thread safety.
 *
 * API quick reference:
 *   - Initialization:
 *       dlist_static_init()
 *       dlist_init()
 *
 *   - Deque-style operations:
 *       dlist_push_front()
 *       dlist_push_back()
 *       dlist_peek_front()
 *       dlist_peek_back()
 *       dlist_pop_front()
 *       dlist_pop_back()
 *
 *   - Position-based operations:
 *       dlist_insert_after()
 *       dlist_insert_before()
 *       dlist_remove()
 *
 *   - State queries:
 *       dlist_node_is_detached()     return true when a node is not linked in any list
 *       dlist_empty()                return true when a list contains no user nodes
 *       dlist_has_one_node()         return true when a list contains exactly one user node
 *       dlist_has_multiple_nodes()   return true when a list contains two or more user nodes
 *       dlist_is_head()              return true when node is the front node of a list
 *       dlist_is_tail()              return true when node is the back node of a list
 *       dlist_peek_next()            return the next user node, or NULL at the end
 *       dlist_peek_prev()            return the previous user node, or NULL at the beginning
 *       dlist_count()                O(n), prefer caller-maintained counters on hot paths
 *
 *   - Owner recovery and iteration:
 *       dlist_entry()
 *       dlist_first_entry()
 *       dlist_last_entry()
 *       dlist_for_each()             iterate over raw dlist_node_t nodes; deleting pos is not allowed
 *       dlist_for_each_safe()        iterate over raw nodes while allowing deletion of pos
 *       dlist_for_each_entry()       iterate over owner objects; deleting pos is not allowed
 *       dlist_for_each_entry_safe()  iterate over owner objects while allowing deletion of pos
 *
 * Examples:
 *   struct task {
 *       int priority;
 *       dlist_node_t link;
 *   };
 *
 *   dlist_t ready_list;
 *   struct task task;
 *
 *   dlist_init(&ready_list);  // init list head
 *   dlist_init(&task.link);   // init user node
 *   dlist_push_back(&ready_list, &task.link);
 *
 *   dlist_node_t *node = dlist_pop_front(&ready_list);
 *   if (node != NULL) {
 *       struct task *ready = dlist_entry(node, struct task, link); // recover owner via dlist_node_t pointer
 *       (void)ready;
 *   }
 *
 *   struct task *pos, *next;
 *   dlist_for_each_entry_safe(pos, next, &ready_list, link) {
 *       dlist_remove(&pos->link);
 *   }
 *
 *   static dlist_t static_ready = dlist_static_init(static_ready); // static init
 */

typedef struct dlist_node {
    struct dlist_node *next;
    struct dlist_node *prev;
} dlist_node_t;
typedef dlist_node_t dlist_t;

/*
 * Static initializer
 *
 * Example:
 *   static dlist_t ready_list = dlist_static_init(ready_list);
 *   static dlist_node_t task_node = dlist_static_init(task_node);
 */
#define dlist_static_init(name) { &(name), &(name) }

/*
 * Runtime initializer
 *
 * Example:
 *   dlist_t ready_list;
 *   dlist_node_t task_node;
 *
 *   dlist_init(&ready_list);
 *   dlist_init(&task_node);
 */
static inline void dlist_init(dlist_node_t *node)
{
    OS_DIAG_ASSERT(node != NULL);
    node->next = node;
    node->prev = node;
}

/*
 * Return true when node is not linked to any other node.
 *
 * A freshly initialized node and a node removed by dlist_remove() are both
 * detached self-loops.
 */
static inline bool dlist_node_is_detached(const dlist_node_t *node)
{
    OS_DIAG_ASSERT(node != NULL);
    return (node->next == node) && (node->prev == node);
}

/*
 * Return true when head contains no user nodes.
 *
 * An empty circular list has its sentinel linked back to itself.
 */
static inline bool dlist_empty(const dlist_t *head)
{
    OS_DIAG_ASSERT(head != NULL);
    return dlist_node_is_detached(head);
}

/* Return true when exactly one user node is linked in head. */
static inline bool dlist_has_one_node(const dlist_t *head)
{
    OS_DIAG_ASSERT(head != NULL);
    return (head->next != head) && (head->next == head->prev);
}

/* Return true when two or more user nodes are linked in head. */
static inline bool dlist_has_multiple_nodes(const dlist_t *head)
{
    OS_DIAG_ASSERT(head != NULL);
    return (head->next != head) && (head->next != head->prev);
}

/*
 * Return true when node is the front node of head.
 *
 * For an empty list, passing head as node also returns true because the
 * sentinel is linked to itself.
 */
static inline bool dlist_is_head(const dlist_t *head, const dlist_node_t *node)
{
    OS_DIAG_ASSERT(head != NULL);
    OS_DIAG_ASSERT(node != NULL);
    return head->next == node;
}

/*
 * Return true when node is the back node of head.
 *
 * For an empty list, passing head as node also returns true because the
 * sentinel is linked to itself.
 */
static inline bool dlist_is_tail(const dlist_t *head, const dlist_node_t *node)
{
    OS_DIAG_ASSERT(head != NULL);
    OS_DIAG_ASSERT(node != NULL);
    return head->prev == node;
}

/*
 * Return the front node in head without removing it.
 *
 * Return NULL when head is empty.
 */
static inline dlist_node_t *dlist_peek_front(const dlist_t *head)
{
    OS_DIAG_ASSERT(head != NULL);
    return dlist_empty(head) ? NULL : head->next;
}

/*
 * Return the back node in head without removing it.
 *
 * Return NULL when head is empty.
 */
static inline dlist_node_t *dlist_peek_back(const dlist_t *head)
{
    OS_DIAG_ASSERT(head != NULL);
    return dlist_empty(head) ? NULL : head->prev;
}

/*
 * Return the user node after node.
 *
 * Return NULL when node is NULL or when node is the back node of head.
 */
static inline dlist_node_t *dlist_peek_next(const dlist_t *head,
                                            const dlist_node_t *node)
{
    OS_DIAG_ASSERT(head != NULL);
    return (node == NULL || dlist_is_tail(head, node)) ? NULL : node->next;
}

/*
 * Return the user node before node.
 *
 * Return NULL when node is NULL or when node is the front node of head.
 */
static inline dlist_node_t *dlist_peek_prev(const dlist_t *head,
                                            const dlist_node_t *node)
{
    OS_DIAG_ASSERT(head != NULL);
    return (node == NULL || dlist_is_head(head, node)) ? NULL : node->prev;
}

#if OS_DIAGNOSTIC_ENABLE
/* Validate adjacent links before insertion. */
static inline void dlist_diagnostic_check_links(const dlist_node_t *prev,
                                                const dlist_node_t *next)
{
    OS_DIAG_ASSERT(prev != NULL);
    OS_DIAG_ASSERT(next != NULL);
    OS_DIAG_ASSERT(prev->next == next);
    OS_DIAG_ASSERT(next->prev == prev);
}

/* Validate that entry's neighbor links are consistent before deletion. */
static inline void dlist_diagnostic_check_entry(const dlist_node_t *entry)
{
    OS_DIAG_ASSERT(entry != NULL);
    OS_DIAG_ASSERT(entry->next != NULL);
    OS_DIAG_ASSERT(entry->prev != NULL);
    OS_DIAG_ASSERT(entry->next->prev == entry);
    OS_DIAG_ASSERT(entry->prev->next == entry);
}
#else
static inline void dlist_diagnostic_check_links(const dlist_node_t *prev,
                                                const dlist_node_t *next)
{
    (void)prev;
    (void)next;
}

static inline void dlist_diagnostic_check_entry(const dlist_node_t *entry)
{
    (void)entry;
}
#endif

/*
 * Push new_node to the front of head.
 *
 * The node is inserted right after the list sentinel, so repeated calls create
 * LIFO order. new_node must be initialized and not already linked in another
 * list.
 *
 * Example:
 *   dlist_t ready_list;
 *   dlist_node_t task_node;
 *
 *   dlist_init(&ready_list);
 *   dlist_init(&task_node);
 *   dlist_push_front(&ready_list, &task_node);
 */
static inline void dlist_push_front(dlist_t *head, dlist_node_t *new_node)
{
    OS_DIAG_ASSERT(head != NULL);
    OS_DIAG_ASSERT(new_node != NULL);
    OS_DIAG_ASSERT(new_node != head);
    OS_DIAG_ASSERT(new_node != head->next);
    OS_DIAG_ASSERT(dlist_node_is_detached(new_node));
    dlist_diagnostic_check_links(head, head->next);

    head->next->prev = new_node;
    new_node->next = head->next;
    new_node->prev = head;
    head->next = new_node;
}

/*
 * Push new_node to the back of head.
 *
 * The node is inserted right before the list sentinel, so repeated calls create
 * FIFO order. new_node must be initialized and not already linked in another
 * list.
 *
 * Example:
 *   dlist_t wait_list;
 *   dlist_node_t task_node;
 *
 *   dlist_init(&wait_list);
 *   dlist_init(&task_node);
 *   dlist_push_back(&wait_list, &task_node);
 */
static inline void dlist_push_back(dlist_t *head, dlist_node_t *new_node)
{
    OS_DIAG_ASSERT(head != NULL);
    OS_DIAG_ASSERT(new_node != NULL);
    OS_DIAG_ASSERT(new_node != head->prev);
    OS_DIAG_ASSERT(new_node != head);
    OS_DIAG_ASSERT(dlist_node_is_detached(new_node));
    dlist_diagnostic_check_links(head->prev, head);

    head->prev->next = new_node;
    new_node->next = head;
    new_node->prev = head->prev;
    head->prev = new_node;
}

/*
 * Insert new_node right after pos.
 *
 * pos may be either a list head or a linked node. new_node must be initialized
 * and not already linked in another list.
 *
 * Example:
 *   dlist_insert_after(&current->link, &next_task->link);
 */
static inline void dlist_insert_after(dlist_node_t *pos, dlist_node_t *new_node)
{
    OS_DIAG_ASSERT(pos != NULL);
    OS_DIAG_ASSERT(new_node != NULL);
    OS_DIAG_ASSERT(new_node != pos);
    OS_DIAG_ASSERT(new_node != pos->next);
    OS_DIAG_ASSERT(dlist_node_is_detached(new_node));
    dlist_diagnostic_check_links(pos, pos->next);

    pos->next->prev = new_node;
    new_node->next = pos->next;
    new_node->prev = pos;
    pos->next = new_node;
}

/*
 * Insert new_node right before pos.
 *
 * pos may be either a list head or a linked node. new_node must be initialized
 * and not already linked in another list.
 *
 * Example:
 *   dlist_insert_before(&current->link, &prev_task->link);
 */
static inline void dlist_insert_before(dlist_node_t *pos, dlist_node_t *new_node)
{
    OS_DIAG_ASSERT(pos != NULL);
    OS_DIAG_ASSERT(new_node != NULL);
    OS_DIAG_ASSERT(new_node != pos->prev);
    OS_DIAG_ASSERT(new_node != pos);
    OS_DIAG_ASSERT(dlist_node_is_detached(new_node));
    dlist_diagnostic_check_links(pos->prev, pos);

    pos->prev->next = new_node;
    new_node->next = pos;
    new_node->prev = pos->prev;
    pos->prev = new_node;
}

/*
 * Remove entry from its current list.
 *
 * After deletion, entry is reinitialized as a detached self-loop, so it can be
 * inserted into a list again with dlist_push_front(), dlist_push_back(), or the insert
 * helpers. Removing an already detached node is a no-op. entry must be either
 * linked in a valid list or already initialized as a detached node.
 *
 * Example:
 *   dlist_remove(&task->link);
 */
static inline void dlist_remove(dlist_node_t *entry)
{
    dlist_diagnostic_check_entry(entry);
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
    dlist_init(entry);
}

/*
 * Pop and return the front node from head.
 *
 * The returned node is removed from the list and reinitialized as a detached
 * self-loop. Return NULL when head is empty.
 *
 * Example:
 *   dlist_node_t *node = dlist_pop_front(&ready_list);
 */
static inline dlist_node_t *dlist_pop_front(dlist_t *head)
{
    dlist_node_t *node;

    OS_DIAG_ASSERT(head != NULL);
    if (dlist_empty(head)) {
        return NULL;
    }

    node = head->next;
    dlist_remove(node);
    return node;
}

/*
 * Pop and return the back node from head.
 *
 * The returned node is removed from the list and reinitialized as a detached
 * self-loop. Return NULL when head is empty.
 *
 * Example:
 *   dlist_node_t *node = dlist_pop_back(&ready_list);
 */
static inline dlist_node_t *dlist_pop_back(dlist_t *head)
{
    dlist_node_t *node;

    OS_DIAG_ASSERT(head != NULL);
    if (dlist_empty(head)) {
        return NULL;
    }

    node = head->prev;
    dlist_remove(node);
    return node;
}

/*
 * Count user nodes in head.
 *
 * This is an O(n) traversal. It is useful for diagnostics, assertions, and
 * low-frequency inspection code. For hot paths, ISRs, schedulers, or queues
 * that need the length frequently, prefer maintaining a separate counter in
 * the owner object instead of calling dlist_count().
 */
static inline size_t dlist_count(const dlist_t *head)
{
    const dlist_node_t *node;
    size_t count = 0;

    OS_DIAG_ASSERT(head != NULL);
    for (node = head->next; node != head; node = node->next) {
        count++;
    }

    return count;
}

#ifndef dlist_container_of

/*
 * Recover the owner object from a pointer to one of its members.
 *
 * ptr must point to type.member. The GNU type check catches accidental member
 * pointer mismatches at compile time.
 */
#define dlist_container_of(ptr, type, member) ({                         \
    __typeof__(ptr) __mptr = (ptr);                                      \
    static_assert(                                                       \
        __builtin_types_compatible_p(__typeof__(*(ptr)),                 \
                                     __typeof__(((type *)0)->member)) || \
            __builtin_types_compatible_p(__typeof__(*(ptr)), void),      \
        "pointer type mismatch in container_of()");                      \
    ((type *)((char *)__mptr - offsetof(type, member)));                 \
})
#endif

/*
 * Recover the owner object from an embedded dlist_node_t pointer.
 *
 * Example:
 *   task_t *task = dlist_entry(node, task_t, link);
 */
#define dlist_entry(ptr, type, member) \
    dlist_container_of(ptr, type, member)

/* Return the first owner object in head. head must not be empty. */
#define dlist_first_entry(head, type, member) \
    dlist_entry((head)->next, type, member)

/* Return the last owner object in head. head must not be empty. */
#define dlist_last_entry(head, type, member) \
    dlist_entry((head)->prev, type, member)

/* Return true when pos has reached the list sentinel. */
#define dlist_entry_is_head(pos, head, member) \
    (&(pos)->member == (head))

/*
 * Iterate over raw dlist_node_t nodes.
 *
 * pos must be a dlist_node_t * variable declared by the caller. Do not delete
 * pos inside the loop body; use dlist_for_each_safe() for deletion.
 */
#define dlist_for_each(pos, head) \
    for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

/*
 * Safely iterate over raw nodes while allowing deletion of pos.
 *
 * pos and n must be dlist_node_t * variables declared by the caller.
 */
#define dlist_for_each_safe(pos, n, head) \
    for ((pos) = (head)->next, (n) = (pos)->next; \
         (pos) != (head); \
         (pos) = (n), (n) = (pos)->next)

/*
 * Return the owner object after pos.
 *
 * pos must be a pointer to the owner type. member is the embedded dlist_node_t
 * field inside that owner type.
 */
#define dlist_next_entry(pos, member) \
    dlist_entry((pos)->member.next, __typeof__(*(pos)), member)

/*
 * Return the owner object before pos.
 *
 * pos must be a pointer to the owner type. member is the embedded dlist_node_t
 * field inside that owner type.
 */
#define dlist_prev_entry(pos, member) \
    dlist_entry((pos)->member.prev, __typeof__(*(pos)), member)

/*
 * Iterate over owner objects in head.
 *
 * pos must be a pointer variable of the owner type. head must be a
 * dlist_t *. Do not delete pos inside the loop body; use
 * dlist_for_each_entry_safe() for deletion.
 *
 * Example:
 *   task_t *task;
 *   dlist_for_each_entry(task, &ready_list, link) {
 *       run_task(task);
 *   }
 */
#define dlist_for_each_entry(pos, head, member)                       \
    for ((pos) = dlist_first_entry(head, __typeof__(*(pos)), member); \
         !dlist_entry_is_head(pos, head, member);                     \
         (pos) = dlist_next_entry(pos, member))

/*
 * Safely iterate over owner objects while allowing deletion of pos.
 *
 * pos and n must be pointer variables of the owner type.
 *
 * Example:
 *   task_t *task, *next;
 *   dlist_for_each_entry_safe(task, next, &ready_list, link) {
 *       dlist_remove(&task->link);
 *   }
 */
#define dlist_for_each_entry_safe(pos, n, head, member)                                       \
    for ((pos) = dlist_first_entry(head, __typeof__(*(pos)), member),                         \
        (n) = dlist_entry_is_head(pos, head, member) ? (pos) : dlist_next_entry(pos, member); \
         !dlist_entry_is_head(pos, head, member);                                             \
         (pos) = (n),                                                                         \
        (n) = dlist_entry_is_head(n, head, member) ? (n) : dlist_next_entry(n, member))

#ifdef __cplusplus
}
#endif

#endif /* DLIST_H */
