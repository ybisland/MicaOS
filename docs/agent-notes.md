# Agent Notes

This document is a compact ability catalog for AI agents working with MicaOS.
Use it to quickly decide which MicaOS capability fits a request and where to
read the detailed user-facing documentation.

## What MicaOS Provides

MicaOS is a small static RTOS for low-resource 32-bit MCUs. It provides:

- a static-priority task scheduler
- OS tick, delay, timeout, and optional software timers
- lightweight synchronization primitives
- fixed-size and byte-stream communication primitives
- a static slab allocator
- intrusive lists, bitmap, byte buffer, and packet buffer utilities
- an application-level event/state message bus
- optional diagnostics, stack watermark, and trace hooks
- Cortex-M context-switch ports

## Ability Catalog

| Situation | Use | Main API/Header | Detailed Document |
| --- | --- | --- | --- |
| Create tasks and start the OS | Kernel scheduler and task API | `kernel/kernel.h` | `docs/getting-started.md`, `docs/kernel-guide.md` |
| Select architecture, enable features, or build the OS | Central configuration | `MicaOS/config.h` | `docs/configuration-and-build.md` |
| Port to Cortex-M0/M0+, M3/M4/M7, or FPU targets | Architecture context port | `arch/arch_context.h` | `docs/porting-guide.md` |
| Delay the current task or advance system time from SysTick | OS tick and delay | `kernel/time.h` through `kernel/kernel.h` | `docs/kernel-guide.md` |
| Wake one specific task | Task notification | `kernel/task.h` through `kernel/kernel.h` | `docs/kernel-guide.md`, `docs/ipc-guide.md` |
| Wait for bit flags from one or more producers | Event set | `kernel/eventset.h` through `kernel/kernel.h` | `docs/ipc-guide.md` |
| Count resources or synchronize producer/consumer availability | Counting semaphore | `kernel/sem.h` through `kernel/kernel.h` | `docs/ipc-guide.md` |
| Transfer fixed-size messages between tasks | Message queue | `kernel/msgq.h` through `kernel/kernel.h` | `docs/ipc-guide.md` |
| Transfer a byte stream between one writer and one reader | SPSC pipe | `kernel/pipe.h` through `kernel/kernel.h` | `docs/ipc-guide.md` |
| Run callbacks from OS tick time | Optional software timer | `kernel/timer.h` through `kernel/kernel.h` | `docs/ipc-guide.md` |
| Allocate fixed-size objects without heap | Slab allocator | `memory/slab.h` | `docs/memory-and-data-structures.md` |
| Embed nodes in owner objects | Doubly/singly intrusive lists | `data_structure/dlist.h`, `data_structure/slist.h` | `docs/memory-and-data-structures.md` |
| Track fixed bit sets or priority-ready state | Bitmap | `data_structure/bitmap.h` | `docs/memory-and-data-structures.md` |
| Buffer raw bytes without kernel blocking semantics | SPSC byte ring buffer | `data_structure/bytebuf.h` | `docs/memory-and-data-structures.md` |
| Buffer variable-size packets without kernel blocking semantics | SPSC packet buffer | `data_structure/packetbuf.h` | `docs/memory-and-data-structures.md` |
| Decouple application modules with publish-subscribe | Message bus service | `service/bus/bus.h` | `docs/message-bus-guide.md` |
| Diagnose API misuse, stack usage, or scheduling behavior | Assertions, diagnostics, stack watermark, trace | `config.h`, `kernel/trace.h` | `docs/debugging-guide.md` |
| Run or extend PC/MCU verification | Test suites and MCU plans | `tests/` | `docs/testing-guide.md` |

## Choosing Between Similar Abilities

- Use `task_notify()` when one task directly wakes another task.
- Use `eventset` when several independent bit flags may wake one or more
  waiters.
- Use `sem` when tasks compete for a countable token.
- Use `msgq` for fixed-size messages with built-in blocking semantics.
- Use `pipe` for a single byte stream between one producer and one consumer.
- Use `bytebuf` or `packetbuf` when you want the data structure only and will
  provide synchronization yourself.
- Use `service/bus` when the goal is application-level decoupling rather than
  a low-level synchronization primitive.

## Basic Scheduling Rule

MicaOS uses static priorities. A READY task with a higher priority preempts the
current task. Tasks with the same priority do not time-slice; they run in FIFO
order only when the current task yields, blocks, waits, delays, or exits.

This rule affects API choice:

- Use `task_yield()` only when same-priority tasks should get a chance to run.
- Use blocking APIs when a task should stop running until data, tokens, flags,
  notifications, or timeouts become available.
- Avoid keeping a high-priority task READY forever unless lower-priority tasks
  are allowed to starve.

## Important Contracts

- MicaOS does not allocate dynamic memory internally.
- Task control blocks, task stacks, queues, buffers, slabs, and bus storage are
  caller-owned unless a module explicitly documents otherwise.
- Blocking APIs use the same timeout model: `OS_NO_WAIT`, finite ticks, or
  `OS_WAIT_FOREVER`.
- Finite timeout values must be `<= OS_TICK_MAX_DELAY`.
- ISR code may wake tasks or use documented non-blocking APIs, but must not
  call blocking wait APIs.
- Equal-priority tasks do not time-slice; they switch only when the running
  task yields, blocks, waits, delays, or exits.
- MicaOS currently has no mutex or priority inheritance by design.

## Public Entry Points

Kernel users include:

```c
#include "kernel/kernel.h"
```

Bus users include:

```c
#include "service/bus/bus.h"
```

Do not add `trace.h` to `kernel.h` unless the trace design is explicitly
changed.

## Diagnostic Model

- `ASSERT` is reserved for application code.
- `OS_ASSERT` checks public MicaOS API misuse.
- `OS_DIAG_ASSERT` enables deeper optional diagnostics when
  `OS_DIAGNOSTIC_ENABLE` and `ASSERT_DEBUG` are both enabled.

Use `OS_ASSERT` for contracts users must obey. Use `OS_DIAG_ASSERT` only for
deeper checks that are useful while investigating integration bugs.
