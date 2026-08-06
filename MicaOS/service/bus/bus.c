#include "bus.h"

#include <string.h>

#include "arch/arch_context.h"
#include "common/compiler.h"

#define BUS_WAKE_BIT ((eventset_bits_t)0x01U)

#if BUS_DIAGNOSTIC_ENABLE
#define BUS_DIAG_ASSERT(cond) ASSERT(cond)
#else
#define BUS_DIAG_ASSERT(cond) ((void)0)
#endif

/* Application-provided static topology symbols. */
extern bus_channel_t *g_bus_channels_table[];
extern uint16_t g_bus_channels_count;
extern bus_subscriber_t *g_bus_subscribers_table[];
extern uint16_t g_bus_subscribers_count;
extern bus_subscription_t g_bus_subscriptions_table[];
extern uint16_t g_bus_subscriptions_count;

#if BUS_DIAGNOSTIC_ENABLE
static bool bus_initialized_;
#endif

static uint16_t bus_next_index_(uint16_t index, uint16_t capacity)
{
    index++;
    return (index == capacity) ? 0U : index;
}

static uint8_t *bus_event_payload_(bus_event_backend_t_ *backend,
                                   uint16_t index)
{
    return &((uint8_t *)backend->payload_storage) [(size_t)index * backend->payload_stride];
}

static uint16_t bus_subscription_count_for_channel_(
    const bus_subscription_t *subscriptions,
    uint16_t subscription_count,
    const bus_channel_t *channel)
{
    uint16_t i;
    uint16_t count = 0U;

    for (i = 0U; i < subscription_count; i++) {
        if (subscriptions[i].channel == channel) {
            count++;
        }
    }

    return count;
}

static bus_subscription_t *bus_find_subscription_(
    bus_subscriber_t *subscriber,
    bus_channel_t *channel)
{
    slist_node_t *node;

    for (node = channel->subscribers.head; node != NULL; node = node->next) {
        bus_subscription_t *subscription;

        subscription = slist_entry(node, bus_subscription_t, channel_node);
        if (subscription->subscriber == subscriber) {
            return subscription;
        }
    }

    return NULL;
}

static void bus_event_reclaim_locked_(bus_event_backend_t_ *backend)
{
    while ((backend->count != 0U) &&
           (backend->meta[backend->head].unread_count == 0U)) {
        backend->head = bus_next_index_(backend->head, backend->capacity);
        backend->count--;
        backend->head_seq++;
    }
}

static void bus_subscription_remove_ready_locked_(
    bus_subscription_t *subscription)
{
    bus_subscriber_t *subscriber;

    if (!subscription->pending) {
        return;
    }

    subscriber = subscription->subscriber;
    subscription->pending = false;
    (void)slist_remove(&subscriber->ready_list, &subscription->ready_node);
}

static void bus_mark_ready_locked_(bus_subscription_t *subscription)
{
    bus_subscriber_t *subscriber;

    if (subscription->pending) {
        return;
    }

    subscriber = subscription->subscriber;
    subscription->pending = true;
    slist_push_back(&subscriber->ready_list, &subscription->ready_node);

    if (subscriber->waiting) {
        eventset_set(&subscriber->wake_event, BUS_WAKE_BIT);
    }
}

static void bus_mark_channel_ready_locked_(bus_channel_t *channel)
{
    slist_node_t *node;

    for (node = channel->subscribers.head; node != NULL; node = node->next) {
        bus_subscription_t *subscription;

        subscription = slist_entry(node, bus_subscription_t, channel_node);
        bus_mark_ready_locked_(subscription);
    }
}

static os_tick_t bus_remaining_timeout_(os_tick_t deadline)
{
    os_tick_t now;

    now = os_tick_get();
    if (os_tick_after_eq(now, deadline)) {
        return 0U;
    }

    return (os_tick_t)(deadline - now);
}

void bus_init(void)
{
    bus_channel_t **channels = g_bus_channels_table;
    uint16_t channel_count = g_bus_channels_count;
    bus_subscriber_t **subscribers = g_bus_subscribers_table;
    uint16_t subscriber_count = g_bus_subscribers_count;
    bus_subscription_t *subscriptions = g_bus_subscriptions_table;
    uint16_t subscription_count = g_bus_subscriptions_count;
    uint16_t i;

#if BUS_DIAGNOSTIC_ENABLE
    bus_initialized_ = false;
#endif

    /* Initialize channels. */
    for (i = 0U; i < channel_count; i++) {
        bus_channel_t *channel;

        channel = channels[i];
        slist_init(&channel->subscribers);

        if (channel->type == BUS_CHANNEL_EVENT) {
            bus_event_backend_t_ *backend;

            backend = (bus_event_backend_t_ *)channel->backend;
            memset(backend->meta, 0, (size_t)backend->capacity * sizeof(backend->meta[0]));
            backend->head = 0U;
            backend->tail = 0U;
            backend->count = 0U;
            backend->subscriber_count =
                bus_subscription_count_for_channel_(subscriptions, subscription_count, channel);
            backend->head_seq = 0U;
            backend->next_seq = 0U;
        } else {
            bus_state_backend_t_ *backend;

            backend = (bus_state_backend_t_ *)channel->backend;
            backend->generation = 0U;
            backend->valid = false;
        }
    }

    /* Initialize subscribers. */
    for (i = 0U; i < subscriber_count; i++) {
        bus_subscriber_t *subscriber;

        subscriber = subscribers[i];
        slist_init(&subscriber->ready_list);
        eventset_init(&subscriber->wake_event);
        subscriber->waiting = false;
    }

    /* Initialize subscriptions. */
    for (i = 0U; i < subscription_count; i++) {
        bus_subscription_t *subscription;

        subscription = &subscriptions[i];
        slist_node_init(&subscription->channel_node);
        slist_node_init(&subscription->ready_node);
        subscription->pending = false;

        if (subscription->channel->type == BUS_CHANNEL_EVENT) {
            bus_event_backend_t_ *backend;

            backend = (bus_event_backend_t_ *)subscription->channel->backend;
            subscription->event_next_seq = backend->next_seq;
        } else {
            subscription->event_next_seq = 0U;
        }

        slist_push_back(&subscription->channel->subscribers, &subscription->channel_node);
    }

#if BUS_DIAGNOSTIC_ENABLE
    bus_initialized_ = true;
#endif
}

static bool bus_publish_common_(bus_channel_t *channel,
                                const void *payload,
                                bool fail_on_full)
{
    uint32_t key;

    BUS_DIAG_ASSERT((channel != NULL) &&
                    (payload != NULL) &&
                    !arch_in_isr() &&
                    bus_initialized_);

    key = arch_irq_lock();

    if (channel->type == BUS_CHANNEL_EVENT) {
        bus_event_backend_t_ *backend;
        bus_event_meta_t_ *meta;

        backend = (bus_event_backend_t_ *)channel->backend;
        if (backend->count == backend->capacity) {
            arch_irq_unlock(key);

            if (fail_on_full) {
                bus_on_publish_fail(channel);
            }

            return false;
        }

        meta = &backend->meta[backend->tail];
        memcpy(bus_event_payload_(backend, backend->tail), payload, channel->payload_size);
        meta->seq = backend->next_seq;
        meta->unread_count = backend->subscriber_count;

        backend->tail = bus_next_index_(backend->tail, backend->capacity);
        backend->count++;
        backend->next_seq++;

        bus_mark_channel_ready_locked_(channel);
        bus_event_reclaim_locked_(backend);
    } else {
        bus_state_backend_t_ *backend;

        backend = (bus_state_backend_t_ *)channel->backend;
        memcpy(backend->latest, payload, channel->payload_size);
        backend->generation++;
        backend->valid = true;

        bus_mark_channel_ready_locked_(channel);
    }

    arch_irq_unlock(key);

    return true;
}

void bus_publish(bus_channel_t *channel, const void *payload)
{
    (void)bus_publish_common_(channel, payload, true);
}

bool bus_try_publish(bus_channel_t *channel, const void *payload)
{
    return bus_publish_common_(channel, payload, false);
}

bus_channel_t *bus_subscriber_recv(bus_subscriber_t *subscriber, os_tick_t timeout)
{
    uint32_t key;
    os_tick_t deadline = 0U;
    os_tick_t wait_ticks;
    bool finite_timeout;

    BUS_DIAG_ASSERT((subscriber != NULL) &&
                    !arch_in_isr() &&
                    bus_initialized_ &&
                    ((timeout == OS_WAIT_FOREVER) ||
                     (timeout <= OS_TICK_MAX_DELAY)));

    finite_timeout = (timeout != OS_NO_WAIT) && (timeout != OS_WAIT_FOREVER);
    if (finite_timeout) {
        deadline = os_tick_get() + timeout;
    }

    for (;;) {
        slist_node_t *node;

        key = arch_irq_lock();
        node = slist_pop_front(&subscriber->ready_list);

        if (node != NULL) {
            bus_subscription_t *subscription;

            if (slist_empty(&subscriber->ready_list)) {
                subscriber->wake_event.bits &= ~BUS_WAKE_BIT;
            }

            subscription = slist_entry(node, bus_subscription_t, ready_node);
            subscription->pending = false;
            arch_irq_unlock(key);
            return subscription->channel;
        }

        if (timeout == OS_NO_WAIT) {
            subscriber->wake_event.bits &= ~BUS_WAKE_BIT;
            arch_irq_unlock(key);
            return NULL;
        }

        wait_ticks = timeout;
        if (finite_timeout) {
            wait_ticks = bus_remaining_timeout_(deadline);
            if (wait_ticks == 0U) {
                arch_irq_unlock(key);
                return NULL;
            }
        }

        subscriber->waiting = true;
        subscriber->wake_event.bits &= ~BUS_WAKE_BIT;
        arch_irq_unlock(key);

        (void)eventset_wait_any(&subscriber->wake_event, BUS_WAKE_BIT, wait_ticks);

        key = arch_irq_lock();
        subscriber->waiting = false;
        arch_irq_unlock(key);
    }
}

bool bus_event_read(bus_subscriber_t *subscriber,
                    bus_channel_t *channel,
                    void *payload_out,
                    uint32_t *seq_out)
{
    uint32_t key;
    bus_event_backend_t_ *backend;
    bus_subscription_t *subscription;
    uint32_t offset;
    uint32_t raw_index;
    uint16_t index;
    bus_event_meta_t_ *meta;

    BUS_DIAG_ASSERT((subscriber != NULL) &&
                    (channel != NULL) &&
                    (payload_out != NULL) &&
                    !arch_in_isr() &&
                    bus_initialized_ &&
                    (channel->type == BUS_CHANNEL_EVENT));

    key = arch_irq_lock();

    subscription = bus_find_subscription_(subscriber, channel);
    if (subscription == NULL) {
        arch_irq_unlock(key);
#if BUS_DIAGNOSTIC_ENABLE
        BUS_DIAG_ASSERT(false);
#endif
        return false;
    }

    // return false when event channel is empty
    backend = (bus_event_backend_t_ *)channel->backend;
    if (subscription->event_next_seq == backend->next_seq) {
        bus_subscription_remove_ready_locked_(subscription);
        arch_irq_unlock(key);
        return false;
    }

    offset = subscription->event_next_seq - backend->head_seq;

    raw_index = (uint32_t)backend->head + offset;
    if (raw_index >= backend->capacity) {
        raw_index -= backend->capacity;
    }
    index = (uint16_t)raw_index;

    meta = &backend->meta[index];

    memcpy(payload_out, bus_event_payload_(backend, index), channel->payload_size);

    if (seq_out != NULL) {
        *seq_out = meta->seq;
    }

    subscription->event_next_seq++;
    meta->unread_count--;

    if (subscription->event_next_seq == backend->next_seq) {
        bus_subscription_remove_ready_locked_(subscription);
    } else {
        bus_mark_ready_locked_(subscription);
    }

    bus_event_reclaim_locked_(backend);
    arch_irq_unlock(key);
    return true;
}

bool bus_state_read(bus_subscriber_t *subscriber,
                    bus_channel_t *channel,
                    void *latest_out,
                    uint32_t *generation_out)
{
    uint32_t key;
    bus_state_backend_t_ *backend;
    bus_subscription_t *subscription;
    uint32_t generation;

    BUS_DIAG_ASSERT((subscriber != NULL) &&
                    (channel != NULL) &&
                    (latest_out != NULL) &&
                    !arch_in_isr() &&
                    bus_initialized_ &&
                    (channel->type == BUS_CHANNEL_STATE));

    key = arch_irq_lock();

    subscription = bus_find_subscription_(subscriber, channel);
    if (subscription == NULL) {
        arch_irq_unlock(key);
#if BUS_DIAGNOSTIC_ENABLE
        BUS_DIAG_ASSERT(false);
#endif
        return false;
    }

    backend = (bus_state_backend_t_ *)channel->backend;
    generation = backend->generation;
    if (!backend->valid) {
        bus_subscription_remove_ready_locked_(subscription);
        arch_irq_unlock(key);
        return false;
    }

    memcpy(latest_out, backend->latest, channel->payload_size);

    if (generation_out != NULL) {
        *generation_out = generation;
    }

    bus_subscription_remove_ready_locked_(subscription);
    arch_irq_unlock(key);
    return true;
}

__WEAK void bus_on_publish_fail(bus_channel_t *channel)
{
    (void)channel;

#ifndef NDEBUG
    ASSERT(false);
    for (;;) {
    }
#endif
}
