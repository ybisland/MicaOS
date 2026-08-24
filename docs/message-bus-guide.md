# Message Bus Guide

The MicaOS bus is an application-level publish/subscribe service.

Include:

```c
#include "service/bus/bus.h"
```

Compile:

```text
MicaOS/service/bus/bus.c
```

## When to Use Bus

Use bus when one module publishes information that multiple other modules may
care about, and the publisher should not know those subscribers.

Good examples:

- key driver publishes key events
- power module publishes battery state
- UI publishes current screen state
- connection module publishes link state
- logger subscribes to several application events

Do not use bus for every communication path. If a relationship is simple and
direct, use a kernel primitive instead.

| Need | Prefer |
| --- | --- |
| Wake one known task | `task_notify` |
| Wait for bits | `eventset` |
| Count tokens | `sem` |
| Fixed-size queue | `msgq` |
| SPSC byte stream | `pipe` |
| One published topic has multiple consumers | `bus` |

## Channel Types

Bus has two channel types.

| Type | Meaning | Use for |
| --- | --- | --- |
| EVENT | Every publish is a separate message | key events, commands, logs |
| STATE | Only the latest value is kept | UI state, battery level, link state |

Rule:

```text
Need every update: EVENT.
Need only latest value: STATE.
```

EVENT channels use a fixed-capacity ring. Every event stays in the ring until
all subscribers have read it. A slow subscriber can fill the ring and make
publish fail.

STATE channels keep only the latest payload. Slow subscribers do not block
STATE publish; they read the latest value and generation.

## Objects

The static topology is built from:

- `bus_channel_t`
- `bus_subscriber_t`
- `bus_subscription_t`

One subscriber can subscribe to multiple channels.

One subscriber must be consumed by only one task.

If two tasks both need the same channel, define two subscribers.

## Recommended File Layout

Use two application files:

```text
app_bus.h
app_bus.c
```

`app_bus.h` declares payload types, channels, subscribers, and an init function.

`app_bus.c` defines the objects and registration tables.

## Example app_bus.h

```c
#ifndef APP_BUS_H
#define APP_BUS_H

#include "service/bus/bus.h"

typedef struct {
    uint8_t key;
    uint8_t action;
} key_event_t;

typedef struct {
    uint8_t screen;
    uint8_t focus;
} ui_state_t;

BUS_CHANNEL_DECLARE(key_event_channel);
BUS_CHANNEL_DECLARE(ui_state_channel);

BUS_SUBSCRIBER_DECLARE(ui_subscriber);
BUS_SUBSCRIBER_DECLARE(logger_subscriber);

void app_bus_init(void);

#endif
```

## Example app_bus.c

```c
#include "app_bus.h"

BUS_SUBSCRIBER_DEFINE(ui_subscriber);
BUS_SUBSCRIBER_DEFINE(logger_subscriber);

BUS_EVENT_CHANNEL_DEFINE(key_event_channel, key_event_t, 8);
BUS_STATE_CHANNEL_DEFINE(ui_state_channel, ui_state_t);

BUS_CHANNELS_REGISTER(
    BUS_CHANNEL(key_event_channel),
    BUS_CHANNEL(ui_state_channel)
);

BUS_SUBSCRIBERS_REGISTER(
    BUS_SUBSCRIBER(ui_subscriber),
    BUS_SUBSCRIBER(logger_subscriber)
);

BUS_SUBSCRIPTIONS_REGISTER(
    BUS_SUBSCRIBE(key_event_channel, ui_subscriber),
    BUS_SUBSCRIBE(key_event_channel, logger_subscriber),
    BUS_SUBSCRIBE(ui_state_channel, ui_subscriber),
    BUS_SUBSCRIBE(ui_state_channel, logger_subscriber)
);

void app_bus_init(void)
{
    bus_init();
}
```

Registration macros must be used once per program:

- `BUS_CHANNELS_REGISTER`
- `BUS_SUBSCRIBERS_REGISTER`
- `BUS_SUBSCRIPTIONS_REGISTER`

## Strong Configuration Contract

The bus runtime does not defensively validate the topology in target firmware.
The application must guarantee:

- channel table has no duplicate channel
- subscriber table has no duplicate subscriber
- every subscription references registered objects
- the same channel/subscriber pair is not subscribed twice
- all three registration tables exist

During development, run the offline checker:

```powershell
python .\tools\check_bus_config.py .\app\app_bus.c
```

For multiple config files:

```powershell
python .\tools\check_bus_config.py .\app\app_bus.c .\app\app_bus_extra.c
```

## Initialization

Call once before any publish or receive operation:

```c
app_bus_init();
```

## Publish EVENT

```c
void key_driver_report(uint8_t key, uint8_t action)
{
    key_event_t event = {
        .key = key,
        .action = action,
    };

    bus_publish(&key_event_channel, &event);
}
```

`bus_publish()` is synchronous, non-blocking, and copies the payload.

For EVENT channels, publish can fail only when the EVENT ring is full. In that
case, `bus_publish()` calls:

```c
void bus_on_publish_fail(bus_channel_t *channel);
```

## Try Publish

Use `bus_try_publish()` for low-value EVENT payloads that may be dropped:

```c
if (!bus_try_publish(&log_event_channel, &event)) {
    dropped_log_events++;
}
```

`bus_try_publish()` returns `false` when an EVENT ring is full and does not call
the publish-fail hook.

## Publish STATE

```c
void ui_set_screen(uint8_t screen, uint8_t focus)
{
    ui_state_t state = {
        .screen = screen,
        .focus = focus,
    };

    bus_publish(&ui_state_channel, &state);
}
```

STATE publish overwrites the latest value and increments generation.

## Receive Ready Channels

Subscriber tasks call:

```c
bus_channel_t *bus_subscriber_recv(bus_subscriber_t *subscriber,
                                   os_tick_t timeout);
```

Return value:

- non-NULL: one channel is ready
- NULL: no channel became ready before timeout

Example:

```c
bus_channel_t *channel;

channel = bus_subscriber_recv(&ui_subscriber, OS_WAIT_FOREVER);

if (channel == &key_event_channel) {
    read_key_events();
} else if (channel == &ui_state_channel) {
    read_ui_state();
}
```

## Channel Ready Queue

Each subscriber has a channel ready queue.

Publish does not put payload into the subscriber queue. It only queues a ready
item saying:

```text
This channel is readable for this subscriber.
```

Payload remains stored in the channel backend:

- EVENT payload in EVENT ring
- STATE payload in latest state storage

The same channel appears at most once in one subscriber ready queue.

If multiple channels are ready, drain them like this:

```c
for (;;) {
    bus_channel_t *channel;

    channel = bus_subscriber_recv(&ui_subscriber, OS_WAIT_FOREVER);

    while (channel != NULL) {
        handle_ready_channel(channel);
        channel = bus_subscriber_recv(&ui_subscriber, OS_NO_WAIT);
    }
}
```

## Read EVENT

```c
bool bus_event_read(bus_subscriber_t *subscriber,
                    bus_channel_t *channel,
                    void *payload_out,
                    uint32_t *seq_out);
```

Example:

```c
static void read_key_events(void)
{
    key_event_t event;
    uint32_t seq;

    while (bus_event_read(&ui_subscriber,
                          &key_event_channel,
                          &event,
                          &seq)) {
        ui_handle_key_event(&event, seq);
    }
}
```

Loop until `false` if the subscriber must catch up with all pending events.

`seq_out` may be NULL.

## Read STATE

```c
bool bus_state_read(bus_subscriber_t *subscriber,
                    bus_channel_t *channel,
                    void *latest_out,
                    uint32_t *generation_out);
```

Example:

```c
static void read_ui_state(void)
{
    ui_state_t state;
    uint32_t generation;

    if (bus_state_read(&ui_subscriber,
                       &ui_state_channel,
                       &state,
                       &generation)) {
        ui_handle_state(&state, generation);
    }
}
```

`generation_out` may be NULL. If the subscriber needs old/new comparison, it
should store the previous state itself.

## Publish Fail Hook

Default behavior:

- debug build: enters `OS_ASSERT` path
- release build with `NDEBUG`: returns without action

Override it if release builds should log, reset, or fail fast:

```c
void bus_on_publish_fail(bus_channel_t *channel)
{
    log_error("bus publish failed: %s", channel->name);
    system_reset();
}
```

If failure is acceptable for a channel, use `bus_try_publish()`.

## Runtime Contract

For code size and runtime speed, bus is a strong-contract service:

- call `bus_init()` first
- call runtime APIs from task context, not ISR
- pass registered channels/subscribers only
- read only channels subscribed by the subscriber
- pass payload buffers large enough for the channel payload type
- use finite timeouts no larger than `OS_TICK_MAX_DELAY`
- consume each subscriber from one task only

Enable `OS_DIAGNOSTIC_ENABLE` while debugging suspected misuse.

## Current v0 Limits

- static topology only
- no runtime subscribe/unsubscribe
- no dynamic memory
- no ISR publish
- no async broker
- no zero-copy API
- copy payload transfer only
- one consumer task per subscriber
