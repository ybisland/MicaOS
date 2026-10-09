---
name: micaos
description: Use this skill when helping a user use, configure, port, debug, test, document, or extend MicaOS. Applies to MicaOS tasks, scheduler, OS tick, notification, eventset, semaphore, msgq, pipe, software timer, trace, slab, data structures, message bus, Cortex-M architecture ports, and STM32 validation.
---

# MicaOS Skill

Use this skill whenever the conversation touches the MicaOS project.

MicaOS is a small static RTOS for low-resource 32-bit MCUs. The AI agent's job
is to help the user use MicaOS correctly: choose the right capability, follow
the documented contracts, configure the OS, write examples, port it, debug it,
or extend it without breaking its core model.

## Read First

Start with the ability catalog:

1. `skills/micaos/references/micaos-ability-catalog.md`
2. `docs/index.md`

Then read the guide that matches the user's goal:

- First use: `docs/getting-started.md`
- Configuration and builds: `docs/configuration-and-build.md`
- Tasks, scheduler, tick, delay, notification: `docs/kernel-guide.md`
- Eventset, semaphore, msgq, pipe, timer: `docs/ipc-guide.md`
- Slab and data structures: `docs/memory-and-data-structures.md`
- Message bus service: `docs/message-bus-guide.md`
- Debugging and diagnostics: `docs/debugging-guide.md`
- Porting to a board or architecture: `docs/porting-guide.md`
- Testing on PC or MCU: `docs/testing-guide.md`
- Internal design changes: `docs/architecture-notes.md`

Before using a module, also read its header. Public contracts are usually
documented in the header.

## How to Help Users Use MicaOS

- Use `micaos/kernel.h` as the public kernel entry point.
- Use `micaos/service/bus.h` for the message bus service.
- Do not add `trace.h` to `kernel.h` unless the trace design is explicitly
  changed.
- Assume MicaOS uses static allocation; do not introduce dynamic allocation.
- Keep examples based on caller-owned task objects, stacks, buffers, queues,
  slabs, and bus storage.
- Use the shared timeout model: `OS_NO_WAIT`, finite ticks, or
  `OS_WAIT_FOREVER`.
- Finite timeout values must be `<= OS_TICK_MAX_DELAY`.
- ISR code may wake tasks or use documented non-blocking APIs, but must not
  call blocking wait APIs.
- Avoid suggesting mutex or priority inheritance unless the user explicitly
  reopens that design.

## Scheduling Model

MicaOS uses static priorities.

- A READY task with a higher priority preempts the current task.
- Tasks with the same priority do not time-slice.
- Same-priority tasks run in FIFO order only when the current task yields,
  blocks, waits, delays, or exits.
- A high-priority task that stays READY forever can starve lower-priority
  tasks.

This scheduling rule affects API choice. Use blocking APIs when a task should
stop running until an event happens. Use `task_yield()` only when same-priority
tasks should get a chance to run.
