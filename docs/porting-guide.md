# MicaOS 移植指南

本文面向想把 MicaOS 接入自己 MCU 工程的使用者。目标是让你知道需要配置哪些文件、编译哪些源码、接入哪些中断，以及最小启动流程应该怎么写。

如果只是学习内核 API 的使用方式，请先看 `os_guide.md`。

## 移植时你需要做什么

把 MicaOS 接入一个工程时，通常只需要完成下面几件事：

1. 把 `MicaOS/` 加入头文件搜索路径。
2. 根据目标 CPU 选择并编译一个架构端口。
3. 确保 `PendSV_Handler` 使用 MicaOS 提供的实现。
4. 在系统 tick 中断里调用 `os_tick_advance()`。
5. 配置 `kernel_config.h` 里的内核选项。
6. 创建任务、加入调度器、调用 `scheduler_start()`。

普通应用代码不需要直接调用 `arch_context_init()`、`arch_context_start()` 或其他架构层函数。这些接口由 task 和 scheduler 模块内部使用。

## 头文件路径

推荐把 `MicaOS/` 目录本身加入 include path：

```text
-I path/to/MicaOS
```

之后用户代码通常只需要包含一个总入口：

```c
#include "kernel/kernel.h"
```

如果你单独使用数据结构或 slab，也可以直接包含对应模块头文件，例如：

```c
#include "data_structure/dlist.h"
#include "memory/slab.h"
```

## 选择架构端口

最终工程中只应该编译一个 `arch_context.c`。当前 Cortex-M 端口的选择规则如下：

| 目标 CPU | 建议端口 |
| --- | --- |
| Cortex-M0 / M0+ | `arch/CortexM/ARMv6M/arch_context.c` |
| Cortex-M3 / M4 / M7，不使用硬件 FPU ABI | `arch/CortexM/ARMv7M/arch_context.c` |
| Cortex-M4F / M7，并且工程启用了硬件 FPU ABI | `arch/CortexM/ARMv7M_FPU/arch_context.c` |

注意：M4F/M7 有 FPU 硬件并不等于工程一定使用硬件 FPU ABI。只有当编译选项启用了硬件浮点调用约定时，才应该选择 `ARMv7M_FPU` 端口。否则使用普通 `ARMv7M` 端口更简单。

## PendSV 接入

MicaOS 的 Cortex-M 端口使用 PendSV 完成上下文切换。你需要确保最终向量表中的 `PendSV_Handler` 来自所选择的 MicaOS 架构端口。

常见情况：

- 如果启动文件里 `PendSV_Handler` 是 weak 符号，直接编译 MicaOS 的 `arch_context.c` 通常就能覆盖它。
- 如果工程里已经有一个强定义的 `PendSV_Handler`，需要移除它，或者改成调用 MicaOS 的实现。
- 不要同时编译多个 Cortex-M 架构端口，否则会出现重复的 `PendSV_Handler`。

PendSV 优先级应该设为最低。当前 Cortex-M 端口会在 `arch_context_start()` 中设置 PendSV 优先级，普通用户通常不需要额外处理。

## Tick 接入

OS tick 是 MicaOS 的时间基础。你需要选择一个周期性中断源，通常是 SysTick，并在中断里调用：

```c
void SysTick_Handler(void)
{
    os_tick_advance();
}
```

每调用一次 `os_tick_advance()`，OS tick 增加 1，并唤醒已经到期的 `task_delay()` 任务。如果启用了 `OS_TIMER_ENABLE`，它也会处理到期 soft timer。

tick 周期由你的工程决定。常见选择是 1 ms 一次，此时 `task_delay(10)` 表示大约延时 10 ms。

## 内核配置

内核配置集中在：

```c
#include "kernel/kernel_config.h"
```

你可以用编译选项覆盖配置，例如：

```text
-DSCHED_PRIORITY_LEVELS=16U
-DOS_TIMER_ENABLE=1
-DOS_TRACE_ENABLE=1
```

也可以在工程配置头文件中定义这些宏，但要确保该配置头在所有内核头文件之前生效。

最重要的规则是：所有 MicaOS 源文件必须看到同一套配置。不要让不同 `.c` 文件使用不同的 `OS_TIMER_ENABLE`、`SCHED_PRIORITY_LEVELS` 等值。

常用配置：

| 配置 | 作用 |
| --- | --- |
| `SCHED_PRIORITY_LEVELS` | 任务优先级数量，范围 `1..32`，数字越小优先级越高 |
| `SCHED_IDLE_STACK_SIZE` | OS 内部 idle task 栈大小，单位 byte，必须 8 字节对齐 |
| `OS_TIMER_ENABLE` | 是否启用 soft timer |
| `OS_DIAGNOSTIC_ENABLE` | 是否启用 OS 内部诊断断言 |
| `TASK_STACK_WATERMARK_ENABLE` | 是否启用任务栈水位估算 |
| `OS_TRACE_ENABLE` | 是否启用通用 trace hook |

## 任务栈

MicaOS 不分配任务栈。每个任务的 `task_t` 和栈都由用户静态提供。

推荐使用 `task_stack()` 声明任务栈：

```c
static task_t worker;
static task_stack(worker_stack, 512);
```

`task_stack()` 会保证 Cortex-M 需要的 8 字节对齐。`size` 单位是 byte。

初始化任务：

```c
task_init(&worker,
          "worker",
          worker_entry,
          NULL,
          worker_stack,
          sizeof(worker_stack),
          0);
```

任务栈大小需要同时容纳：

- 架构端口构造的初始上下文帧
- 任务运行时的函数调用深度
- 中断或库函数可能带来的额外栈使用

如果不确定栈大小，可以先开大一些。之后打开 `TASK_STACK_WATERMARK_ENABLE` 观察历史栈使用量，再逐步调整。

## 最小启动流程

一个最小应用大致如下：

```c
#include "kernel/kernel.h"

static task_t worker;
static task_stack(worker_stack, 512);

static void worker_entry(void *arg)
{
    (void)arg;

    for (;;) {
        do_work();
        task_delay(10);
    }
}

int main(void)
{
    board_init();

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

`scheduler_start()` 不会返回。它会启动最高优先级 READY 任务；如果没有用户任务可运行，OS 会运行内部 idle task。

## Idle hook

MicaOS 内部拥有 idle task 和 idle stack。用户不需要自己创建 idle task。

如果需要喂狗、进入低功耗或做空闲统计，可以覆盖 weak hook：

```c
void scheduler_idle_hook(void)
{
    feed_watchdog();
    enter_sleep_mode();
}
```

默认实现会等待中断。

Idle stack 大小由 `SCHED_IDLE_STACK_SIZE` 控制，默认是 128 bytes。这个默认值只适用于最小 idle 行为：默认 hook，或者用户 hook 中只调用 `__WFI()` / `arch_wait_for_interrupt()`。

在 NUCLEO-F411RE、Debug、`TASK_STACK_WATERMARK_ENABLE=1` 的实测中，WFI-only idle 水位如下：

```text
ARMv7M soft-float:
SCHED_IDLE_STACK_SIZE=128: used 88, unused 40
SCHED_IDLE_STACK_SIZE=256: used 88, unused 168
SCHED_IDLE_STACK_SIZE=512: used 88, unused 424

ARMv7M_FPU hard-float:
SCHED_IDLE_STACK_SIZE=128: used 92, unused 36
```

因此，移植时如果 idle hook 只等待中断，可以先使用默认 128 bytes。如果你在 idle hook 中调用 HAL 低功耗函数、喂狗、打印日志或执行其他逻辑，应当先增大 `SCHED_IDLE_STACK_SIZE`，并打开 `TASK_STACK_WATERMARK_ENABLE` 后用 `scheduler_idle_stack_used()` / `scheduler_idle_stack_unused()` 重新测量。

## 中断里能做什么

ISR 中可以唤醒任务，但不能调用会阻塞的 API。

常见允许用法：

```c
task_notify(&worker);
eventset_set(&events, RX_READY);
sem_give(&rx_sem);
msgq_send(&queue, &msg, OS_NO_WAIT);
pipe_write(&pipe, data, len, OS_NO_WAIT);
```

不要在 ISR 中调用：

```c
task_delay(1);
task_notify_wait(OS_WAIT_FOREVER);
sem_take(&sem, 10);
msgq_recv(&queue, &msg, 10);
pipe_read(&pipe, buf, sizeof(buf), 10);
```

带 timeout 的通信和同步 API 在 ISR 中只能使用 `OS_NO_WAIT`。

## 编译文件建议

如果你想使用当前 kernel 的常用功能，通常编译下面这些文件：

```text
kernel/task.c
kernel/scheduler.c
kernel/time.c
kernel/eventset.c
kernel/sem.c
kernel/msgq.c
kernel/pipe.c
kernel/trace.c
common/assert.c
data_structure/bytebuf.c
```

`pipe` 依赖 `bytebuf.c`，所以使用 pipe 时需要把它一起加入编译。

然后再加上一个架构端口，例如：

```text
arch/CortexM/ARMv6M/arch_context.c
```

如果启用 soft timer，还需要编译：

```text
kernel/timer.c
```

`timer.c` 内部也受 `OS_TIMER_ENABLE` 保护；但从工程组织上看，启用 timer 时把它加入编译列表最清晰。

如果使用 slab 或 packetbuf，再额外编译：

```text
memory/slab.c
data_structure/packetbuf.c
```

header-only 模块如 `dlist.h`、`slist.h`、`bitmap.h` 不需要单独编译。

## 最小移植检查表

第一次接入时可以按下面检查：

- `MicaOS/` 已加入 include path。
- 用户代码包含 `kernel/kernel.h`。
- 所有 MicaOS 源文件看到同一套 `kernel_config.h` 配置。
- 只编译了一个 `arch_context.c`。
- 向量表中的 `PendSV_Handler` 来自 MicaOS。
- SysTick 或其他周期中断调用了 `os_tick_advance()`。
- 每个任务都使用用户提供的静态 `task_t` 和任务栈。
- 任务栈使用 `task_stack()` 声明，或者手动保证 8 字节对齐。
- `scheduler_init()` 在 `task_init()` / `scheduler_add()` 之前调用。
- `scheduler_start()` 在所有初始任务加入后调用，并且不期望它返回。

如果这些都满足，就可以开始做最小任务切换测试。
