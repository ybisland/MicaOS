# MicaOS

MicaOS 是一个面向低成本 32 位 MCU 的小型静态 OS。它不依赖动态内存，任务控制块、任务栈、队列缓冲区等对象都由用户提供存储。

## 特点

- 静态优先级调度：数字越小优先级越高。
- 抢占式+协作式调度（不支持时间片轮转）。
- 不同优先级之间支持抢占，同优先级任务协作式调度。
- 内置 task notification、eventset、counting semaphore、msgq、SPSC pipe。
- 可选 soft timer、trace hook、任务栈水位估算。
- service 层提供同步 message bus，用于静态发布订阅通信。
- 支持架构：ARMv6-M、ARMv7-M、ARMv7-M FPU。

## 用户文档

1. `docs/kernel-guide.md`：任务、调度、tick、同步和通信 API 的用法。
2. `docs/porting-guide.md`：把 MicaOS 接入新的 MCU 工程。
3. `docs/message-bus-guide.md`：Event/State Bus 的静态发布订阅用法。
4. 各模块头文件：查看更精确的参数约束和模块级说明。

## 构建方式

MicaOS 支持两种集成方式：

1. CMake：把 `MicaOS/` 作为子目录加入用户工程。

    ```cmake
    add_subdirectory(path/to/MicaOS)
    target_link_libraries(app PRIVATE MicaOS::micaos)
    ```

2. 手动工程：Keil、IAR 或其他 IDE 工程中手动加入 MicaOS `.c` 文件。

两种方式都使用同一个配置文件：

```text
MicaOS/config.h
```

用户通过 `config.h` 选择架构端口、timer、trace、诊断等选项。CMake 脚本只负责加入源码和头文件路径，不单独维护另一套 OS 配置。

## 资源占用

测试环境：
- STM32G070RBTx
- `arm-none-eabi-gcc 15.2.1`、`nano.specs`、`-g0`
- 诊断和 trace 关闭，定义 `NDEBUG`

| 优化 | 内核功能配置 | Kernel only ROM | Kernel only RAM | Kernel + bus ROM | Kernel + bus RAM |
| --- | --- | ---: | ---: | ---: | ---: |
| `-Os` | minimal | 4013 B | 300 B | 5087 B | 300 B |
| `-O1` | minimal | 4895 B | 300 B | 6131 B | 300 B |
| `-O2` | minimal | 4809 B | 300 B | 6153 B | 300 B |
| `-Os` | timer+watermark | 4387 B | 308 B | 5461 B | 308 B |
| `-O1` | timer+watermark | 5341 B | 308 B | 6577 B | 308 B |
| `-O2` | timer+watermark | 5253 B | 308 B | 6597 B | 308 B |

说明：

- `minimal`：`OS_TIMER_ENABLE=0`，
  `TASK_STACK_WATERMARK_ENABLE=0`。
- `timer/watermark`：soft timer 和任务栈水位估算开启。
- `Kernel + bus`：在 `Kernel only` 基础上加入 `service/bus/bus.c`。

## TODO
- 支持更多Trace Backend
- Skills
