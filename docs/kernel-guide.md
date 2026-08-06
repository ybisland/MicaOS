# MicaOS 使用指南

本文档面向 MicaOS 使用者，按“先会用，再理解细节”的顺序介绍内核 API。
各模块头文件中还包含更完整的参数约束和模块级说明。

用户代码通常只需要包含一个总入口：

```c
#include "kernel/kernel.h"
```

`task.h`、`scheduler.h`、`time.h`、`eventset.h`、`sem.h`、`msgq.h`、`pipe.h`
以及可选的 `timer.h` 是为了拆分内核实现而存在。普通用户不需要分别包含它们。

## 基础使用

基础使用包括任务创建、调度器启动、tick 推进、延时、可选 soft timer 和 idle hook。

### MicaOS 定位

MicaOS 面向低成本 32 位 MCU，核心目标是简单、静态、可预测。

- 不依赖动态内存。
- 任务控制块和任务栈由用户静态分配。
- OS 内部拥有 idle task 和 idle stack。
- 支持静态优先级调度。
- 支持不同优先级之间抢占。
- 同优先级任务不做时间片轮转，只在协作式调度点切换。
- ISR 可以唤醒任务，但不会直接在 ISR 内切换任务。

### 内核配置

内核级配置集中在 `kernel_config.h`。项目可以通过构建选项统一覆盖这些宏，确保所有内核源码看到同一份配置。

常用配置：

```c
#define SCHED_PRIORITY_LEVELS 8U
#define SCHED_IDLE_STACK_SIZE 128U
#define OS_TIMER_ENABLE 0
#define OS_DIAGNOSTIC_ENABLE 0
#define TASK_STACK_WATERMARK_ENABLE 0
#define TASK_STACK_FILL_PATTERN 0xA5U
#define OS_TRACE_ENABLE 0
```

含义：

- `SCHED_PRIORITY_LEVELS`：任务优先级数量，范围是 1..32。数字越小优先级越高。
- `SCHED_IDLE_STACK_SIZE`：OS 内部 idle task 的栈大小，单位是字节，必须 8 字节对齐。
- `OS_TIMER_ENABLE`：是否启用 soft timer。默认关闭，让 `os_tick_advance()` 保持短小。
- `OS_DIAGNOSTIC_ENABLE`：是否启用 OS 内部一致性诊断断言，默认关闭。
- `TASK_STACK_WATERMARK_ENABLE`：是否启用任务栈水位估算。默认关闭。
- `TASK_STACK_FILL_PATTERN`：栈水位估算使用的填充值，默认 `0xA5`。
- `OS_TRACE_ENABLE`：是否启用通用 trace hook。默认关闭。

推荐通过编译选项覆盖，例如：

```c
-DSCHED_PRIORITY_LEVELS=16U
-DOS_TIMER_ENABLE=1
```

### 创建任务

每个任务至少需要一个 `task_t`、一段任务栈和一个任务入口函数。

```c
static task_t worker;
static task_stack(worker_stack, 256);

static void worker_entry(void *arg)
{
    (void)arg;

    for (;;) {
        /* Do work. */
        task_delay(10);
    }
}
```

`task_stack(name, size)` 用于声明 8 字节对齐的任务栈，`size` 单位是字节。

使用 `task_init()` 初始化任务：

```c
task_init(&worker,                 // `worker`：任务控制块。
          "worker",                // `"worker"`：任务名，可以用于调试。
          worker_entry,            // `worker_entry`：任务入口函数。
          NULL,                    // `NULL`：传给任务入口函数的参数。
          worker_stack,            // `worker_stack`：任务栈。
          sizeof(worker_stack),    // `sizeof(worker_stack)`：任务栈大小。
          0);                      // `0`：任务优先级，数字越小优先级越高。
```

### 任务调试信息

task 模块提供一组只读查询 API，用于调试、日志输出或简单状态观察。

```c
const char *task_get_name(const task_t *task);
task_priority_t task_get_priority(const task_t *task);
task_state_t task_get_state(const task_t *task);
task_wait_type_t task_get_wait_type(const task_t *task);
```

`task_get_name()` 返回 `task_init()` 时传入的名字。如果初始化时传入 `NULL`，则返回 `NULL`。

`task_get_priority()` 返回任务配置的静态优先级。

`task_get_state()` 返回任务当前状态快照，例如 `TASK_STATE_READY`、`TASK_STATE_RUNNING`、`TASK_STATE_BLOCKED`。

`task_get_wait_type()` 返回任务当前等待原因快照，例如 `TASK_WAIT_DELAY`、`TASK_WAIT_SEM`、`TASK_WAIT_MSGQ_RECV`。

这些接口只返回调用瞬间的快照。任务状态可能在返回后立刻变化，因此它们适合调试和观测，不应作为复杂同步逻辑的依据。

启用 `TASK_STACK_WATERMARK_ENABLE` 后，还可以查询任务栈历史使用情况：

```c
size_t task_get_stack_unused(const task_t *task);
size_t task_get_stack_used(const task_t *task);
```

开启后，`task_init()` 会先用 `TASK_STACK_FILL_PATTERN` 填充整段任务栈，再构造初始上下文帧。
任务运行后，被使用过的栈空间会覆盖这个 pattern。查询函数通过扫描仍保留 pattern 的区域，估算历史最大栈使用量。
当前实现按栈向低地址增长来计算，这符合 Cortex-M 和常见 32 位 MCU 的任务栈使用方式。

这个结果是调试估算值，适合用来调整任务栈大小。它不是栈溢出保护机制。

### 启动调度器

调度器启动顺序通常是：
1. `scheduler_init()` 初始化调度器.
2. `scheduler_add()` 把已初始化的任务加入调度队列。如果调度器已经运行，
新加入的高优先级任务会抢占当前低优先级任务。
3. `scheduler_start()` 启动最高优先级 READY 任务，并且不会返回。

```c
int main(void)
{
    scheduler_init();

    task_init(&worker,
              "worker",
              worker_entry,
              NULL,
              worker_stack,
              sizeof(worker_stack),
              0);

    scheduler_add(&worker);
    scheduler_start();
}
```

### 调度规则

优先级数字越小，优先级越高。比如 priority 0 高于 priority 1。

优先级数量：

任务优先级默认有8级(0..7)，可通过 `SCHED_PRIORITY_LEVELS` 修改。优先级数量越多，ready queue
数组占用的 RAM 越多，调度管理成本也会略有增加，应当按需分配优先级数量。

调度规则：

- 高优先级 READY 任务会抢占低优先级当前任务。
- 同优先级任务不会互相抢占。
- 同优先级任务按 FIFO 顺序在协作式调度点切换。
- 同优先级之间无时间片轮转。

常见调度点：

- `task_yield()`
- `task_delay()`
- `task_notify_wait()`
- `eventset_wait_any()` / `eventset_wait_all()`
- `sem_take()`
- `msgq_send()` / `msgq_recv()`
- `task_exit()`

### 系统心跳

`os_tick_advance()` 用于推进 OS tick，通常在 SysTick 或其他周期性定时器 ISR 中调用。

```c
void SysTick_Handler(void)
{
    os_tick_advance();
}
```

每调用一次 `os_tick_advance()`，OS tick 增加一，并唤醒已经到期的 delay 任务。
如果启用了 `OS_TIMER_ENABLE`，还会执行到期的 soft timer 回调。

读取当前 tick：

```c
os_tick_t start;

start = os_tick_get();
```

tick 是 `uint32_t`，会自然回绕。因此，比较时间时不要直接使用 `<` 或 `>`，应使用`time.h`
中提供的API：

```c
// elapsed
if (os_tick_elapsed(os_tick_get(), start, 100)) {
    /* 100 ticks elapsed. */
}

// until
os_tick_t deadline = os_tick_get() + 100;
if (os_tick_after_eq(os_tick_get(), deadline)) {
    /* deadline reached. */
}
```

由于tick自然回绕的特性， 所有涉及时间先后和时间差的判断，都必须保证**真实差值**小于计数器范围的一半。
因此 `task_delay` 最大值限制为 `OS_TICK_MAX_DELAY`，当前定义为 `INT32_MAX - 1`。

### 主动让出CPU

常用的主动让出CPU的函数有两个：

1. `task_yield`

    ```c
    for (;;) {
        do_small_work();
        task_yield();
    }
    ```

    `task_yield()` 是协作式调度点，主动让出CPU，让调度器进行一次调度。

    需要注意，如果当前任务的优先级较高，`yield`之后调度器可能还是选择当前任务进行工作。

2. `task_delay(tick)` 任务延时

    ```c
    task_delay(10);
    ```

    `task_delay(0)` 等价于 `task_yield()`。非零 delay 会阻塞当前任务，直到指定 tick 数过去。

### Soft Timer

soft timer 是由 OS tick 驱动的软件定时器。它适合周期性触发小动作，或者在超时后通知某个任务继续处理。
timer 对象由用户静态分配，内部不使用动态内存。

soft timer 默认关闭，以保持 `os_tick_advance()` 尽量短小。需要使用时，通过构建选项统一定义：

```c
#define OS_TIMER_ENABLE 1
```

这个宏必须对内核源码也生效，推荐使用编译选项 `-DOS_TIMER_ENABLE=1`。

开启后，`kernel.h` 会包含 `timer.h`，`os_tick_advance()` 也会处理到期 timer。

API 概览：

```c
typedef void (*timer_callback_t)(void *arg);

void timer_init(soft_timer_t *timer, timer_callback_t callback, void *arg);
void timer_start(soft_timer_t *timer, os_tick_t delay, os_tick_t period);
void timer_stop(soft_timer_t *timer);
bool timer_is_running(const soft_timer_t *timer);
```

`timer_init()` 初始化 timer，`callback` 不能为空。

`timer_start()` 应在 `scheduler_init()` 之后调用。

`timer_start(timer, delay, 0)` 启动一次性 timer，`delay` 到期后执行一次回调。

`timer_start(timer, delay, period)` 启动周期性 timer，第一次在 `delay` tick 后触发，之后每 `period` tick 触发一次。
`delay` 必须非零且 `<= OS_TICK_MAX_DELAY`；`period == 0` 表示一次性 timer，非零时也必须 `<= OS_TICK_MAX_DELAY`。

`timer_stop()` 停止 timer，重复停止已经停止的 timer 是安全的。

`timer_is_running()` 查询 timer 当前是否处于运行状态。

示例：使用 soft timer 周期性唤醒任务。

```c
static task_t blink_task;
static soft_timer_t blink_timer;

static void blink_timer_cb(void *arg)
{
    task_t *task = (task_t *)arg;

    task_notify(task);
}

static void blink_entry(void *arg)
{
    (void)arg;

    timer_init(&blink_timer, blink_timer_cb, &blink_task);
    timer_start(&blink_timer, 100, 100);

    for (;;) {
        if (task_notify_wait(OS_WAIT_FOREVER)) {
            toggle_led();
        }
    }
}
```

timer callback 从 `os_tick_advance()` 的处理路径中执行，通常仍然处于 ISR 上下文。
因此 callback 必须短小，不能调用会阻塞的 API。推荐在 callback 中只做通知动作，例如：

- `task_notify()`
- `eventset_set()`
- `sem_give()`
- `msgq_send(..., OS_NO_WAIT)`
- `pipe_write(..., OS_NO_WAIT)`


### Idle Task

OS 内部拥有 idle task 和 idle stack。**默认 idle hook 会等待中断(`__WFI()`)。**
用户可以重写 `scheduler_idle_hook()`，用于喂狗、进入低功耗模式，或者直接返回。

```c
void scheduler_idle_hook(void)
{
    feed_watchdog();
}
```

内部 idle stack 大小由 `SCHED_IDLE_STACK_SIZE` 控制。

Idle stack 应该这样配置：

- 默认 `SCHED_IDLE_STACK_SIZE` 是 128 bytes。
- 这个默认值只面向最小 idle 行为：idle hook 只等待中断，例如默认实现，或者用户实现中只调用 `__WFI()` / `arch_wait_for_interrupt()`。
- 在 NUCLEO-F411RE、Debug、`TASK_STACK_WATERMARK_ENABLE=1` 的 MCU 测试中，WFI-only idle 实测结果如下：

```text
ARMv7M soft-float:
SCHED_IDLE_STACK_SIZE=128: used 88, unused 40
SCHED_IDLE_STACK_SIZE=256: used 88, unused 168
SCHED_IDLE_STACK_SIZE=512: used 88, unused 424

ARMv7M_FPU hard-float:
SCHED_IDLE_STACK_SIZE=128: used 92, unused 36
```

因此，如果你的 idle hook 只做 WFI，128 bytes 可以作为默认配置使用。

如果你重写 `scheduler_idle_hook()` 并在里面执行更多逻辑，例如喂狗、调用 HAL 低功耗接口、打印日志、做统计或访问复杂驱动，就不应该继续假设 128 bytes 一定足够。此时建议：

1. 先把 `SCHED_IDLE_STACK_SIZE` 调大，例如 256 或 512 bytes。
2. 打开 `TASK_STACK_WATERMARK_ENABLE`。
3. 运行最接近真实业务的场景。
4. 用 `scheduler_idle_stack_used()` / `scheduler_idle_stack_unused()` 查看 idle stack 水位。
5. 根据实测值保留足够余量后再确定最终配置。

注意：idle stack 水位只是一种调试估算，用来帮助配置栈大小；它不是栈溢出保护机制。

## 同步原语

同步原语用于让任务等待某个条件，或者从 ISR/其他任务唤醒等待任务。

### 阻塞 API 的通用规则

内核中带 timeout 的阻塞 API 使用统一语义：

- `timeout == OS_NO_WAIT`：只检查一次，不阻塞，立即返回。
- `timeout` 为有限 tick：等待指定 tick 数，超时返回失败。
- `timeout == OS_WAIT_FOREVER`：一直等待，不设置超时。

除了 `OS_WAIT_FOREVER` 这个特殊值，所有有限 timeout 都必须 `<= OS_TICK_MAX_DELAY`，
这是为了保证 tick 回绕时仍能判断时间先后。
`OS_NO_WAIT` 的值是 0，是非阻塞检查，也属于合法 timeout。

示例：

```c
if (sem_take(&rx_sem, OS_NO_WAIT)) {
    /* Took a token without blocking. */
}

if (sem_take(&rx_sem, 100)) {
    /* Took a token within 100 ticks. */
}

if (sem_take(&rx_sem, OS_WAIT_FOREVER)) {
    /* Took a token, waited as long as needed. */
}
```

### ISR 调用规则

ISR 中适合做的事情：

- `task_notify()`
- `eventset_set()`
- `sem_give()`
- 启用 `OS_TIMER_ENABLE` 后的 `timer_start()` / `timer_stop()`
- `msgq_send(..., OS_NO_WAIT)`
- `msgq_recv(..., OS_NO_WAIT)`
- `os_tick_advance()`

ISR 中禁止做的事情：

- 调用会阻塞的 wait API，如`task_notify_wait()`、`task_delay()`
- 调用带非零 timeout 的 `sem_take()`、`msgq_send()` 或 `msgq_recv()`。

### Task Notification

task notification 是最轻量的任务直达唤醒机制。它适合“一方唤醒某个固定任务”的场景。

API 概览：

```c
void task_notify(task_t *task);
bool task_notify_wait(os_tick_t timeout);
```

`task_notify(task)` 向指定任务发送一次通知。它可以在任务上下文或 ISR 中调用。

`task_notify_wait(timeout)` 只能由普通任务调用(禁止在ISR中调用)，用于让当前任务等待通知。
如果已经有未处理通知，它会立即返回 `true`；否则根据 timeout 规则决定是否阻塞。
返回 `false` 表示没有等到通知，通常是 `timeout == OS_NO_WAIT` 时没有未处理通知，
或者有限 timeout 到期。

ISR 或其他任务通知目标任务：

```c
static task_t io_task;

void USART_IRQHandler(void)
{
    task_notify(&io_task);
}
```

目标任务等待通知：

```c
static void io_entry(void *arg)
{
    (void)arg;

    for (;;) {
        if (task_notify_wait(OS_WAIT_FOREVER)) {
            /* Read received data. */
        }
    }
}
```

只检查一次，不阻塞：

```c
if (task_notify_wait(OS_NO_WAIT)) {
    /* A notification was available. */
}
```

特点：

- 每个任务最多记录一次未处理通知。
- 多次 notify 不会计数累加，只表示“至少通知过一次”。
- notify 发生在 wait 之前不会丢失；下一次 wait 会直接返回成功。
- 如果任务正在等待其他同步对象（如eventset），notify 不会打断那个等待，会保留到之后的 notify_wait 调用。
- notification 没有“多个 waiter”的概念，因为它绑定在某个具体任务上；但可以有多个 producer 通知同一个任务。

如果需要多个条件位、wait-any 或 wait-all 语义，使用 eventset。

### Eventset

eventset 是独立事件标志对象，适合表示某个驱动、模块或服务拥有的一组状态位。
它内部保存一个 32-bit 事件变量，最多可以记录 32 个标志位。

API 概览：

```c
void eventset_init(eventset_t *eventset);
void eventset_set(eventset_t *eventset, eventset_bits_t bits);
void eventset_clear(eventset_t *eventset, eventset_bits_t bits);
eventset_bits_t eventset_get(eventset_t *eventset);
eventset_bits_t eventset_wait_any(eventset_t *eventset,
                                  eventset_bits_t mask,
                                  os_tick_t timeout);
eventset_bits_t eventset_wait_all(eventset_t *eventset,
                                  eventset_bits_t mask,
                                  os_tick_t timeout);
```

`eventset_init()` 初始化 eventset，使用前必须调用。

`eventset_set()` 设置事件位，可以在任务上下文或 ISR 中调用。它会唤醒**所有**等待条件已经满足的任务。

`eventset_clear()` 手动清除事件位。eventset唤醒任务后不会自动清除 bit，需手动清除。

`eventset_get()` 读取当前事件位。

`eventset_wait_any()` 等待 mask 中任意一个 bit 成立，返回匹配到的 bit。

`eventset_wait_all()` 等待 mask 中所有 bit 都成立，成功时返回 mask。

两个 wait API 返回 `0` 表示没有等到条件，通常是 `timeout == OS_NO_WAIT` 时条件不成立，
或者有限 timeout 到期。

定义事件位并初始化：

```c
#define RX_READY  (1UL << 0)
#define TX_DONE   (1UL << 1)

static eventset_t uart_events;

static void app_init(void)
{
    eventset_init(&uart_events);
}
```

生产者设置事件位：

```c
void USART_IRQHandler(void)
{
    eventset_set(&uart_events, RX_READY);
}
```

等待任意事件位：

```c
eventset_bits_t bits;

bits = eventset_wait_any(&uart_events,
                         RX_READY | TX_DONE,
                         OS_WAIT_FOREVER);

if ((bits & RX_READY) != 0U) {
    /* RX is ready. */
}
if ((bits & TX_DONE) != 0U) {
    /* TX is done. */
}
```

等待所有事件位：

```c
if (eventset_wait_all(&uart_events, RX_READY | TX_DONE, 100) != 0U) {
    /* RX_READY and TX_DONE are both set. */
}
```

读取当前事件位：

```c
eventset_bits_t current;

current = eventset_get(&uart_events);
```

清除事件位：

```c
eventset_clear(&uart_events, RX_READY);
```

等待 eventset 不会自动清除 bit，用户需要在合适的位置手动调用 `eventset_clear()`。
这样可以避免多个任务等待同一个 eventset 时，第一个醒来的任务把其他任务关心的状态清掉。

如果想把某个 bit 当作消费型事件，常见写法是 wait 成功后由事件拥有者清除：

```c
bits = eventset_wait_any(&uart_events, RX_READY, OS_WAIT_FOREVER);
if ((bits & RX_READY) != 0U) {
    eventset_clear(&uart_events, RX_READY);
}
```

多个 waiter 的情况：

- 多个任务可以等待同一个 eventset。
- `eventset_set()` 会遍历等待队列，唤醒所有条件满足的任务。
- 被唤醒任务进入 READY 后，实际运行顺序由调度器优先级和同优先级 FIFO 规则决定。
- 因为 bit 不会自动清除，多个 waiter 可以同时观察到同一个事件状态。
- 如果某个 bit 是消费型事件，需要明确约定由谁清除，避免一个任务清除后影响其他任务。

### Semaphore

sem 是轻量 counting semaphore，用于表示资源或事件 token 的数量。

API 概览：

```c
void sem_init(sem_t *sem, uint16_t initial, uint16_t limit);
void sem_give(sem_t *sem);
bool sem_take(sem_t *sem, os_tick_t timeout);
```

`sem_init()` 初始化 semaphore。`initial` 是初始 token 数量，`limit` 是最大 token 数量。

`sem_give()` 产生一个 token，可以在任务上下文或 ISR 中调用。如果已经有 waiter，
它会唤醒一个 waiter，而不是增加 count。需注意，若唤醒的 waiter 优先级更高，
在任务上下文中会触发抢占；在 ISR 中会请求一次延迟调度，等 ISR 退出后再切换任务。

`sem_take()` 消费一个 token。返回 `true` 表示成功获得 token；
返回 `false` 表示没有获得 token，通常是 `timeout == OS_NO_WAIT` 时没有 token，
或者有限 timeout 到期。

初始化 semaphore：

```c
static sem_t rx_packets;

static void app_init(void)
{
    sem_init(&rx_packets, 0, 16);
}
```

生产一个 token：

```c
void USART_IRQHandler(void)
{
    /* Store the packet first, then signal that one packet is available. */
    sem_give(&rx_packets);
}
```

消费一个 token：

```c
static void rx_task_entry(void *arg)
{
    (void)arg;

    for (;;) {
        if (sem_take(&rx_packets, OS_WAIT_FOREVER)) {
            /* Consume one received packet. */
        }
    }
}
```

`sem_give()` 的语义：

- 如果已经有任务等待，唤醒一个等待任务。
- 如果没有任务等待，count 增加到 limit 为止。
- 如果 count 已经等于 limit，再次 give 保持饱和状态。

多个任务已经阻塞在同一个 semaphore 上时，`sem_give()` 按等待先后 FIFO 唤醒一个 waiter。
被唤醒的任务何时实际运行，仍由调度器优先级决定：高优先级任务可能抢占当前任务，
同优先级任务在调度点按 FIFO 规则运行。

多个 waiter 的情况：

- 多个任务可以等待同一个 semaphore。
- 已经阻塞的 waiter 按 FIFO 顺序排队。
- 每次 `sem_give()` 最多唤醒一个 waiter。
- 被唤醒的 waiter 直接获得这次 give 对应的 token。
- 如果没有 waiter，token 才会累加到 count 中。
- semaphore 适合表达“资源数量”或“事件次数”，不适合表达多个 named condition。

## 通信原语

通信原语用于在任务之间传递数据。当前内核提供 `pipe` 和 `msgq`：

- `pipe`：SPSC 字节流，单入口单出口，带阻塞 read/write。
- `msgq`：MPMC 定长消息队列，带阻塞 send/recv。

另外，也可以使用数据结构模块自己组合通信机制：

- `bytebuf` SPSC byte ring buffer 数据结构，无同步。
- `packetbuf` SPSC 变长 packet buffer 数据结构，无同步。
- `bytebuf` 和 `packetbuf` 是纯数据结构，无同步机制。
- `bytebuf` 和 `packetbuf` 本身是 SPSC 无锁结构；如果用于 MPMC 场景，需要用户自己实现临界区保护。
- 如果需要阻塞/唤醒语义，可以把它们和 `task_notify()`、`eventset` 或 `sem` 组合使用。

例如：ISR 把字节写入 `bytebuf`，然后 `task_notify()` 唤醒处理任务；或者驱动把 packet 写入
`packetbuf`，再用 `sem_give()` 表示有一个 packet 可读。

### Pipe

pipe 是严格 SPSC 字节流通信对象。它内部使用一段 caller-owned storage 作为环形缓冲区，
并提供阻塞 read/write 语义，仅支持单入口单出口。

API 概览：

```c
#define pipe_storage(name, size) ...

void pipe_init(pipe_t *pipe, void *buffer, uint16_t size);
uint16_t pipe_write(pipe_t *pipe,
                            const void *data,
                            uint16_t len,
                            os_tick_t timeout);
uint16_t pipe_read(pipe_t *pipe,
                           void *data,
                           uint16_t len,
                           os_tick_t timeout);
uint16_t pipe_count(pipe_t *pipe);
uint16_t pipe_space(pipe_t *pipe);
void pipe_reset(pipe_t *pipe);
```

`pipe_storage()` 静态声明 pipe 使用的字节存储空间。

`pipe_init()` 初始化 pipe。`buffer` 必须指向 `size` 字节的存储。

`pipe_write()` 写入字节流，返回实际写入字节数。

`pipe_read()` 读取字节流，返回实际读取字节数。

`pipe_count()` 返回当前可读字节数。

`pipe_space()` 返回当前可写字节数。

`pipe_reset()` 丢弃已有数据，并中止正在等待的 reader/writer。它适合错误恢复或协议重置，
不适合普通数据传输流程。

定义和初始化 pipe：

```c
static pipe_t uart_pipe;
static pipe_storage(uart_pipe_storage, 128);

static void app_init(void)
{
    pipe_init(&uart_pipe, uart_pipe_storage, sizeof(uart_pipe_storage));
}
```

ISR 中非阻塞写入：

```c
void USART_IRQHandler(void)
{
    uint8_t byte = USART_READ_BYTE();

    (void)pipe_write(&uart_pipe, &byte, 1, OS_NO_WAIT);
}
```

任务中阻塞读取：

```c
static void uart_task_entry(void *arg)
{
    uint8_t buffer[32];
    uint16_t len;

    (void)arg;

    for (;;) {
        len = pipe_read(&uart_pipe,
                                buffer,
                                sizeof(buffer),
                                OS_WAIT_FOREVER);
        if (len != 0U) {
            process_bytes(buffer, len);
        }
    }
}
```

pipe 的读写是 byte stream 语义，没有消息边界。一次 read/write 返回值可能小于请求的 `len`：

- 如果已有数据或空间，尽量读/写并立即返回。
- 如果完全不能读/写，按 timeout 规则决定是否阻塞。
- 阻塞等待成功后，只保证至少能传输 1 字节，不保证满足完整 `len`。

SPSC 约束：

- 一个 pipe 只能有一个 writer 和一个 reader。
- 不能多个任务同时写同一个 pipe，也不能多个任务同时读同一个 pipe。
- ISR 可以作为 writer 或 reader，但只能使用 `OS_NO_WAIT`。
- 如果需要 MPMC 字节流，请基于 `bytebuf`、`sem`、`eventset` 等模块自行组合，并自己实现临界区保护。

### Message Queue

msgq 是固定长度消息队列。队列存储由用户提供，内部不使用动态内存。

API 概览：

```c
#define msgq_storage(name, msg_type, capacity) ...

void msgq_init(msgq_t *q, void *buffer, uint16_t msg_size, uint16_t capacity);
bool msgq_send(msgq_t *q, const void *msg, os_tick_t timeout);
bool msgq_recv(msgq_t *q, void *msg, os_tick_t timeout);
uint16_t msgq_count(msgq_t *q);
uint16_t msgq_space(msgq_t *q);
```

`msgq_storage()` 静态声明 msgq 使用的存储空间。

`msgq_init()` 初始化队列。`buffer` 必须指向 `msg_size * capacity` 字节的存储。

`msgq_send()` 发送一条定长消息。返回 `true` 表示消息已经拷贝进队列；
返回 `false` 表示没有发送成功，通常是队列满且不等待，或者有限 timeout 到期。

`msgq_recv()` 接收一条定长消息。返回 `true` 表示消息已经拷贝到用户缓冲区；
返回 `false` 表示没有接收成功，通常是队列空且不等待，或者有限 timeout 到期。

`msgq_count()` 返回队列中已有消息数量。

`msgq_space()` 返回队列剩余空槽数量。

定义消息类型和队列存储：

```c
typedef struct app_msg {
    uint16_t id;
    uint16_t value;
} app_msg_t;

static msgq_t app_msgq;
static msgq_storage(app_msgq_storage, app_msg_t, 8);
```

初始化队列：

```c
msgq_init(&app_msgq,
          app_msgq_storage,
          sizeof(app_msg_t),
          8);
```

发送消息：

```c
app_msg_t msg = {
    .id = 1,
    .value = 123,
};

if (!msgq_send(&app_msgq, &msg, 100)) {
    /* Queue stayed full for 100 ticks. */
}
```

ISR 中非阻塞发送：

```c
void ADC_IRQHandler(void)
{
    app_msg_t msg = {
        .id = 1,
        .value = read_adc_value(),
    };

    (void)msgq_send(&app_msgq, &msg, OS_NO_WAIT);
}
```

接收消息：

```c
app_msg_t msg;

if (msgq_recv(&app_msgq, &msg, OS_WAIT_FOREVER)) {
    /* Process one message. */
}
```

查看队列状态：

```c
uint16_t used;
uint16_t free_slots;

used = msgq_count(&app_msgq);
free_slots = msgq_space(&app_msgq);
```

`msgq_send()` 在队列有空槽时立即拷贝消息；队列满时可以选择不阻塞、有限等待或无限等待。
`msgq_recv()` 在队列有消息时立即拷贝消息；队列空时可以选择不阻塞、有限等待或无限等待。

等待期间 msgq 不保存用户传入的 `msg` 指针。任务被唤醒后会重新尝试队列操作，
因此可以安全传入栈上消息对象。

ISR 中只能使用 `timeout == OS_NO_WAIT` 的非阻塞 msgq 调用。

多个 waiter 的情况：

- 多个任务可以同时等待发送，也可以同时等待接收。
- 当 send 成功后，如果有接收任务正在等待，会唤醒一个接收 waiter。
- 当 recv 成功后，如果有发送任务正在等待，会唤醒一个发送 waiter。
- 等待队列按 FIFO 选择一个 waiter 唤醒。
- 被唤醒任务并不是直接拿到消息或空槽，而是重新尝试 send/recv 操作。
- 实际运行顺序仍由调度器优先级和同优先级 FIFO 规则决定。

## 如何选择原语

先横向对比三个同步原语：

| 原语 | 保存的信息 | 等待对象 | 唤醒范围 | 是否计数 | 多 waiter 语义 | 典型用途 |
| --- | --- | --- | --- | --- | --- | --- |
| `task_notify` | 每个任务 1 个未处理通知状态 | 固定目标任务 | 只唤醒目标任务 | 不计数 | 没有共享 waiter 队列，多个 producer 可通知同一任务 | ISR/任务直接唤醒某个固定任务 |
| `eventset` | 32 个事件 bit | eventset 对象 | 唤醒所有条件满足的 waiter | 不计数，bit 是状态 | 多个 waiter 可同时观察同一状态，bit 需手动清除 | 多个 named condition、wait-any、wait-all |
| `sem` | token count | semaphore 对象 | 每次 give 最多唤醒一个 waiter | 计数到 limit | 已阻塞 waiter 按 FIFO 唤醒；被唤醒任务获得 token | 资源数量、事件次数、生产者/消费者计数 |

再根据数据传递需求选择：

- 只想唤醒某个固定任务：使用 `task_notify()`。
- 需要等待多个状态位：使用 `eventset`。
- 需要表示资源数量或事件 token：使用 `sem`。
- 需要传递定长数据，并希望 send/recv 可阻塞：使用 `msgq`。
- 需要传递字节流，并且是 SPSC 场景：使用 `pipe`。
- 需要在指定 tick 后或周期性触发非阻塞回调：启用 `OS_TIMER_ENABLE` 后使用 `timer`。
- 需要传递变长 packet，且是 SPSC 场景：使用 `packetbuf`。
- 只需要字节流缓存，不需要阻塞语义：使用 `bytebuf`。
- 需要管理固定大小对象池：使用 `slab`。

`packetbuf`、`bytebuf` 和 `slab` 不是内核阻塞对象。它们是通用模块，具体用法见各自头文件。
如果用 `bytebuf` 或 `packetbuf` 实现通信，需要额外选择一种同步原语来通知消费者有新数据。

## OS 调试

OS 调试分成三层：诊断断言、任务状态观察、trace hook。

### 调试开关

常用调试开关都在 `kernel_config.h` 中：

```c
#define OS_DIAGNOSTIC_ENABLE 1
#define TASK_STACK_WATERMARK_ENABLE 1
#define OS_TRACE_ENABLE 1
```

建议开发阶段按需打开，发布版本按实际开销关闭。

公共 API 的参数契约仍使用普通 `ASSERT` 检查，用于尽早暴露用户误用。
`OS_DIAGNOSTIC_ENABLE` 只控制 OS 内部一致性检查，这些检查主要用于内核开发和测试阶段。

### 任务状态观察

任务查询 API 可以用来打印日志或在调试器中观察任务状态：

```c
const char *name = task_get_name(task);
task_priority_t priority = task_get_priority(task);
task_state_t state = task_get_state(task);
task_wait_type_t wait = task_get_wait_type(task);
```

这些接口返回的是调用瞬间的快照。它们适合调试和观测，不适合用来实现复杂同步逻辑。

### 栈水位检测

启用 `TASK_STACK_WATERMARK_ENABLE` 后，可以估算任务历史最大栈使用量：

```c
size_t unused = task_get_stack_unused(task);
size_t used = task_get_stack_used(task);
```

这个功能会在 `task_init()` 时填充任务栈，因此只建议在调试阶段打开。
它不是栈溢出保护，只用于帮助评估任务栈大小是否合适。

### Trace Hook

trace hook 用于观察内核调度事件。启用 `OS_TRACE_ENABLE` 后，调度器会在关键事件发生时调用：

```c
void os_trace_task_ready(const task_t *task);
void os_trace_task_block(const task_t *task);
void os_trace_task_switch(const task_t *from, const task_t *to);
void os_trace_task_exit(const task_t *task);
```

默认实现为空。用户可以在自己的文件中定义同名函数覆盖默认 weak hook：

```c
void os_trace_task_switch(const task_t *from, const task_t *to)
{
    debug_log("switch %s -> %s",
              task_get_name(from),
              task_get_name(to));
}
```

trace hook 运行在内核关键路径中，通常处于关中断状态或 PendSV 上下文。
hook 必须短小、非阻塞，不能调用会阻塞的内核 API，也不应该修改 task 或 scheduler 状态。
推荐在 hook 中写入 RTT、ring buffer、计数器，或者翻转 GPIO。直接 `printf` 可能很慢，除非确认输出后端足够轻量。

第一版 trace 是工具无关的。后续可以在这些 hook 之上实现 SystemView、Tracealyzer 或 Event Recorder backend。

## 架构层和移植说明

架构层负责上下文切换、中断锁、idle wait 和 ISR 检测。普通用户通常不需要直接调用架构层 API；
把 MicaOS 接入一个 MCU 工程时，只需要选择正确的架构端口、接入 PendSV 和系统 tick。

完整步骤见 `porting_guide.md`。

当前 Cortex-M 端口：

- `ARMv6M`：Cortex-M0/M0+
- `ARMv7M`：Cortex-M3/M4/M7 整数上下文端口
- `ARMv7M_FPU`：Cortex-M4F/M7 硬件 FPU 端口

最终工程中只应该编译一个架构实现。

## 进阶：内部等待模型

这一节是实现说明，普通使用者可以跳过。

delay、notify、eventset、sem、pipe 和 msgq 都使用同一套内部等待模型。
当任务需要阻塞时，内核会记录等待原因和等待对象，把任务挂到对应对象的等待队列或等待槽上；
如果需要超时，还会把任务挂到全局 timeout list 上。

当等待条件满足时，等待对象负责把任务从自己的等待队列或等待槽移除，取消 timeout，
然后把任务重新放回 READY 队列。

当超时发生时，tick 模块负责把任务从等待对象中移除，设置失败结果，
然后把任务重新放回 READY 队列。

这个模型意味着：一个任务同一时刻只能等待一个对象。

## 内存与数据结构模块

`slab` 模块提供固定大小块分配器，由用户提供存储，不依赖动态内存。
它适合用来实现 task pool、timer pool、message pool 等对象池。

通用数据结构模块位于 `data_structure/`：

- `dlist`：侵入式双向链表
- `slist`：侵入式单向链表
- `bitmap`：固定大小 bitmap helper
- `bytebuf`：SPSC byte ring buffer
- `packetbuf`：SPSC packet buffer

这些模块既可以给内核使用，也可以给应用层使用。详细 API 和示例见各自头文件。
