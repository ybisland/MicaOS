#ifndef BUS_H
#define BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <micaos/common/assert.h>
#include <micaos/data_structure/slist.h>
#include <micaos/eventset.h>
#include <micaos/time.h>

/*
 * Event/State Bus
 *
 * Lightweight publish/subscribe service for decoupling application modules.
 * Runtime publish is synchronous and non-blocking.
 *
 * Channel types:
 *   - BUS_CHANNEL_EVENT keeps every published event until all subscribers read
 *     it. Every event records the number of those who have not read it, so a slow
 *     subscriber can fill the event ring and make publish fail.
 *   - BUS_CHANNEL_STATE keeps only the latest value. Subscribers can get the
 *     latest generation when reading the state.
 *
 * Usage:
 *   1. Define subscribers, channels, and subscriptions in a central app_bus.c,
 *      and expose the required objects through app_bus.h.
 *   2. In app_bus.c, use BUS_SUBSCRIBER_DEFINE(),
 *      BUS_EVENT_CHANNEL_DEFINE(), BUS_STATE_CHANNEL_DEFINE(), and the
 *      BUS_*_REGISTER() macros to build the static bus topology.
 *   3. In app_bus.h, declare shared channels/subscribers with
 *      BUS_CHANNEL_DECLARE() and BUS_SUBSCRIBER_DECLARE().
 *   4. During application initialization, call bus_init() once before any
 *      publish or receive operation.
 *   5. Publishers call bus_publish(), or bus_try_publish() for low-value
 *      EVENT payloads that may be dropped when the ring is full.
 *   6. Subscriber tasks call bus_subscriber_recv(), then read the selected
 *      channel with bus_event_read() or bus_state_read().
 *
 * Usage notes:
 *   - One subscriber may subscribe to multiple channels.
 *   - One subscriber must be consumed by only one task.
 *
 * For code size and runtime performance, this framework follows a
 * strong-contract model rather than a defensive model. Users must strictly
 * follow the documented contracts. When debugging suspected misuse, define
 * OS_DIAGNOSTIC_ENABLE to 1 to enable diagnostics for contract violations.
 *
 * Runtime contract:
 *   - bus_init() must be called before using bus.
 *   - Runtime APIs must be called from task context, not ISR context.
 *   - Channels, subscribers, and subscriptions must come from the registered
 *     static topology.
 *   - Payload input/output pointers must refer to object storage large enough
 *     for the selected channel payload type.
 *   - Finite timeout values must be no larger than OS_TICK_MAX_DELAY.
 *
 * Publish failure:
 *   - STATE publish normally does not fail.
 *   - EVENT publish fails only when the event ring is full.
 *   - bus_publish() calls bus_on_publish_fail() on EVENT ring full.
 *   - bus_try_publish() returns false on EVENT ring full and does not call the
 *     failure hook.
 *   - The default bus_on_publish_fail() stops through OS_ASSERT in debug
 *     builds when NDEBUG is not defined, and returns without action when
 *     NDEBUG is defined. Override the hook when release builds also need
 *     logging, reset, or a custom fail-fast policy.
 *
 * Implementation notes:
 *   - No dynamic memory is used.
 *   - EVENT channels use a fixed-size ring and per-event reference counts.
 *   - STATE channels store only the latest payload and a generation counter.
 *   - Each subscriber owns a channel ready queue. Publish only enqueues ready
 *     channel notifications; payload remains stored in the channel backend.
 */

typedef enum bus_channel_type {
    BUS_CHANNEL_EVENT = 0,
    BUS_CHANNEL_STATE,
} bus_channel_type_t;

/*
 * Public object storage.
 *
 * Define subscribers and channels through BUS_SUBSCRIBER_DEFINE(),
 * BUS_EVENT_CHANNEL_DEFINE(), and BUS_STATE_CHANNEL_DEFINE().
 * Register subscription entries through BUS_SUBSCRIPTIONS_REGISTER() and
 * BUS_SUBSCRIBE().
 * Fields are visible only because MicaOS uses static allocation; application
 * code should not access them directly.
 */
typedef struct bus_channel {
    const char *name;
    uint16_t payload_size;
    bus_channel_type_t type;
    void *backend;
    slist_t subscribers; // store subscription.channel_node
} bus_channel_t;

typedef struct bus_subscriber {
    slist_t ready_list;  // store subscription.ready_node
    eventset_t wake_event;
    bool waiting;
} bus_subscriber_t;

typedef struct bus_subscription {
    bus_channel_t *channel;
    bus_subscriber_t *subscriber;
    slist_node_t channel_node;
    slist_node_t ready_node;
    bool pending; // true when ready_node is linked in subscriber ready_list
    uint32_t event_next_seq;
} bus_subscription_t;

typedef union bus_max_align_ {
    void *ptr;
    uint32_t u32;
    uint64_t u64;
    double d;
} bus_max_align_t_;

typedef struct bus_event_meta_ {
    uint16_t unread_count; // subscribers that have not read this event
    uint32_t seq;          // sequence number of the event
} bus_event_meta_t_;

typedef struct bus_event_backend_ {
    bus_event_meta_t_ *meta;   // array of meta info
    void *payload_storage;     // array of storage
    uint16_t payload_stride;   // payload stride
    uint16_t capacity;         // ring capacity
    uint16_t head;             // ring head point
    uint16_t tail;             // ring tail point
    uint16_t count;            // event count
    uint16_t subscriber_count; // subscriber numbers
    uint32_t head_seq;         // the sequence of first event in the ring
    uint32_t next_seq;         // the sequence of the incoming event in the ring
} bus_event_backend_t_;

typedef struct bus_state_backend_ {
    void *latest;
    uint32_t generation;
    bool valid;
} bus_state_backend_t_;

#define BUS_CHANNEL_DECLARE(name) \
    extern bus_channel_t name

#define BUS_SUBSCRIBER_DECLARE(name) \
    extern bus_subscriber_t name

#define BUS_SUBSCRIBER_DEFINE(name) \
    bus_subscriber_t name = { 0 }

#define BUS_EVENT_CHANNEL_DEFINE(name, payload_type_, capacity_)               \
    static_assert(sizeof(payload_type_) <= UINT16_MAX,                         \
                  "bus EVENT payload is too large");                           \
    static_assert(((capacity_) > 0) && ((capacity_) <= UINT16_MAX),            \
                  "bus EVENT capacity must be 1..UINT16_MAX");                 \
    typedef union name##_payload_slot {                                        \
        bus_max_align_t_ align;                                                \
        payload_type_ payload;                                                 \
        uint8_t bytes[sizeof(payload_type_)];                                  \
    } name##_payload_slot_t;                                                   \
    static_assert(sizeof(name##_payload_slot_t) <= UINT16_MAX,                 \
                  "bus EVENT payload stride is too large");                    \
    static name##_payload_slot_t name##_payload_storage[(capacity_)];          \
    static bus_event_meta_t_ name##_event_meta[(capacity_)];                   \
    static bus_event_backend_t_ name##_event_backend = {                       \
        name##_event_meta,                                                     \
        name##_payload_storage,                                                \
        (uint16_t)sizeof(name##_payload_slot_t),                               \
        (uint16_t)(capacity_),                                                 \
    };                                                                         \
    bus_channel_t name = {                                                     \
        #name,                                                                 \
        (uint16_t)sizeof(payload_type_),                                       \
        BUS_CHANNEL_EVENT,                                                     \
        &name##_event_backend,                                                 \
        slist_static_init(),                                                   \
    }

#define BUS_STATE_CHANNEL_DEFINE(name, payload_type_)                          \
    static_assert(sizeof(payload_type_) <= UINT16_MAX,                         \
                  "bus STATE payload is too large");                           \
    typedef union name##_state_buffer {                                        \
        bus_max_align_t_ align;                                                \
        payload_type_ payload;                                                 \
        uint8_t bytes[sizeof(payload_type_)];                                  \
    } name##_state_buffer_t;                                                   \
    static name##_state_buffer_t name##_state_buffer;                          \
    static bus_state_backend_t_ name##_state_backend = {                       \
        name##_state_buffer.bytes,                                             \
        0U,                                                                    \
        false,                                                                 \
    };                                                                         \
    bus_channel_t name = {                                                     \
        #name,                                                                 \
        (uint16_t)sizeof(payload_type_),                                       \
        BUS_CHANNEL_STATE,                                                     \
        &name##_state_backend,                                                 \
        slist_static_init(),                                                   \
    }

#define BUS_CHANNEL(channel_) \
    &(channel_)

#define BUS_SUBSCRIBER(subscriber_) \
    &(subscriber_)

#define BUS_SUBSCRIBE(channel_, subscriber_) \
    { &(channel_), &(subscriber_) }

#define BUS_ARRAY_SIZE_(array_) \
    (sizeof(array_) / sizeof((array_)[0]))

#define BUS_CHANNELS_REGISTER(...)                                             \
    bus_channel_t *g_bus_channels_table[] = { __VA_ARGS__ };                   \
    uint16_t g_bus_channels_count =                                            \
        (uint16_t)BUS_ARRAY_SIZE_(g_bus_channels_table)

#define BUS_SUBSCRIBERS_REGISTER(...)                                          \
    bus_subscriber_t *g_bus_subscribers_table[] = { __VA_ARGS__ };             \
    uint16_t g_bus_subscribers_count =                                         \
        (uint16_t)BUS_ARRAY_SIZE_(g_bus_subscribers_table)

#define BUS_SUBSCRIPTIONS_REGISTER(...)                                        \
    bus_subscription_t g_bus_subscriptions_table[] = { __VA_ARGS__ };          \
    uint16_t g_bus_subscriptions_count =                                       \
        (uint16_t)BUS_ARRAY_SIZE_(g_bus_subscriptions_table)

/*
 * Initialize the global bus using the registered static topology defined in
 * app_bus.c.
 *
 * The correctness of the configuration shall be guaranteed by the user.
 * bus_init() initializes runtime links and does not validate the config.
 */
void bus_init(void);

/*
 * Publish one EVENT or STATE payload.
 *
 * This API copies payload once. If an EVENT ring is full,
 * bus_on_publish_fail() is called.
 *
 * Normal application code can treat bus_publish() as always succeeding. A
 * failure means the bus design is incorrect: increase the EVENT capacity, make
 * sure subscribers consume events in time, or use bus_try_publish() for events
 * that are allowed to be dropped.
 */
void bus_publish(bus_channel_t *channel, const void *payload);

/*
 * Try to publish one EVENT or STATE payload.
 *
 * This API copies payload once.
 *
 * Return true when payload has been published. Return false only when an EVENT
 * channel ring is full and no payload is published. This API is intended for
 * low-value events that may be dropped.
 * STATE channels normally return true.
 */
bool bus_try_publish(bus_channel_t *channel, const void *payload);

/*
 * Receive one ready channel for subscriber.
 *
 * timeout follows MicaOS timeout rules:
 *   - OS_NO_WAIT checks once and never blocks.
 *   - OS_WAIT_FOREVER waits without a deadline.
 *
 * Return NULL when no channel is ready before timeout expires.
 */
bus_channel_t *bus_subscriber_recv(bus_subscriber_t *subscriber,
                                   os_tick_t timeout);

/*
 * Read the next EVENT payload for subscriber on channel.
 *
 * Return true when one event payload has been copied to payload_out. Return
 * false when no event is currently readable. For EVENT ready, call this in a
 * loop until false if the subscriber must catch up with every pending event.
 */
bool bus_event_read(bus_subscriber_t *subscriber,
                    bus_channel_t *channel,
                    void *payload_out,
                    uint32_t *seq_out);

/*
 * Read the latest STATE payload for subscriber on channel.
 *
 * Return true when the latest state payload has been copied to latest_out.
 * Return false when the state channel has no valid latest value yet. STATE read
 * returns the latest value and generation.
 */
bool bus_state_read(bus_subscriber_t *subscriber,
                    bus_channel_t *channel,
                    void *latest_out,
                    uint32_t *generation_out);

/*
 * Weak publish-failed hook.
 *
 * The default implementation stops through OS_ASSERT when NDEBUG is not
 * defined. When NDEBUG is defined, the default implementation returns without
 * action. Projects may override this hook to log, reset, or apply a custom
 * publish-failure policy. Use bus_try_publish() for low-value events that may
 * be dropped without entering this hook.
 *
 * `channel->name` can be useful in the hook for logging.
 */
void bus_on_publish_fail(bus_channel_t *channel);

#ifdef __cplusplus
}
#endif

#endif /* BUS_H */
