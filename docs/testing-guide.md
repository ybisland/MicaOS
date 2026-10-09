# Testing Guide

This document records how MicaOS has been validated and how to repeat the main
tests.

## Test Types

MicaOS uses three levels of tests:

1. PC tests for pure data structures and logic.
2. MCU functional tests for context switching and wakeup behavior.
3. MCU stress tests for long-running scheduler and IPC behavior.

## PC Tests

PC tests live under:

```text
tests/
```

They are intended for modules that do not require Cortex-M context switching.

Typical targets:

- dlist
- slist
- bitmap
- bytebuf
- packetbuf
- slab
- bus logic where possible

Kernel blocking behavior still needs MCU validation because it depends on
context switching and interrupt behavior.

## MCU Boards Used

| Board | Core | Port | Notes |
| --- | --- | --- | --- |
| NUCLEO-F411RE | Cortex-M4 | ARMv7M | main functional and stress board |
| NUCLEO-F411RE | Cortex-M4F | ARMv7M_FPU | FPU context tests |
| STM32G070 board | Cortex-M0+ | ARMv6M | ARMv6-M validation |

## Functional Coverage

MCU functional tests covered:

- first task start
- PendSV switching
- priority preemption
- equal-priority FIFO cooperative scheduling
- `task_yield`
- `task_delay`
- `os_tick_advance`
- notification
- eventset
- semaphore
- msgq
- pipe
- soft timer
- idle task stack watermark
- FPU context preservation on F411 hard-float build
- bus event/state publish-subscribe behavior

## Stress Results

Known completed stress runs:

| Board | Test | Duration | Result |
| --- | --- | --- | --- |
| F411 | IPC/timer/scheduler stability | 4 h | pass, no FAIL/ASSERT |
| F411 | high-frequency scheduler stress | 2 h | pass, no FAIL/ASSERT |
| F411 | FPU stress | 2 h | pass, no FAIL/ASSERT |
| F411 | bus stress | 1 h | pass, no FAIL/ASSERT |
| G070 | scheduler stress | 40 min | pass, no FAIL/ASSERT |
| G070 | bus stress | 40 min | pass, no FAIL/ASSERT |

## Serial Log Pattern

Stress firmware prints periodic status lines and final scripts count:

- `FAIL`
- `ASSERT`
- `OK`

A successful run has:

```text
FAIL count: 0
ASSERT count: 0
```

and counters continue increasing.

## When to Re-run MCU Tests

Re-run MCU tests after changing:

- architecture context switching
- scheduler
- task state/wait logic
- timeout logic
- IPC wait/wakeup paths
- FPU save/restore
- `micaos_config.h` options that affect kernel behavior
- bus event/state backend

PC-only tests are not enough for these areas.
