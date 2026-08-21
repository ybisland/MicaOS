# MicaOS 移植指南

本文面向想把 MicaOS 接入自己 MCU 工程的使用者。目标是让你知道需要配置哪些文件、编译哪些源码、接入哪些中断，以及最小启动流程应该怎么写。

如果只是学习内核 API 的使用方式，请先看 `os_guide.md`。

## 移植时你需要做什么

把 MicaOS 接入一个工程时，通常只需要完成下面几件事：

1. 把 `MicaOS/` 加入头文件搜索路径。
2. 配置 `MicaOS/config.h`，包括目标架构端口和需要启用的功能。
3. 用 CMake 或手动工程加入 MicaOS 源文件。
4. 确保 `PendSV_Handler` 使用 MicaOS 提供的实现。
5. 在系统 tick 中断里调用 `os_tick_advance()`。
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

## 编译器要求

MicaOS 最低支持 GNU C99，默认使用 GNU C11 版本。CMake 集成会为 `MicaOS::micaos` 设置 C11，并开启 C extensions。若使用C99需要手动修改CMakeList.txt。

手动集成 Keil、IAR 或其他 IDE 工程时，也需要选择等价的 GNU C11 模式。例如 GCC/Clang 工程通常使用：

```text
-std=gnu11
```

MicaOS 使用少量 GNU 风格扩展，例如 `__typeof__` 和编译器属性宏。目标编译器需要支持 GCC、Clang 或 ARM Compiler 风格的 GNU C 扩展。

## 选择架构端口

在 `MicaOS/config.h` 中配置 `MICAOS_ARCH_PORT`：

```c
#define MICAOS_ARCH_PORT MICAOS_ARCH_PORT_ARMV7M
```

当前 Cortex-M 端口的选择规则如下：

| 目标 CPU | 配置值 | 端口文件 |
| --- | --- | --- |
| Cortex-M0 / M0+ | `MICAOS_ARCH_PORT_ARMV6M` | `arch/CortexM/ARMv6M/arch_context.c` |
| Cortex-M3 / M4 / M7，不使用硬件 FPU ABI | `MICAOS_ARCH_PORT_ARMV7M` | `arch/CortexM/ARMv7M/arch_context.c` |
| Cortex-M4F / M7，并且工程启用了硬件 FPU ABI | `MICAOS_ARCH_PORT_ARMV7M_FPU` | `arch/CortexM/ARMv7M_FPU/arch_context.c` |

注意：M4F/M7 有 FPU 硬件并不等于工程一定使用硬件 FPU ABI。只有当编译选项启用了硬件浮点调用约定时，才应该选择 `ARMv7M_FPU` 端口。否则使用普通 `ARMv7M` 端口更简单。

每个架构端口文件内部都会根据 `MICAOS_ARCH_PORT` 自裁剪。因此 CMake 或 Keil/IAR 工程可以把所有
`arch_context.c` 都加入编译，最终只有选中的端口会生成 `PendSV_Handler`、`SVC_Handler` 和上下文切换代码。

如果你选择只手动加入一个 `arch_context.c`，也可以；但这个文件必须和 `MICAOS_ARCH_PORT` 保持一致。

## PendSV 接入

MicaOS 的 Cortex-M 端口使用 PendSV 完成上下文切换。你需要确保最终向量表中的 `PendSV_Handler` 来自所选择的 MicaOS 架构端口。

常见情况：

- 如果启动文件里 `PendSV_Handler` 是 weak 符号，直接编译 MicaOS 的 `arch_context.c` 通常就能覆盖它。
- 如果工程里已经有一个强定义的 `PendSV_Handler`，需要移除它，或者改成调用 MicaOS 的实现。
- 如果同时编译多个 Cortex-M 架构端口，必须确保 `MICAOS_ARCH_PORT` 已经正确配置。

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

## MicaOS 配置

MicaOS 全局配置集中在：

```text
MicaOS/config.h
```

推荐直接编辑这个文件。这样 CMake、Keil、IAR 或其他构建系统都使用同一套配置。

最重要的规则是：所有 MicaOS 源文件必须看到同一套配置。不要让不同 `.c` 文件使用不同的
`MICAOS_ARCH_PORT`、`OS_TIMER_ENABLE`、`SCHED_PRIORITY_LEVELS` 等值。

常用配置：

| 配置 | 作用 |
| --- | --- |
| `MICAOS_ARCH_PORT` | 选择架构上下文切换端口 |
| `SCHED_PRIORITY_LEVELS` | 任务优先级数量，范围 `1..32`，数字越小优先级越高 |
| `SCHED_IDLE_STACK_SIZE` | OS 内部 idle task 栈大小，单位 byte，必须 8 字节对齐 |
| `OS_TIMER_ENABLE` | 是否启用 soft timer |
| `OS_DIAGNOSTIC_ENABLE` | 是否启用 MicaOS 详细诊断断言 |
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

## CMake 构建

如果用户工程使用 CMake，可以把 MicaOS 作为一个子目录加入：

```cmake
add_subdirectory(path/to/MicaOS)
target_link_libraries(app PRIVATE MicaOS::micaos)
```

MicaOS 的 CMakeLists 只负责加入源码和 include path，不重新定义 OS 行为选项。
用户仍然通过 `MicaOS/config.h` 选择架构、timer、trace、诊断等配置。

当前 CMakeLists 会把 MicaOS 的 `.c` 文件加入静态库，包括所有 Cortex-M 架构端口文件。
具体哪个端口生成代码由 `MICAOS_ARCH_PORT` 决定。

## 手动构建文件建议

如果使用 Keil、IAR 或其他手动工程，通常把下面这些文件加入工程：

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
data_structure/packetbuf.c
memory/slab.c
service/bus/bus.c
```

`pipe` 依赖 `bytebuf.c`，所以使用 pipe 时需要把它一起加入编译。

然后加入架构端口文件。可以全部加入：

```text
arch/CortexM/ARMv6M/arch_context.c
arch/CortexM/ARMv7M/arch_context.c
arch/CortexM/ARMv7M_FPU/arch_context.c
```

也可以只加入 `MICAOS_ARCH_PORT` 对应的那个端口文件。

如果启用 soft timer，还需要编译：

```text
kernel/timer.c
```

`timer.c` 内部受 `OS_TIMER_ENABLE` 保护，因此一直加入编译也是安全的。

header-only 模块如 `dlist.h`、`slist.h`、`bitmap.h` 不需要单独编译。

## 最小移植检查表

第一次接入时可以按下面检查：

- `MicaOS/` 已加入 include path。
- 用户代码包含 `kernel/kernel.h`。
- 所有 MicaOS 源文件看到同一套 `config.h` 配置。
- `MICAOS_ARCH_PORT` 已经选择正确架构。
- 架构端口文件已经加入编译；如果只加入一个端口，它必须和 `MICAOS_ARCH_PORT` 一致。
- 向量表中的 `PendSV_Handler` 来自 MicaOS。
- SysTick 或其他周期中断调用了 `os_tick_advance()`。
- 每个任务都使用用户提供的静态 `task_t` 和任务栈。
- 任务栈使用 `task_stack()` 声明，或者手动保证 8 字节对齐。
- `scheduler_init()` 在 `task_init()` / `scheduler_add()` 之前调用。
- `scheduler_start()` 在所有初始任务加入后调用，并且不期望它返回。

如果这些都满足，就可以开始做最小任务切换测试。
