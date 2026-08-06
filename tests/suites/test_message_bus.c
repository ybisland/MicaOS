#include "tests/test.h"
#include "tests/host/kernel_host_stub.h"
#include "service/bus/bus.h"

typedef struct test_event {
    uint32_t id;
    uint32_t value;
} test_event_t;

typedef struct test_state {
    uint32_t value;
} test_state_t;

#define TEST_BUS_ARRAY_SIZE(array_) \
    (sizeof(array_) / sizeof((array_)[0]))

bus_channel_t *g_bus_channels_table[4];
uint16_t g_bus_channels_count;
bus_subscriber_t *g_bus_subscribers_table[4];
uint16_t g_bus_subscribers_count;
bus_subscription_t g_bus_subscriptions_table[8];
uint16_t g_bus_subscriptions_count;

static uint32_t publish_fail_count_;
static bus_channel_t *publish_fail_channel_;

static void test_bus_hook_reset(void)
{
    publish_fail_count_ = 0U;
    publish_fail_channel_ = NULL;
}

static void test_bus_config_set(bus_channel_t *const *channels,
                                uint16_t channel_count,
                                bus_subscriber_t *const *subscribers,
                                uint16_t subscriber_count,
                                const bus_subscription_t *subscriptions,
                                uint16_t subscription_count)
{
    uint16_t i;

    g_bus_channels_count = channel_count;
    for (i = 0U; i < channel_count; i++) {
        g_bus_channels_table[i] = channels[i];
    }

    g_bus_subscribers_count = subscriber_count;
    for (i = 0U; i < subscriber_count; i++) {
        g_bus_subscribers_table[i] = subscribers[i];
    }

    g_bus_subscriptions_count = subscription_count;
    for (i = 0U; i < subscription_count; i++) {
        g_bus_subscriptions_table[i] = subscriptions[i];
    }
}

#define TEST_BUS_CONFIG_AND_INIT(channels_, subscribers_, subscriptions_)      \
    do {                                                                       \
        test_bus_hook_reset();                                                 \
        test_bus_config_set((channels_),                                       \
                            (uint16_t)TEST_BUS_ARRAY_SIZE(channels_),          \
                            (subscribers_),                                    \
                            (uint16_t)TEST_BUS_ARRAY_SIZE(subscribers_),       \
                            (subscriptions_),                                  \
                            (uint16_t)TEST_BUS_ARRAY_SIZE(subscriptions_));    \
        bus_init();                                                            \
    } while (0)

void bus_on_publish_fail(bus_channel_t *channel)
{
    publish_fail_count_++;
    publish_fail_channel_ = channel;
}

static void test_message_bus_event_publish_recv_and_read_fifo(void)
{
    BUS_SUBSCRIBER_DEFINE(first_subscriber);
    BUS_SUBSCRIBER_DEFINE(second_subscriber);
    BUS_EVENT_CHANNEL_DEFINE(event_channel, test_event_t, 2);
    bus_channel_t *const channels[] = { &event_channel };
    bus_subscriber_t *const subscribers[] = {
        &first_subscriber,
        &second_subscriber,
    };
    bus_subscription_t subscriptions[] = {
        BUS_SUBSCRIBE(event_channel, first_subscriber),
        BUS_SUBSCRIBE(event_channel, second_subscriber),
    };
    test_event_t in1 = { 1U, 10U };
    test_event_t in2 = { 2U, 20U };
    test_event_t out = { 0U, 0U };
    uint32_t seq;

    kernel_host_reset();
    TEST_BUS_CONFIG_AND_INIT(channels, subscribers, subscriptions);

    bus_publish(&event_channel, &in1);
    bus_publish(&event_channel, &in2);

    TEST_EQ_PTR(&event_channel,
                bus_subscriber_recv(&first_subscriber, OS_NO_WAIT));

    TEST_ASSERT(bus_event_read(&first_subscriber,
                               &event_channel,
                               &out,
                               &seq));
    TEST_EQ_U32(1U, out.id);
    TEST_EQ_U32(10U, out.value);
    TEST_EQ_U32(0U, seq);

    TEST_ASSERT(bus_event_read(&first_subscriber,
                               &event_channel,
                               &out,
                               &seq));
    TEST_EQ_U32(2U, out.id);
    TEST_EQ_U32(20U, out.value);
    TEST_EQ_U32(1U, seq);

    TEST_ASSERT(!bus_event_read(&first_subscriber,
                                &event_channel,
                                &out,
                                &seq));

    TEST_EQ_PTR(&event_channel,
                bus_subscriber_recv(&second_subscriber, OS_NO_WAIT));

    TEST_ASSERT(bus_event_read(&second_subscriber,
                               &event_channel,
                               &out,
                               NULL));
    TEST_EQ_U32(1U, out.id);
    TEST_EQ_U32(10U, out.value);

    TEST_ASSERT(bus_event_read(&second_subscriber,
                               &event_channel,
                               &out,
                               NULL));
    TEST_EQ_U32(2U, out.id);
    TEST_EQ_U32(20U, out.value);

    TEST_EQ_PTR(NULL, bus_subscriber_recv(&first_subscriber, OS_NO_WAIT));
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_message_bus_event_ready_is_coalesced(void)
{
    BUS_SUBSCRIBER_DEFINE(subscriber);
    BUS_EVENT_CHANNEL_DEFINE(event_channel, test_event_t, 4);
    bus_channel_t *const channels[] = { &event_channel };
    bus_subscriber_t *const subscribers[] = { &subscriber };
    bus_subscription_t subscriptions[] = {
        BUS_SUBSCRIBE(event_channel, subscriber),
    };
    test_event_t in1 = { 1U, 10U };
    test_event_t in2 = { 2U, 20U };
    test_event_t out = { 0U, 0U };

    kernel_host_reset();
    TEST_BUS_CONFIG_AND_INIT(channels, subscribers, subscriptions);

    bus_publish(&event_channel, &in1);
    bus_publish(&event_channel, &in2);

    TEST_EQ_PTR(&event_channel, bus_subscriber_recv(&subscriber, OS_NO_WAIT));
    TEST_EQ_PTR(NULL, bus_subscriber_recv(&subscriber, OS_NO_WAIT));

    TEST_ASSERT(bus_event_read(&subscriber,
                               &event_channel,
                               &out,
                               NULL));
    TEST_EQ_U32(1U, out.id);

    TEST_ASSERT(bus_event_read(&subscriber,
                               &event_channel,
                               &out,
                               NULL));
    TEST_EQ_U32(2U, out.id);

    TEST_ASSERT(!bus_event_read(&subscriber,
                                &event_channel,
                                &out,
                                NULL));
}

static void test_message_bus_event_partial_read_requeues_channel(void)
{
    BUS_SUBSCRIBER_DEFINE(subscriber);
    BUS_EVENT_CHANNEL_DEFINE(event_channel, test_event_t, 4);
    bus_channel_t *const channels[] = { &event_channel };
    bus_subscriber_t *const subscribers[] = { &subscriber };
    bus_subscription_t subscriptions[] = {
        BUS_SUBSCRIBE(event_channel, subscriber),
    };
    test_event_t in1 = { 1U, 10U };
    test_event_t in2 = { 2U, 20U };
    test_event_t out = { 0U, 0U };

    kernel_host_reset();
    TEST_BUS_CONFIG_AND_INIT(channels, subscribers, subscriptions);

    bus_publish(&event_channel, &in1);
    bus_publish(&event_channel, &in2);

    TEST_EQ_PTR(&event_channel, bus_subscriber_recv(&subscriber, OS_NO_WAIT));

    TEST_ASSERT(bus_event_read(&subscriber,
                               &event_channel,
                               &out,
                               NULL));
    TEST_EQ_U32(1U, out.id);

    TEST_EQ_PTR(&event_channel, bus_subscriber_recv(&subscriber, OS_NO_WAIT));

    TEST_ASSERT(bus_event_read(&subscriber,
                               &event_channel,
                               &out,
                               NULL));
    TEST_EQ_U32(2U, out.id);

    TEST_ASSERT(!bus_event_read(&subscriber,
                                &event_channel,
                                &out,
                                NULL));
    TEST_EQ_PTR(NULL, bus_subscriber_recv(&subscriber, OS_NO_WAIT));
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_message_bus_event_ring_full_is_all_or_none(void)
{
    BUS_SUBSCRIBER_DEFINE(first_subscriber);
    BUS_SUBSCRIBER_DEFINE(second_subscriber);
    BUS_EVENT_CHANNEL_DEFINE(event_channel, test_event_t, 1);
    bus_channel_t *const channels[] = { &event_channel };
    bus_subscriber_t *const subscribers[] = {
        &first_subscriber,
        &second_subscriber,
    };
    bus_subscription_t subscriptions[] = {
        BUS_SUBSCRIBE(event_channel, first_subscriber),
        BUS_SUBSCRIBE(event_channel, second_subscriber),
    };
    test_event_t in1 = { 1U, 10U };
    test_event_t in2 = { 2U, 20U };
    test_event_t out = { 0U, 0U };

    kernel_host_reset();
    TEST_BUS_CONFIG_AND_INIT(channels, subscribers, subscriptions);

    bus_publish(&event_channel, &in1);
    TEST_ASSERT(!bus_try_publish(&event_channel, &in2));
    TEST_EQ_U32(0U, publish_fail_count_);

    TEST_EQ_PTR(&event_channel,
                bus_subscriber_recv(&first_subscriber, OS_NO_WAIT));
    TEST_ASSERT(bus_event_read(&first_subscriber,
                               &event_channel,
                               &out,
                               NULL));
    TEST_EQ_U32(1U, out.id);

    TEST_ASSERT(!bus_try_publish(&event_channel, &in2));

    TEST_EQ_PTR(&event_channel,
                bus_subscriber_recv(&second_subscriber, OS_NO_WAIT));
    TEST_ASSERT(bus_event_read(&second_subscriber,
                               &event_channel,
                               &out,
                               NULL));
    TEST_EQ_U32(1U, out.id);

    bus_publish(&event_channel, &in2);
}

static void test_message_bus_publish_full_calls_fail_hook(void)
{
    BUS_SUBSCRIBER_DEFINE(subscriber);
    BUS_EVENT_CHANNEL_DEFINE(event_channel, test_event_t, 1);
    bus_channel_t *const channels[] = { &event_channel };
    bus_subscriber_t *const subscribers[] = { &subscriber };
    bus_subscription_t subscriptions[] = {
        BUS_SUBSCRIBE(event_channel, subscriber),
    };
    test_event_t in1 = { 1U, 10U };
    test_event_t in2 = { 2U, 20U };
    test_event_t out = { 0U, 0U };

    kernel_host_reset();
    TEST_BUS_CONFIG_AND_INIT(channels, subscribers, subscriptions);

    bus_publish(&event_channel, &in1);
    bus_publish(&event_channel, &in2);

    TEST_EQ_U32(1U, publish_fail_count_);
    TEST_EQ_PTR(&event_channel, publish_fail_channel_);

    TEST_EQ_PTR(&event_channel, bus_subscriber_recv(&subscriber, OS_NO_WAIT));
    TEST_ASSERT(bus_event_read(&subscriber,
                               &event_channel,
                               &out,
                               NULL));
    TEST_EQ_U32(1U, out.id);
    TEST_ASSERT(!bus_event_read(&subscriber,
                                &event_channel,
                                &out,
                                NULL));
}

static void test_message_bus_event_ring_wrap_preserves_fifo_order(void)
{
    BUS_SUBSCRIBER_DEFINE(subscriber);
    BUS_EVENT_CHANNEL_DEFINE(event_channel, test_event_t, 3);
    bus_channel_t *const channels[] = { &event_channel };
    bus_subscriber_t *const subscribers[] = { &subscriber };
    bus_subscription_t subscriptions[] = {
        BUS_SUBSCRIBE(event_channel, subscriber),
    };
    test_event_t in1 = { 1U, 10U };
    test_event_t in2 = { 2U, 20U };
    test_event_t in3 = { 3U, 30U };
    test_event_t in4 = { 4U, 40U };
    test_event_t in5 = { 5U, 50U };
    test_event_t out = { 0U, 0U };
    uint32_t seq;

    kernel_host_reset();
    TEST_BUS_CONFIG_AND_INIT(channels, subscribers, subscriptions);

    bus_publish(&event_channel, &in1);
    bus_publish(&event_channel, &in2);

    TEST_ASSERT(bus_event_read(&subscriber, &event_channel, &out, &seq));
    TEST_EQ_U32(1U, out.id);
    TEST_EQ_U32(0U, seq);
    TEST_ASSERT(bus_event_read(&subscriber, &event_channel, &out, &seq));
    TEST_EQ_U32(2U, out.id);
    TEST_EQ_U32(1U, seq);

    bus_publish(&event_channel, &in3);
    bus_publish(&event_channel, &in4);
    bus_publish(&event_channel, &in5);

    TEST_EQ_PTR(&event_channel, bus_subscriber_recv(&subscriber, OS_NO_WAIT));

    TEST_ASSERT(bus_event_read(&subscriber, &event_channel, &out, &seq));
    TEST_EQ_U32(3U, out.id);
    TEST_EQ_U32(2U, seq);
    TEST_ASSERT(bus_event_read(&subscriber, &event_channel, &out, &seq));
    TEST_EQ_U32(4U, out.id);
    TEST_EQ_U32(3U, seq);
    TEST_ASSERT(bus_event_read(&subscriber, &event_channel, &out, &seq));
    TEST_EQ_U32(5U, out.id);
    TEST_EQ_U32(4U, seq);
    TEST_ASSERT(!bus_event_read(&subscriber, &event_channel, &out, &seq));
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_message_bus_state_keeps_latest_and_generation(void)
{
    BUS_SUBSCRIBER_DEFINE(subscriber);
    BUS_STATE_CHANNEL_DEFINE(state_channel, test_state_t);
    bus_channel_t *const channels[] = { &state_channel };
    bus_subscriber_t *const subscribers[] = { &subscriber };
    bus_subscription_t subscriptions[] = {
        BUS_SUBSCRIBE(state_channel, subscriber),
    };
    test_state_t s1 = { 10U };
    test_state_t s2 = { 20U };
    test_state_t s3 = { 30U };
    test_state_t latest = { 0U };
    uint32_t generation;

    kernel_host_reset();
    TEST_BUS_CONFIG_AND_INIT(channels, subscribers, subscriptions);

    bus_publish(&state_channel, &s1);
    bus_publish(&state_channel, &s2);
    bus_publish(&state_channel, &s3);

    TEST_EQ_PTR(&state_channel, bus_subscriber_recv(&subscriber, OS_NO_WAIT));
    TEST_EQ_PTR(NULL, bus_subscriber_recv(&subscriber, OS_NO_WAIT));

    TEST_ASSERT(bus_state_read(&subscriber,
                               &state_channel,
                               &latest,
                               &generation));
    TEST_EQ_U32(30U, latest.value);
    TEST_EQ_U32(3U, generation);

    TEST_ASSERT(bus_state_read(&subscriber,
                               &state_channel,
                               &latest,
                               &generation));
    TEST_EQ_U32(30U, latest.value);
    TEST_EQ_U32(3U, generation);
}

static void test_message_bus_state_is_invalid_until_first_publish(void)
{
    BUS_SUBSCRIBER_DEFINE(subscriber);
    BUS_STATE_CHANNEL_DEFINE(state_channel, test_state_t);
    bus_channel_t *const channels[] = { &state_channel };
    bus_subscriber_t *const subscribers[] = { &subscriber };
    bus_subscription_t subscriptions[] = {
        BUS_SUBSCRIBE(state_channel, subscriber),
    };
    test_state_t latest = { 0U };
    uint32_t generation = 0xFFFFFFFFU;

    kernel_host_reset();
    TEST_BUS_CONFIG_AND_INIT(channels, subscribers, subscriptions);

    TEST_EQ_PTR(NULL, bus_subscriber_recv(&subscriber, OS_NO_WAIT));
    TEST_ASSERT(!bus_state_read(&subscriber,
                                &state_channel,
                                &latest,
                                &generation));
    TEST_EQ_U32(0xFFFFFFFFU, generation);
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_message_bus_multiple_channels_ready_in_publish_order(void)
{
    BUS_SUBSCRIBER_DEFINE(subscriber);
    BUS_EVENT_CHANNEL_DEFINE(first_event_channel, test_event_t, 4);
    BUS_EVENT_CHANNEL_DEFINE(second_event_channel, test_event_t, 4);
    BUS_STATE_CHANNEL_DEFINE(state_channel, test_state_t);
    bus_channel_t *const channels[] = {
        &first_event_channel,
        &second_event_channel,
        &state_channel,
    };
    bus_subscriber_t *const subscribers[] = { &subscriber };
    bus_subscription_t subscriptions[] = {
        BUS_SUBSCRIBE(first_event_channel, subscriber),
        BUS_SUBSCRIBE(second_event_channel, subscriber),
        BUS_SUBSCRIBE(state_channel, subscriber),
    };
    test_event_t e1 = { 1U, 10U };
    test_event_t e2 = { 2U, 20U };
    test_event_t e3 = { 3U, 30U };
    test_state_t state = { 40U };

    kernel_host_reset();
    TEST_BUS_CONFIG_AND_INIT(channels, subscribers, subscriptions);

    bus_publish(&second_event_channel, &e2);
    bus_publish(&state_channel, &state);
    bus_publish(&first_event_channel, &e1);
    bus_publish(&second_event_channel, &e3);

    TEST_EQ_PTR(&second_event_channel,
                bus_subscriber_recv(&subscriber, OS_NO_WAIT));
    TEST_EQ_PTR(&state_channel,
                bus_subscriber_recv(&subscriber, OS_NO_WAIT));
    TEST_EQ_PTR(&first_event_channel,
                bus_subscriber_recv(&subscriber, OS_NO_WAIT));
    TEST_EQ_PTR(NULL, bus_subscriber_recv(&subscriber, OS_NO_WAIT));
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

static void test_message_bus_reinit_clears_runtime_state(void)
{
    BUS_SUBSCRIBER_DEFINE(subscriber);
    BUS_EVENT_CHANNEL_DEFINE(event_channel, test_event_t, 2);
    BUS_STATE_CHANNEL_DEFINE(state_channel, test_state_t);
    bus_channel_t *const channels[] = {
        &event_channel,
        &state_channel,
    };
    bus_subscriber_t *const subscribers[] = { &subscriber };
    bus_subscription_t subscriptions[] = {
        BUS_SUBSCRIBE(event_channel, subscriber),
        BUS_SUBSCRIBE(state_channel, subscriber),
    };
    test_event_t event = { 1U, 10U };
    test_event_t event_out = { 0U, 0U };
    test_state_t state = { 20U };
    test_state_t state_out = { 0U };

    kernel_host_reset();
    TEST_BUS_CONFIG_AND_INIT(channels, subscribers, subscriptions);

    bus_publish(&event_channel, &event);
    bus_publish(&state_channel, &state);

    bus_init();

    TEST_EQ_PTR(NULL, bus_subscriber_recv(&subscriber, OS_NO_WAIT));
    TEST_ASSERT(!bus_event_read(&subscriber,
                                &event_channel,
                                &event_out,
                                NULL));
    TEST_ASSERT(!bus_state_read(&subscriber,
                                &state_channel,
                                &state_out,
                                NULL));
    TEST_EQ_U32(0U, kernel_host_irq_lock_depth());
}

void test_message_bus_run(void)
{
    TEST_RUN(test_message_bus_event_publish_recv_and_read_fifo);
    TEST_RUN(test_message_bus_event_ready_is_coalesced);
    TEST_RUN(test_message_bus_event_partial_read_requeues_channel);
    TEST_RUN(test_message_bus_event_ring_full_is_all_or_none);
    TEST_RUN(test_message_bus_publish_full_calls_fail_hook);
    TEST_RUN(test_message_bus_event_ring_wrap_preserves_fifo_order);
    TEST_RUN(test_message_bus_state_is_invalid_until_first_publish);
    TEST_RUN(test_message_bus_state_keeps_latest_and_generation);
    TEST_RUN(test_message_bus_multiple_channels_ready_in_publish_order);
    TEST_RUN(test_message_bus_reinit_clears_runtime_state);
}
