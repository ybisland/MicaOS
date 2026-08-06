#include "tests/test.h"
#include "data_structure/slist.h"

typedef struct test_slist_item {
    uint32_t value;
    slist_node_t node;
} test_slist_item_t;

static void test_slist_item_init(test_slist_item_t *item, uint32_t value)
{
    item->value = value;
    slist_node_init(&item->node);
}

static void test_slist_init_makes_empty_list(void)
{
    slist_t list;
    slist_node_t node;

    node.next = &node;
    slist_init(&list);
    slist_node_init(&node);

    TEST_ASSERT(slist_empty(&list));
    TEST_ASSERT(!slist_has_one_node(&list));
    TEST_ASSERT(!slist_has_multiple_nodes(&list));
    TEST_EQ_PTR(NULL, slist_peek_front(&list));
    TEST_EQ_PTR(NULL, slist_peek_back(&list));
    TEST_EQ_PTR(NULL, slist_peek_next(NULL));
    TEST_EQ_PTR(NULL, node.next);
    TEST_EQ_SIZE(0U, slist_count(&list));
}

static void test_slist_static_init_makes_empty_list(void)
{
    slist_t list = slist_static_init();

    TEST_ASSERT(slist_empty(&list));
    TEST_EQ_PTR(NULL, slist_pop_front(&list));
    TEST_EQ_PTR(NULL, slist_remove_after(&list, NULL));
    TEST_EQ_SIZE(0U, slist_count(&list));
}

static void test_slist_single_node_state_queries_are_correct(void)
{
    slist_t list;
    slist_node_t node;

    slist_init(&list);
    slist_node_init(&node);

    slist_push_back(&list, &node);

    TEST_ASSERT(!slist_empty(&list));
    TEST_ASSERT(slist_has_one_node(&list));
    TEST_ASSERT(!slist_has_multiple_nodes(&list));
    TEST_ASSERT(slist_is_head(&list, &node));
    TEST_ASSERT(slist_is_tail(&list, &node));
    TEST_EQ_PTR(&node, slist_peek_front(&list));
    TEST_EQ_PTR(&node, slist_peek_back(&list));
    TEST_EQ_PTR(NULL, slist_peek_next(&node));
    TEST_EQ_SIZE(1U, slist_count(&list));
}

static void test_slist_push_back_pop_front_is_fifo(void)
{
    slist_t list;
    slist_node_t a;
    slist_node_t b;
    slist_node_t c;

    slist_init(&list);
    slist_node_init(&a);
    slist_node_init(&b);
    slist_node_init(&c);

    slist_push_back(&list, &a);
    slist_push_back(&list, &b);
    slist_push_back(&list, &c);

    TEST_ASSERT(slist_has_multiple_nodes(&list));
    TEST_EQ_SIZE(3U, slist_count(&list));
    TEST_EQ_PTR(&a, slist_pop_front(&list));
    TEST_EQ_PTR(NULL, a.next);
    TEST_EQ_PTR(&b, slist_pop_front(&list));
    TEST_EQ_PTR(&c, slist_pop_front(&list));
    TEST_EQ_PTR(NULL, slist_pop_front(&list));
    TEST_ASSERT(slist_empty(&list));
}

static void test_slist_push_front_pop_front_is_lifo(void)
{
    slist_t list;
    slist_node_t a;
    slist_node_t b;
    slist_node_t c;

    slist_init(&list);
    slist_node_init(&a);
    slist_node_init(&b);
    slist_node_init(&c);

    slist_push_front(&list, &a);
    slist_push_front(&list, &b);
    slist_push_front(&list, &c);

    TEST_EQ_PTR(&c, slist_pop_front(&list));
    TEST_EQ_PTR(&b, slist_pop_front(&list));
    TEST_EQ_PTR(&a, slist_pop_front(&list));
    TEST_ASSERT(slist_empty(&list));
}

static void test_slist_push_front_keeps_tail_at_oldest_node(void)
{
    slist_t list;
    slist_node_t a;
    slist_node_t b;
    slist_node_t c;

    slist_init(&list);
    slist_node_init(&a);
    slist_node_init(&b);
    slist_node_init(&c);

    slist_push_front(&list, &a);
    slist_push_front(&list, &b);
    slist_push_front(&list, &c);

    TEST_EQ_PTR(&c, slist_peek_front(&list));
    TEST_EQ_PTR(&a, slist_peek_back(&list));
    TEST_ASSERT(slist_is_tail(&list, &a));
}

static void test_slist_insert_after_middle_and_tail_updates_order(void)
{
    slist_t list;
    slist_node_t a;
    slist_node_t b;
    slist_node_t c;
    slist_node_t d;

    slist_init(&list);
    slist_node_init(&a);
    slist_node_init(&b);
    slist_node_init(&c);
    slist_node_init(&d);

    slist_push_back(&list, &a);
    slist_push_back(&list, &d);
    slist_insert_after(&list, &a, &b);
    slist_insert_after(&list, &b, &c);

    TEST_EQ_SIZE(4U, slist_count(&list));
    TEST_EQ_PTR(&a, slist_pop_front(&list));
    TEST_EQ_PTR(&b, slist_pop_front(&list));
    TEST_EQ_PTR(&c, slist_pop_front(&list));
    TEST_EQ_PTR(&d, slist_pop_front(&list));
    TEST_ASSERT(slist_empty(&list));
}

static void test_slist_insert_after_tail_moves_tail(void)
{
    slist_t list;
    slist_node_t a;
    slist_node_t b;

    slist_init(&list);
    slist_node_init(&a);
    slist_node_init(&b);

    slist_push_back(&list, &a);
    slist_insert_after(&list, &a, &b);

    TEST_EQ_PTR(&a, slist_peek_front(&list));
    TEST_EQ_PTR(&b, slist_peek_back(&list));
    TEST_ASSERT(slist_is_tail(&list, &b));
}

static void test_slist_remove_after_null_removes_front(void)
{
    slist_t list;
    slist_node_t a;
    slist_node_t b;

    slist_init(&list);
    slist_node_init(&a);
    slist_node_init(&b);

    slist_push_back(&list, &a);
    slist_push_back(&list, &b);

    TEST_EQ_PTR(&a, slist_remove_after(&list, NULL));
    TEST_EQ_PTR(NULL, a.next);
    TEST_EQ_PTR(&b, slist_peek_front(&list));
    TEST_EQ_PTR(&b, slist_peek_back(&list));
}

static void test_slist_remove_after_middle_and_tail_updates_tail(void)
{
    slist_t list;
    slist_node_t a;
    slist_node_t b;
    slist_node_t c;

    slist_init(&list);
    slist_node_init(&a);
    slist_node_init(&b);
    slist_node_init(&c);

    slist_push_back(&list, &a);
    slist_push_back(&list, &b);
    slist_push_back(&list, &c);

    TEST_EQ_PTR(&b, slist_remove_after(&list, &a));
    TEST_EQ_PTR(NULL, b.next);
    TEST_EQ_PTR(&c, slist_peek_next(&a));
    TEST_EQ_PTR(&c, slist_peek_back(&list));

    TEST_EQ_PTR(&c, slist_remove_after(&list, &a));
    TEST_EQ_PTR(NULL, c.next);
    TEST_EQ_PTR(&a, slist_peek_front(&list));
    TEST_EQ_PTR(&a, slist_peek_back(&list));
    TEST_EQ_PTR(NULL, slist_remove_after(&list, &a));
}

static void test_slist_remove_finds_head_middle_tail_and_reports_missing(void)
{
    slist_t list;
    slist_node_t a;
    slist_node_t b;
    slist_node_t c;
    slist_node_t missing;

    slist_init(&list);
    slist_node_init(&a);
    slist_node_init(&b);
    slist_node_init(&c);
    slist_node_init(&missing);

    slist_push_back(&list, &a);
    slist_push_back(&list, &b);
    slist_push_back(&list, &c);

    TEST_ASSERT(slist_remove(&list, &a));
    TEST_EQ_PTR(NULL, a.next);
    TEST_EQ_PTR(&b, slist_peek_front(&list));

    TEST_ASSERT(slist_remove(&list, &c));
    TEST_EQ_PTR(NULL, c.next);
    TEST_EQ_PTR(&b, slist_peek_back(&list));

    TEST_ASSERT(!slist_remove(&list, &missing));
    TEST_EQ_SIZE(1U, slist_count(&list));

    TEST_ASSERT(slist_remove(&list, &b));
    TEST_ASSERT(slist_empty(&list));
}

static void test_slist_raw_iteration_visits_nodes_in_order(void)
{
    slist_t list;
    slist_node_t a;
    slist_node_t b;
    slist_node_t c;
    slist_node_t *pos;
    uint32_t index = 0U;

    slist_init(&list);
    slist_node_init(&a);
    slist_node_init(&b);
    slist_node_init(&c);

    slist_push_back(&list, &a);
    slist_push_back(&list, &b);
    slist_push_back(&list, &c);

    slist_for_each(pos, &list) {
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

static void test_slist_raw_safe_iteration_allows_remove(void)
{
    slist_t list;
    slist_node_t a;
    slist_node_t b;
    slist_node_t c;
    slist_node_t *pos;
    slist_node_t *next;
    uint32_t removed = 0U;

    slist_init(&list);
    slist_node_init(&a);
    slist_node_init(&b);
    slist_node_init(&c);

    slist_push_back(&list, &a);
    slist_push_back(&list, &b);
    slist_push_back(&list, &c);

    slist_for_each_safe(pos, next, &list) {
        TEST_ASSERT(slist_remove(&list, pos));
        removed++;
    }

    TEST_EQ_U32(3U, removed);
    TEST_ASSERT(slist_empty(&list));
}

static void test_slist_entry_and_iteration_return_owner_objects(void)
{
    slist_t list;
    test_slist_item_t a;
    test_slist_item_t b;
    test_slist_item_t c;
    test_slist_item_t *pos;
    uint32_t sum = 0U;

    slist_init(&list);
    test_slist_item_init(&a, 1U);
    test_slist_item_init(&b, 2U);
    test_slist_item_init(&c, 4U);

    slist_push_back(&list, &a.node);
    slist_push_back(&list, &b.node);
    slist_push_back(&list, &c.node);

    TEST_EQ_PTR(&a, slist_entry(slist_peek_front(&list), test_slist_item_t, node));
    TEST_EQ_PTR(&c, slist_entry(slist_peek_back(&list), test_slist_item_t, node));

    slist_for_each_entry(pos, &list, node) {
        sum += pos->value;
    }

    TEST_EQ_U32(7U, sum);
}

static void test_slist_first_last_and_next_entry_return_owner_objects(void)
{
    slist_t list;
    test_slist_item_t a;
    test_slist_item_t b;
    test_slist_item_t c;

    slist_init(&list);
    test_slist_item_init(&a, 1U);
    test_slist_item_init(&b, 2U);
    test_slist_item_init(&c, 3U);

    slist_push_back(&list, &a.node);
    slist_push_back(&list, &b.node);
    slist_push_back(&list, &c.node);

    TEST_EQ_PTR(&a, slist_first_entry(&list, test_slist_item_t, node));
    TEST_EQ_PTR(&c, slist_last_entry(&list, test_slist_item_t, node));
    TEST_EQ_PTR(&b, slist_next_entry(&a, node));
    TEST_EQ_PTR(&c, slist_next_entry(&b, node));
    TEST_EQ_PTR(NULL, slist_next_entry(&c, node));
}

static void test_slist_entry_iteration_handles_empty_list(void)
{
    slist_t list;
    test_slist_item_t *pos = (test_slist_item_t *)1;
    uint32_t count = 0U;

    slist_init(&list);

    slist_for_each_entry(pos, &list, node) {
        count++;
    }

    TEST_EQ_U32(0U, count);
    TEST_EQ_PTR(NULL, pos);
}

static void test_slist_entry_safe_iteration_allows_remove(void)
{
    slist_t list;
    test_slist_item_t a;
    test_slist_item_t b;
    test_slist_item_t c;
    test_slist_item_t *pos;
    test_slist_item_t *next;
    uint32_t removed = 0U;

    slist_init(&list);
    test_slist_item_init(&a, 1U);
    test_slist_item_init(&b, 2U);
    test_slist_item_init(&c, 3U);

    slist_push_back(&list, &a.node);
    slist_push_back(&list, &b.node);
    slist_push_back(&list, &c.node);

    slist_for_each_entry_safe(pos, next, &list, node) {
        TEST_ASSERT(slist_remove(&list, &pos->node));
        removed++;
    }

    TEST_EQ_U32(3U, removed);
    TEST_ASSERT(slist_empty(&list));
    TEST_EQ_PTR(NULL, a.node.next);
    TEST_EQ_PTR(NULL, b.node.next);
    TEST_EQ_PTR(NULL, c.node.next);
}

void test_slist_run(void)
{
    TEST_RUN(test_slist_init_makes_empty_list);
    TEST_RUN(test_slist_static_init_makes_empty_list);
    TEST_RUN(test_slist_single_node_state_queries_are_correct);
    TEST_RUN(test_slist_push_back_pop_front_is_fifo);
    TEST_RUN(test_slist_push_front_pop_front_is_lifo);
    TEST_RUN(test_slist_push_front_keeps_tail_at_oldest_node);
    TEST_RUN(test_slist_insert_after_middle_and_tail_updates_order);
    TEST_RUN(test_slist_insert_after_tail_moves_tail);
    TEST_RUN(test_slist_remove_after_null_removes_front);
    TEST_RUN(test_slist_remove_after_middle_and_tail_updates_tail);
    TEST_RUN(test_slist_remove_finds_head_middle_tail_and_reports_missing);
    TEST_RUN(test_slist_raw_iteration_visits_nodes_in_order);
    TEST_RUN(test_slist_raw_safe_iteration_allows_remove);
    TEST_RUN(test_slist_entry_and_iteration_return_owner_objects);
    TEST_RUN(test_slist_first_last_and_next_entry_return_owner_objects);
    TEST_RUN(test_slist_entry_iteration_handles_empty_list);
    TEST_RUN(test_slist_entry_safe_iteration_allows_remove);
}
