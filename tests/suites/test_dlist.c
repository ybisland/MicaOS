#include "tests/test.h"
#include "data_structure/dlist.h"

typedef struct test_dlist_item {
    uint32_t value;
    dlist_node_t node;
} test_dlist_item_t;

static void test_dlist_item_init(test_dlist_item_t *item, uint32_t value)
{
    item->value = value;
    dlist_init(&item->node);
}

static void test_dlist_init_makes_empty_list(void)
{
    dlist_t list;
    dlist_node_t node;

    dlist_init(&list);
    dlist_init(&node);

    TEST_ASSERT(dlist_empty(&list));
    TEST_ASSERT(dlist_node_is_detached(&node));
    TEST_EQ_PTR(NULL, dlist_peek_front(&list));
    TEST_EQ_PTR(NULL, dlist_peek_back(&list));
    TEST_EQ_SIZE(0U, dlist_count(&list));
}

static void test_dlist_single_node_state_queries_are_correct(void)
{
    dlist_t list;
    dlist_node_t node;

    dlist_init(&list);
    dlist_init(&node);

    dlist_push_back(&list, &node);

    TEST_ASSERT(!dlist_empty(&list));
    TEST_ASSERT(dlist_has_one_node(&list));
    TEST_ASSERT(!dlist_has_multiple_nodes(&list));
    TEST_ASSERT(dlist_is_head(&list, &node));
    TEST_ASSERT(dlist_is_tail(&list, &node));
    TEST_EQ_PTR(&node, dlist_peek_front(&list));
    TEST_EQ_PTR(&node, dlist_peek_back(&list));
    TEST_EQ_PTR(NULL, dlist_peek_next(&list, &node));
    TEST_EQ_PTR(NULL, dlist_peek_prev(&list, &node));
}

static void test_dlist_static_init_makes_empty_list(void)
{
    dlist_t list = dlist_static_init(list);

    TEST_ASSERT(dlist_empty(&list));
    TEST_EQ_PTR(NULL, dlist_pop_front(&list));
    TEST_EQ_PTR(NULL, dlist_pop_back(&list));
}

static void test_dlist_push_back_pop_front_is_fifo(void)
{
    dlist_t list;
    dlist_node_t a;
    dlist_node_t b;
    dlist_node_t c;

    dlist_init(&list);
    dlist_init(&a);
    dlist_init(&b);
    dlist_init(&c);

    dlist_push_back(&list, &a);
    dlist_push_back(&list, &b);
    dlist_push_back(&list, &c);

    TEST_ASSERT(dlist_has_multiple_nodes(&list));
    TEST_EQ_SIZE(3U, dlist_count(&list));
    TEST_EQ_PTR(&a, dlist_pop_front(&list));
    TEST_EQ_PTR(&b, dlist_pop_front(&list));
    TEST_EQ_PTR(&c, dlist_pop_front(&list));
    TEST_EQ_PTR(NULL, dlist_pop_front(&list));
    TEST_ASSERT(dlist_empty(&list));
}

static void test_dlist_push_front_pop_front_is_lifo(void)
{
    dlist_t list;
    dlist_node_t a;
    dlist_node_t b;
    dlist_node_t c;

    dlist_init(&list);
    dlist_init(&a);
    dlist_init(&b);
    dlist_init(&c);

    dlist_push_front(&list, &a);
    dlist_push_front(&list, &b);
    dlist_push_front(&list, &c);

    TEST_EQ_PTR(&c, dlist_pop_front(&list));
    TEST_EQ_PTR(&b, dlist_pop_front(&list));
    TEST_EQ_PTR(&a, dlist_pop_front(&list));
    TEST_ASSERT(dlist_empty(&list));
}

static void test_dlist_push_front_pop_back_is_fifo(void)
{
    dlist_t list;
    dlist_node_t a;
    dlist_node_t b;
    dlist_node_t c;

    dlist_init(&list);
    dlist_init(&a);
    dlist_init(&b);
    dlist_init(&c);

    dlist_push_front(&list, &a);
    dlist_push_front(&list, &b);
    dlist_push_front(&list, &c);

    TEST_EQ_PTR(&a, dlist_pop_back(&list));
    TEST_EQ_PTR(&b, dlist_pop_back(&list));
    TEST_EQ_PTR(&c, dlist_pop_back(&list));
    TEST_ASSERT(dlist_empty(&list));
}

static void test_dlist_push_back_pop_back_is_lifo_from_tail(void)
{
    dlist_t list;
    dlist_node_t a;
    dlist_node_t b;
    dlist_node_t c;

    dlist_init(&list);
    dlist_init(&a);
    dlist_init(&b);
    dlist_init(&c);

    dlist_push_back(&list, &a);
    dlist_push_back(&list, &b);
    dlist_push_back(&list, &c);

    TEST_EQ_PTR(&c, dlist_pop_back(&list));
    TEST_EQ_PTR(&b, dlist_pop_back(&list));
    TEST_EQ_PTR(&a, dlist_pop_back(&list));
    TEST_ASSERT(dlist_empty(&list));
}

static void test_dlist_insert_after_and_before_place_nodes_correctly(void)
{
    dlist_t list;
    dlist_node_t a;
    dlist_node_t b;
    dlist_node_t c;

    dlist_init(&list);
    dlist_init(&a);
    dlist_init(&b);
    dlist_init(&c);

    dlist_push_back(&list, &a);
    dlist_insert_after(&a, &c);
    dlist_insert_before(&c, &b);

    TEST_EQ_PTR(&a, dlist_peek_front(&list));
    TEST_EQ_PTR(&c, dlist_peek_back(&list));
    TEST_EQ_PTR(&b, dlist_peek_next(&list, &a));
    TEST_EQ_PTR(&c, dlist_peek_next(&list, &b));
    TEST_EQ_PTR(NULL, dlist_peek_next(&list, &c));
    TEST_EQ_PTR(&b, dlist_peek_prev(&list, &c));
    TEST_EQ_PTR(&a, dlist_peek_prev(&list, &b));
    TEST_EQ_PTR(NULL, dlist_peek_prev(&list, &a));
}

static void test_dlist_insert_after_and_before_head_match_front_back(void)
{
    dlist_t list;
    dlist_node_t a;
    dlist_node_t b;
    dlist_node_t c;

    dlist_init(&list);
    dlist_init(&a);
    dlist_init(&b);
    dlist_init(&c);

    dlist_insert_after(&list, &b);
    dlist_insert_after(&list, &a);
    dlist_insert_before(&list, &c);

    TEST_EQ_SIZE(3U, dlist_count(&list));
    TEST_EQ_PTR(&a, dlist_pop_front(&list));
    TEST_EQ_PTR(&b, dlist_pop_front(&list));
    TEST_EQ_PTR(&c, dlist_pop_front(&list));
    TEST_ASSERT(dlist_empty(&list));
}

static void test_dlist_peek_next_prev_return_null_for_null_node(void)
{
    dlist_t list;
    dlist_node_t node;

    dlist_init(&list);
    dlist_init(&node);
    dlist_push_back(&list, &node);

    TEST_EQ_PTR(NULL, dlist_peek_next(&list, NULL));
    TEST_EQ_PTR(NULL, dlist_peek_prev(&list, NULL));
}

static void test_dlist_remove_detaches_node(void)
{
    dlist_t list;
    dlist_node_t a;
    dlist_node_t b;
    dlist_node_t c;

    dlist_init(&list);
    dlist_init(&a);
    dlist_init(&b);
    dlist_init(&c);

    dlist_push_back(&list, &a);
    dlist_push_back(&list, &b);
    dlist_push_back(&list, &c);

    dlist_remove(&b);

    TEST_ASSERT(dlist_node_is_detached(&b));
    TEST_EQ_SIZE(2U, dlist_count(&list));
    TEST_EQ_PTR(&a, dlist_pop_front(&list));
    TEST_EQ_PTR(&c, dlist_pop_front(&list));
    TEST_ASSERT(dlist_empty(&list));
}

static void test_dlist_remove_front_back_and_detached_nodes(void)
{
    dlist_t list;
    dlist_node_t a;
    dlist_node_t b;
    dlist_node_t c;
    dlist_node_t detached;

    dlist_init(&list);
    dlist_init(&a);
    dlist_init(&b);
    dlist_init(&c);
    dlist_init(&detached);

    dlist_push_back(&list, &a);
    dlist_push_back(&list, &b);
    dlist_push_back(&list, &c);

    dlist_remove(&a);
    TEST_ASSERT(dlist_node_is_detached(&a));
    TEST_EQ_PTR(&b, dlist_peek_front(&list));

    dlist_remove(&c);
    TEST_ASSERT(dlist_node_is_detached(&c));
    TEST_EQ_PTR(&b, dlist_peek_back(&list));

    dlist_remove(&detached);
    TEST_ASSERT(dlist_node_is_detached(&detached));
    TEST_EQ_SIZE(1U, dlist_count(&list));
    TEST_EQ_PTR(&b, dlist_pop_front(&list));
    TEST_ASSERT(dlist_empty(&list));
}

static void test_dlist_raw_iteration_visits_nodes_in_order(void)
{
    dlist_t list;
    dlist_node_t a;
    dlist_node_t b;
    dlist_node_t c;
    dlist_node_t *pos;
    uint32_t index = 0U;

    dlist_init(&list);
    dlist_init(&a);
    dlist_init(&b);
    dlist_init(&c);

    dlist_push_back(&list, &a);
    dlist_push_back(&list, &b);
    dlist_push_back(&list, &c);

    dlist_for_each(pos, &list) {
        if (index == 0U) {
            TEST_EQ_PTR(&a, pos);
        } else if (index == 1U) {
            TEST_EQ_PTR(&b, pos);
        } else if (index == 2U) {
            TEST_EQ_PTR(&c, pos);
        } else {
            TEST_ASSERT(false);
        }

        index++;
    }

    TEST_EQ_U32(3U, index);
}

static void test_dlist_raw_safe_iteration_allows_remove(void)
{
    dlist_t list;
    dlist_node_t a;
    dlist_node_t b;
    dlist_node_t c;
    dlist_node_t *pos;
    dlist_node_t *next;
    uint32_t removed = 0U;

    dlist_init(&list);
    dlist_init(&a);
    dlist_init(&b);
    dlist_init(&c);

    dlist_push_back(&list, &a);
    dlist_push_back(&list, &b);
    dlist_push_back(&list, &c);

    dlist_for_each_safe(pos, next, &list) {
        dlist_remove(pos);
        removed++;
    }

    TEST_EQ_U32(3U, removed);
    TEST_ASSERT(dlist_empty(&list));
    TEST_ASSERT(dlist_node_is_detached(&a));
    TEST_ASSERT(dlist_node_is_detached(&b));
    TEST_ASSERT(dlist_node_is_detached(&c));
}

static void test_dlist_entry_and_iteration_return_owner_objects(void)
{
    dlist_t list;
    test_dlist_item_t a;
    test_dlist_item_t b;
    test_dlist_item_t c;
    test_dlist_item_t *pos;
    uint32_t sum = 0U;

    dlist_init(&list);
    test_dlist_item_init(&a, 1U);
    test_dlist_item_init(&b, 2U);
    test_dlist_item_init(&c, 4U);

    dlist_push_back(&list, &a.node);
    dlist_push_back(&list, &b.node);
    dlist_push_back(&list, &c.node);

    TEST_EQ_PTR(&a, dlist_entry(dlist_peek_front(&list), test_dlist_item_t, node));
    TEST_EQ_PTR(&c, dlist_entry(dlist_peek_back(&list), test_dlist_item_t, node));

    dlist_for_each_entry(pos, &list, node) {
        sum += pos->value;
    }

    TEST_EQ_U32(7U, sum);
}

static void test_dlist_first_and_last_entry_return_owner_objects(void)
{
    dlist_t list;
    test_dlist_item_t a;
    test_dlist_item_t b;
    test_dlist_item_t c;

    dlist_init(&list);
    test_dlist_item_init(&a, 1U);
    test_dlist_item_init(&b, 2U);
    test_dlist_item_init(&c, 3U);

    dlist_push_back(&list, &a.node);
    dlist_push_back(&list, &b.node);
    dlist_push_back(&list, &c.node);

    TEST_EQ_PTR(&a, dlist_first_entry(&list, test_dlist_item_t, node));
    TEST_EQ_PTR(&c, dlist_last_entry(&list, test_dlist_item_t, node));
}

static void test_dlist_safe_iteration_allows_remove(void)
{
    dlist_t list;
    test_dlist_item_t a;
    test_dlist_item_t b;
    test_dlist_item_t c;
    test_dlist_item_t *pos;
    test_dlist_item_t *next;

    dlist_init(&list);
    test_dlist_item_init(&a, 1U);
    test_dlist_item_init(&b, 2U);
    test_dlist_item_init(&c, 3U);

    dlist_push_back(&list, &a.node);
    dlist_push_back(&list, &b.node);
    dlist_push_back(&list, &c.node);

    dlist_for_each_entry_safe(pos, next, &list, node) {
        dlist_remove(&pos->node);
    }

    TEST_ASSERT(dlist_empty(&list));
    TEST_ASSERT(dlist_node_is_detached(&a.node));
    TEST_ASSERT(dlist_node_is_detached(&b.node));
    TEST_ASSERT(dlist_node_is_detached(&c.node));
}

void test_dlist_run(void)
{
    TEST_RUN(test_dlist_init_makes_empty_list);
    TEST_RUN(test_dlist_single_node_state_queries_are_correct);
    TEST_RUN(test_dlist_static_init_makes_empty_list);
    TEST_RUN(test_dlist_push_back_pop_front_is_fifo);
    TEST_RUN(test_dlist_push_front_pop_front_is_lifo);
    TEST_RUN(test_dlist_push_front_pop_back_is_fifo);
    TEST_RUN(test_dlist_push_back_pop_back_is_lifo_from_tail);
    TEST_RUN(test_dlist_insert_after_and_before_place_nodes_correctly);
    TEST_RUN(test_dlist_insert_after_and_before_head_match_front_back);
    TEST_RUN(test_dlist_peek_next_prev_return_null_for_null_node);
    TEST_RUN(test_dlist_remove_detaches_node);
    TEST_RUN(test_dlist_remove_front_back_and_detached_nodes);
    TEST_RUN(test_dlist_raw_iteration_visits_nodes_in_order);
    TEST_RUN(test_dlist_raw_safe_iteration_allows_remove);
    TEST_RUN(test_dlist_entry_and_iteration_return_owner_objects);
    TEST_RUN(test_dlist_first_and_last_entry_return_owner_objects);
    TEST_RUN(test_dlist_safe_iteration_allows_remove);
}
