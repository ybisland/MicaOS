# MicaOS Bus 使用说明

Bus 是 MicaOS 的应用层订阅发布服务，源码位于：

```text
MicaOS/service/bus/
```

使用时包含头文件：

```c
#include "service/bus/bus.h"
```

工程需要编译：

```text
service/bus/bus.c
```

## 1. Bus 解决什么问题

Bus 用来解耦应用模块。

发布者只负责把某类信息发布到一个 channel；订阅者按需订阅 channel。发布者不需要知道有多少订阅者，也不需要知道订阅者属于哪个模块。

典型场景：

- 按键模块发布按键事件，UI、日志、业务逻辑分别订阅。
- 电源模块发布电池状态，UI、低功耗策略、诊断模块分别订阅。
- 连接模块发布连接状态，多个业务模块按需响应。
- UI 模块发布当前页面状态，背光、业务、日志模块各自处理。

Bus 不替代内核同步和通信原语。通信关系很明确时，优先使用更直接的原语：

| 需求 | 推荐 |
| --- | --- |
| 唤醒指定任务 | `task_notify` |
| 等待多个标志位 | `eventset` |
| 计数同步 | `sem` |
| 固定大小消息队列 | `msgq` |
| SPSC 字节流 | `pipe` |
| 一个事件或状态被多个模块订阅 | `bus` |

## 2. 两类 Channel

Bus 有两类 channel：EVENT 和 STATE。

| 类型 | 语义 | 适合场景 |
| --- | --- | --- |
| `BUS_CHANNEL_EVENT` | 每次 publish 都是一条独立消息，subscriber 应逐条读取 | 按键、命令、操作记录、必须完整处理的事件 |
| `BUS_CHANNEL_STATE` | 只保存最新值，publish 会覆盖旧值 | 当前页面、电池电量、连接状态、传感器最新值 |

选择原则：

```text
每一次变化都要处理：用 EVENT。
只关心当前最新值：用 STATE。
```

EVENT channel 使用固定容量 ring。每条 EVENT 需要被所有 subscriber 读完后才能释放。因此任意一个 subscriber 太慢，都可能让 EVENT ring 填满。

STATE channel 只保存 latest。subscriber 太慢不会阻塞 publish，只会看到最新值。

## 3. 核心对象

Bus 有三个静态对象：

```text
channel：发布对象，表示一类 EVENT 或 STATE。
subscriber：订阅者对象，通常对应一个消费任务。
subscription：订阅关系，表示某个 subscriber 订阅某个 channel。
```

一个 subscriber 可以订阅多个 channel。

一个 subscriber 只能由一个 task 消费。不要让多个 task 同时调用同一个 subscriber 的 `bus_subscriber_recv()`、`bus_event_read()` 或 `bus_state_read()`。

如果两个 task 都需要接收同一个 channel，应该定义两个 subscriber，并让它们分别订阅这个 channel。

## 4. 推荐文件组织

建议应用侧集中放置 bus 配置：

```text
app_bus.h：声明业务模块需要使用的 channel / subscriber / payload type。
app_bus.c：定义 channel、subscriber、subscription，并注册 bus 配置表。
```

业务模块 include `app_bus.h`，不要到处散落 bus 配置。

## 5. 定义 Payload

payload 类型通常放在 `app_bus.h`，这样 publisher 和 subscriber 都能使用：

```c
typedef struct {
    uint8_t key;
    uint8_t action;
} key_event_t;

typedef struct {
    uint8_t screen;
    uint8_t focus;
} ui_state_t;
```

## 6. 声明对象

`app_bus.h` 示例：

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

## 7. 定义和注册对象

`app_bus.c` 示例：

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

注意：

- `BUS_CHANNELS_REGISTER()`、`BUS_SUBSCRIBERS_REGISTER()`、`BUS_SUBSCRIPTIONS_REGISTER()` 在整个程序中各只能使用一次。
- `BUS_EVENT_CHANNEL_DEFINE(name, type, capacity)` 的 capacity 是 EVENT ring 容量。
- `BUS_STATE_CHANNEL_DEFINE(name, type)` 不需要容量，STATE 只保存最新值。
- channel 的 `name` 会自动使用对象名字符串，例如 `"key_event_channel"`，可用于 publish fail 日志。

## 8. 配置是强契约

Bus 默认不在目标固件中验证配置表。用户需要保证：

- channel 表中没有重复 channel。
- subscriber 表中没有重复 subscriber。
- subscription 引用的 channel 和 subscriber 都已经注册。
- 同一个 `channel + subscriber` 没有重复订阅。
- 三张注册表都存在，且每张表至少有一个有效项。

这是为了减少目标固件代码大小，并降低 hot path 开销。

开发阶段可以用离线脚本检查显式配置：

```powershell
python .\MicaOS\tools\check_bus_config.py .\app\app_bus.c
```

如果配置分散在多个文件，可以传入多个文件：

```powershell
python .\MicaOS\tools\check_bus_config.py .\app\app_bus.c .\app\app_bus_extra.c
```

脚本只用于开发检查，不进入目标固件。

## 9. 初始化

在任何 publish 或 subscriber recv/read 前，调用一次：

```c
app_bus_init();
```

通常在创建业务任务前初始化 bus。

## 10. 发布 EVENT

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

`bus_publish()` 是同步、非阻塞、copy publish：

```text
同步：publish 在当前 task 中立即完成。
非阻塞：不会等待 subscriber 处理。
copy：payload 会复制进 channel 内部存储。
```

EVENT publish 的正常失败原因只有一个：EVENT ring 满。

如果 EVENT ring 满，`bus_publish()` 会调用：

```c
void bus_on_publish_fail(bus_channel_t *channel);
```

这表示应用设计需要调整：增大 EVENT 容量、降低发布频率、保证 subscriber 及时读取，或者把低价值 EVENT 改成 `bus_try_publish()`。

## 11. 可丢弃 EVENT

低价值 EVENT 可以用 `bus_try_publish()`：

```c
if (!bus_try_publish(&log_event_channel, &event)) {
    dropped_log_events++;
}
```

`bus_try_publish()` 返回 `false` 只表示 EVENT ring 满，本次 payload 没有发布，也不会调用 `bus_on_publish_fail()`。

建议：

```text
关键业务事件：bus_publish()
允许丢弃的日志/统计事件：bus_try_publish()
```

## 12. 发布 STATE

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

STATE publish 会覆盖 latest，并递增 generation。STATE 不会因为 subscriber 慢而积压多份数据。

## 13. 接收 Ready Channel

subscriber task 先调用 `bus_subscriber_recv()`，获取哪个 channel 可读：

```c
bus_channel_t *channel;

channel = bus_subscriber_recv(&ui_subscriber, OS_WAIT_FOREVER);
```

timeout 使用 MicaOS 通用语义：

| timeout | 行为 |
| --- | --- |
| `OS_NO_WAIT` | 只检查一次，不阻塞 |
| 有限 tick | 最多等待指定 tick |
| `OS_WAIT_FOREVER` | 一直等待直到有 channel ready |

返回值：

```text
非 NULL：返回一个 ready channel。
NULL：没有 ready channel，或有限 timeout 到期。
```

如果 subscriber 订阅多个 channel，就用返回的 channel 指针判断来源：

```c
if (channel == &key_event_channel) {
    /* 读取 key EVENT */
} else if (channel == &ui_state_channel) {
    /* 读取 ui STATE */
}
```

## 14. Channel Ready Queue

每个 subscriber 内部都有一个 channel ready queue。

publish 时不会把 payload 放入 subscriber 的 ready queue，只会放入一个 ready item，用来告诉 subscriber：

```text
这个 channel 当前可读。
```

payload 仍然保存在 channel 内部：

- EVENT payload 保存在 EVENT ring。
- STATE payload 保存在 latest storage。

同一个 channel 对同一个 subscriber 最多只会在 ready queue 中出现一次。如果 channel 已经 ready，后续 publish 只更新 channel 后端数据，不会重复插入 ready item。

如果多个 channel 都 ready，`bus_subscriber_recv()` 每次返回一个 channel。用户可以继续用 `OS_NO_WAIT` 取出剩余 ready channel：

```c
for (;;) {
    bus_channel_t *channel;

    channel = bus_subscriber_recv(&ui_subscriber, OS_WAIT_FOREVER);

    while (channel != NULL) {
        ui_handle_ready_channel(channel);
        channel = bus_subscriber_recv(&ui_subscriber, OS_NO_WAIT);
    }
}
```

## 15. 读取 EVENT

EVENT ready 后，通常循环读取，直到返回 `false`：

```c
static void ui_read_key_events(void)
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

为什么建议循环读取：

```text
一个 ready channel 可能对应多条 EVENT。
循环读取可以一次追上该 channel 当前积压的所有 EVENT。
```

如果业务每次只处理一条 EVENT，也可以只调用一次 `bus_event_read()`。如果还有未读 EVENT，这个 channel 会重新进入 subscriber 的 ready queue，后续还会被 `bus_subscriber_recv()` 返回。

`seq_out` 可为 `NULL`。需要调试顺序或统计丢失时，再传入 `uint32_t *`。

## 16. 读取 STATE

STATE ready 后读取 latest：

```c
static void ui_read_state(void)
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

`generation_out` 可为 `NULL`。如果需要判断状态是否变化，subscriber 可以保存上次读到的 generation。

STATE 不保存 previous。如果业务需要比较 old/new，subscriber 自己保存上一份 state。

如果每次变化都必须处理，应使用 EVENT，而不是 STATE。

## 17. 完整 Subscriber 示例

```c
static void ui_task_entry(void *arg)
{
    (void)arg;

    for (;;) {
        bus_channel_t *channel;

        channel = bus_subscriber_recv(&ui_subscriber, OS_WAIT_FOREVER);

        while (channel != NULL) {
            if (channel == &key_event_channel) {
                key_event_t event;

                while (bus_event_read(&ui_subscriber,
                                      &key_event_channel,
                                      &event,
                                      NULL)) {
                    ui_handle_key_event(&event);
                }
            } else if (channel == &ui_state_channel) {
                ui_state_t state;
                uint32_t generation;

                if (bus_state_read(&ui_subscriber,
                                   &ui_state_channel,
                                   &state,
                                   &generation)) {
                    ui_handle_state(&state, generation);
                }
            }

            channel = bus_subscriber_recv(&ui_subscriber, OS_NO_WAIT);
        }
    }
}
```

## 18. Publish Fail Hook

`bus_publish()` 遇到 EVENT ring 满时会调用 weak hook：

```c
void bus_on_publish_fail(bus_channel_t *channel);
```

默认行为：

```text
未定义 NDEBUG：进入 `OS_ASSERT` 路径，用于开发阶段尽早暴露问题。
定义 NDEBUG：默认 hook 直接返回。
```

如果 release 固件也需要日志、复位或其他 fail-fast 策略，项目应重写 hook：

```c
void bus_on_publish_fail(bus_channel_t *channel)
{
    log_error("bus publish failed: %s", channel->name);
    system_reset();
}
```

如果某个 EVENT 允许丢弃，应使用 `bus_try_publish()`，不要依赖 hook。

## 19. 运行时契约

Bus 是 hot path service，默认采用强契约模式，而不是防御式框架。用户必须遵守以下契约：

- `bus_init()` 已经调用。
- runtime API 只在 task context 调用，不在 ISR 调用。
- channel、subscriber、subscription 来自注册表。
- subscriber 已订阅要读取的 channel。
- payload 指针有效。
- payload 对象大小与 channel 定义的 payload type 匹配。
- 有限 timeout 不超过 `OS_TICK_MAX_DELAY`。
- 一个 subscriber 只由一个 task 消费。

调试疑似误用时，可开启：

```c
#define OS_DIAGNOSTIC_ENABLE 1
```

开启后，bus 会用 `OS_DIAG_ASSERT` 检查部分契约错误，例如空指针、ISR 误调用、错误 channel 类型、非法 timeout 等。默认关闭时，这些检查不进入 hot path。

## 20. ISR 使用限制

v0 不支持 ISR 调用 bus runtime API：

- 不在 ISR 调用 `bus_publish()`。
- 不在 ISR 调用 `bus_try_publish()`。
- 不在 ISR 调用 `bus_subscriber_recv()`。
- 不在 ISR 调用 `bus_event_read()` / `bus_state_read()`。

如果 ISR 需要触发应用消息，推荐 ISR 先用 `task_notify`、`eventset` 或 `sem` 唤醒一个任务，再由任务调用 bus publish。

## 21. 常见问题

### EVENT ring 满了怎么办

常见原因：

- capacity 太小。
- 某个 subscriber 长时间没有读取。
- 高频数据流被设计成 EVENT，但业务其实只关心最新值。

处理方式：

- 增大 `BUS_EVENT_CHANNEL_DEFINE()` 的 capacity。
- 确保 subscriber 对 EVENT 循环读取到 `bus_event_read()` 返回 `false`。
- 只关心最新值时改用 STATE。
- 允许丢弃时改用 `bus_try_publish()`。

### `bus_subscriber_recv()` 返回 channel 后不读取会怎样

`bus_subscriber_recv()` 只取出 ready item，不读取 payload。

如果不调用 read API：

- EVENT 仍留在 ring 中，占用容量。
- STATE latest 仍然保留。
- 这次 ready item 已被取出；除非后续 publish 或 EVENT 读取后重新 pending，否则不会自动再次返回同一个 ready item。

因此拿到 ready channel 后，应调用对应 read API。

### 一个 subscriber 可以订阅多个 channel 吗

可以。通过 `bus_subscriber_recv()` 的返回值判断哪个 channel ready。

### 多个 task 可以共用一个 subscriber 吗

不可以。一个 subscriber 只给一个 task 消费。

### 多个 subscriber 可以订阅同一个 channel 吗

可以。这正是 bus 的主要用途。

EVENT channel 会为每个 subscriber 维护读取进度；STATE channel 每个 subscriber 都读取同一份 latest。

## 22. 最小使用流程

```text
1. 定义 payload type。
2. 在 app_bus.h 中声明 channel/subscriber。
3. 在 app_bus.c 中定义 subscriber。
4. 在 app_bus.c 中定义 EVENT/STATE channel。
5. 在 app_bus.c 中注册 channel 表。
6. 在 app_bus.c 中注册 subscriber 表。
7. 在 app_bus.c 中注册 subscription 表。
8. 可选：运行 check_bus_config.py 做离线配置检查。
9. 初始化时调用 bus_init()。
10. publisher 调用 bus_publish() 或 bus_try_publish()。
11. subscriber task 调用 bus_subscriber_recv()。
12. 根据返回 channel 调用 bus_event_read() 或 bus_state_read()。
```

## 23. 当前 v0 限制

- 静态配置，不支持运行时 subscribe/unsubscribe。
- 不使用动态内存。
- 不支持 ISR publish。
- 不支持 async broker。
- 不支持 zero-copy read。
- 不执行用户回调。
- payload 通过 copy 传递。
- 一个 subscriber 只能由一个 task 消费。
