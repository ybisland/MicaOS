# MicaOS MCU Test Plan

This file is the working checklist for MCU-side validation. It is mainly for
tracking what still needs to be verified on real hardware.

Current primary board:

- STM32G070, Cortex-M0+, ARMv6-M
- DAP-Link + pyOCD flashing
- USART3 on PD8/PD9, 115200 baud, COM26

## Test Policy

- PC tests cover pure data structure and object logic.
- MCU tests focus on hardware-dependent kernel behavior:
  context switching, SysTick, PendSV, ISR wakeup, task resume, and real stack
  behavior.
- Run tests in batches. Do not try to validate every primitive in one firmware
  image before the core scheduling path is proven.

## A. Basic Board Link

- [x] UART banner is printed after reset.
- [x] UART heartbeat is printed periodically.
- [x] SysTick calls `os_tick_advance()`.
- [x] A forced context switch reaches PendSV without faulting.

## B. Task Start And Context Switch

- [x] `scheduler_start()` enters the first task entry function.
- [x] Task entry receives the correct argument.
- [x] Two same-priority tasks alternate with `task_yield()`.
- [x] Three tasks repeatedly yield for a stress loop without faulting.
- [x] A task that returns enters the task-exit path instead of running into
      invalid code.

## C. Priority Scheduling

- [x] A higher-priority ready task preempts a lower-priority task at the next
      scheduling point.
- [x] A same-priority ready task does not immediately preempt the current task.
- [x] Same-priority tasks keep FIFO ordering.
- [x] The internal idle task runs when all user tasks are blocked.
- [x] The idle hook can be overridden and is called.

## D. Delay And Tick Wakeup

- [x] `task_delay(0)` behaves like `task_yield()`.
- [x] `task_delay(1)` resumes after at least one OS tick.
- [x] Tasks with different delay values wake in deadline order.
- [x] Multiple tasks with the same wake tick are all woken.
- [x] Long delays, such as 1s and 2s, are approximately correct.

## E. Task Notification

- [x] A waiting task is woken by `task_notify()`.
- [x] Notification sent before wait is not lost.
- [x] `task_notify_wait(OS_NO_WAIT)` checks once and returns immediately.
- [x] `task_notify_wait(timeout)` returns false on timeout.
- [x] ISR-side `task_notify()` wakes the target task according to scheduler
      rules.

## F. Event Set

- [x] Wait-any succeeds when any requested bit is set.
- [x] Wait-all succeeds only when all requested bits are set.
- [x] Manual clear works; waiting again blocks or times out.
- [x] Multiple tasks waiting on the same event set are handled as documented.
- [x] ISR-side event set wakes waiting tasks.

## G. Semaphore

- [x] Initial count can be taken.
- [x] Taking when count is zero blocks the task.
- [x] `sem_give()` wakes one waiter.
- [x] Multiple waiters competing for one token follow scheduler priority and
      ready-queue rules.
- [x] `OS_NO_WAIT` and finite timeout behavior are correct.
- [x] ISR-side `sem_give()` wakes a waiting task.

## H. Message Queue

- [x] Send and receive one message.
- [x] Multiple messages are received in FIFO order.
- [x] Sending to a full queue blocks until a receiver frees space.
- [x] Receiving from an empty queue blocks until a sender provides data.
- [x] `OS_NO_WAIT` and finite timeout behavior are correct.
- [x] Basic multi-producer and multi-consumer competition behaves as documented.

## I. Pipe

- [x] SPSC write/read works.
- [x] Byte order is preserved across repeated operations.
- [x] Buffer wrap-around preserves data.
- [x] Full pipe blocks writer and wakes it after reader consumes data.
- [x] Empty pipe blocks reader and wakes it after writer provides data.
- [x] `OS_NO_WAIT` and finite timeout behavior are correct.

## J. Soft Timer

- [x] One-shot timer fires once.
- [x] Periodic timer fires repeatedly.
- [x] Stopped timer no longer fires.
- [x] Multiple timers fire in deadline order.
- [x] Firmware builds and runs with `OS_TIMER_ENABLE=0`.
- [x] Firmware builds and runs with `OS_TIMER_ENABLE=1`.

## K. Debug And Trace

- [x] Trace disabled build runs normally.
- [x] Trace enabled build emits task switch, ready, block, and wake events.
- [x] Stack watermark returns plausible values after tasks run.
- [x] `OS_DIAGNOSTIC_ENABLE=1` catches intentional misuse in debug firmware.
- [x] `OS_DIAGNOSTIC_ENABLE=0` keeps normal firmware behavior unchanged.

## ARMv6-M / STM32G070 Result

- [x] A-K functional tests passed on STM32G070RBTx, Cortex-M0+.
- [x] Default build passed.
- [x] Trace enabled build passed.
- [x] `OS_TIMER_ENABLE=0` build passed.
- [x] `OS_DIAGNOSTIC_ENABLE=0` build passed.
- [x] Diagnostic assert probe passed.
- Idle stack watermark during the A-K functional run:
  `used=96`, `unused=32`, `size=128`.
- Note: this G070 board's DAPLink does not control target NRST. The test idle
  hook intentionally does not enter WFI so SWD attach remains reliable.

## ARMv6-M / STM32G070 Stability

- [x] 30-minute long stability test passed.
- Log: `tests/mcu/g070_long_30min.log`
- Result summary:
  - `FAIL count: 0`
  - `ASSERT count: 0`
  - `OK count: 30`
- First OK:
  `[G070-LONG] OK tick=60232 notify=12046 event_isr=8604 event_timer=3542 sem=5475 msgq_tx=406127 msgq_rx=406127 pipe_tx=1520352 pipe_rx=1520320 timer=3542 delay=6022 yield=27420 idle_used=64 idle_unused=64 err=0`
- Last OK:
  `[G070-LONG] OK tick=1811279 notify=362255 event_isr=258754 event_timer=106545 sem=164661 msgq_tx=12314511 msgq_rx=12314511 pipe_tx=45613376 pipe_rx=45613376 timer=106545 delay=181127 yield=695234 idle_used=64 idle_unused=64 err=0`

- [x] 40-minute scheduler stress test passed.
- Log: `tests/mcu/g070_sched_stress_40min.log`
- Result summary:
  - `FAIL count: 0`
  - `ASSERT count: 0`
  - `OK count: 29`
- First OK:
  `[G070-SCHED] OK tick=60311 notify=60310 event_isr=30155 event_timer=20102 sem=20103 msgq_tx=338415 msgq_rx=338415 pipe_tx=1337088 pipe_rx=1337088 timer=20102 delay=6030 yield=40834 idle_used=64 idle_unused=64 err=0`
- Last OK:
  `[G070-SCHED] OK tick=1747679 notify=1747678 event_isr=873839 event_timer=582558 sem=582559 msgq_tx=9722751 msgq_rx=9722751 pipe_tx=38765248 pipe_rx=38765216 timer=582558 delay=174767 yield=1213075 idle_used=64 idle_unused=64 err=0`

- [x] 40-minute bus service stress test passed.
- Log: `tests/mcu/g070_bus_stress_40min.log`
- Result summary:
  - `FAIL count: 0`
  - `ASSERT count: 0`
  - `OK count: 40`
- First OK:
  `[G070-BUS] OK tick=60005 ev0_pub=73142 ev0_rx0=73142 ev0_rx1=73142 ev1_pub=73142 ev1_rx0=73142 ev1_rx1=73142 st0_pub=73142 st0_rx0=73142 st0_rx1=73142 st1_pub=73141 st1_rx0=73141 st1_rx1=73141 ready0=292567 ready1=292567 timeout0=0 timeout1=0 yield=73141 publish_fail=0 idle_used=96 idle_unused=32 err=0`
- Last OK:
  `[G070-BUS] OK tick=2401160 ev0_pub=2925668 ev0_rx0=2925668 ev0_rx1=2925668 ev1_pub=2925668 ev1_rx0=2925668 ev1_rx1=2925668 st0_pub=2925668 st0_rx0=2925668 st0_rx1=2925668 st1_pub=2925668 st1_rx0=2925668 st1_rx1=2925668 ready0=11702672 ready1=11702672 timeout0=0 timeout1=0 yield=2925668 publish_fail=0 idle_used=96 idle_unused=32 err=0`

## First Batch

Start with a minimal firmware that validates the core G070 port:

- [x] UART banner.
- [x] UART heartbeat.
- [x] `scheduler_start()`.
- [x] Two same-priority tasks using `task_yield()`.
- [x] `task_delay()` wakeup through SysTick.
- [x] ISR wakeup using either `task_notify()` or `sem_give()`.

Passing this batch proves the main path:

`SysTick -> os_tick_advance() -> task wakeup -> ready queue -> PendSV -> context switch`
