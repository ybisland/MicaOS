# MicaOS

MicaOS is a small, static, predictable RTOS for low-resource 32-bit MCUs.

It is designed for projects that prefer explicit static allocation, simple
kernel behavior, and low runtime overhead over a large feature set.

## Main Features

- Static allocation: task control blocks, task stacks, queues, and buffers are
  caller-owned.
- Predictable scheduling: static priorities, higher-priority preemption,
  equal-priority FIFO at cooperative points, and no time slicing.
- Kernel primitives: notification, eventset, semaphore, message queue, pipe,
  soft timer, trace hooks.
- Common modules: slab, dlist, slist, bitmap, bytebuf, packetbuf.
- Application service: event/state bus.
- Cortex-M ports for ARMv6-M, ARMv7-M, and ARMv7-M hard-float builds.

## Current Target

MicaOS currently targets 32-bit MCUs.

Verified boards so far:

| Board | Core | Port | Result |
| --- | --- | --- | --- |
| NUCLEO-F411RE | Cortex-M4 | ARMv7M | Functional and stress tested |
| NUCLEO-F411RE | Cortex-M4F | ARMv7M_FPU | Functional and FPU stress tested |
| STM32G070 board | Cortex-M0+ | ARMv6M | Functional and stress tested |

## Documentation

Start here:

- [Documentation index](docs/index.md)
- [Getting started](docs/getting-started.md)
- [Configuration and build](docs/configuration-and-build.md)
- [Kernel guide](docs/kernel-guide.md)
- [IPC guide](docs/ipc-guide.md)
- [Message bus guide](docs/message-bus-guide.md)
- [Porting guide](docs/porting-guide.md)

For maintainers:

- [Architecture notes](docs/architecture-notes.md)
- [Testing guide](docs/testing-guide.md)

For AI agents:

- [MicaOS skill](skills/micaos/SKILL.md)
- [MicaOS ability catalog](skills/micaos/references/micaos-ability-catalog.md)

## Build Model

Configuration is centralized in:

```text
config/micaos_config.h
```

MicaOS can be built either by CMake or by manually adding source files to
Keil, IAR, STM32CubeIDE, Makefile, or another build system.

For CMake projects, add MicaOS as a subdirectory and link its target:

```cmake
add_subdirectory(path/to/MicaOS)
target_link_libraries(app PRIVATE MicaOS::micaos)
```

The target publishes `include` and `config` as compile-time include paths, so
application code can include
the namespaced public API:

```c
#include <micaos/kernel.h>
```

The static library owns the implementation sources under `src`;
architecture and feature selection still comes from
`config/micaos_config.h`.

The current minimum language mode is GNU C11. Cortex-M ports also require
compiler support for inline assembly and common compiler attributes.

For details, see [Configuration and build](docs/configuration-and-build.md).

## Todo
- .clang-format
- trace
