# MicaOS STM32F411 MCU Test Plan

This file tracks MCU-side validation on the NUCLEO-F411RE board.

Current board:

- STM32F411RE, Cortex-M4
- ST-LINK V2-1 + STM32CubeProgrammer CLI
- USART2 on PA2/PA3, 115200 baud, COM27
- LD2 on PA5

## Port Variants

- [x] ARMv7M integer-only context switch, soft-float build.
- [x] ARMv7M_FPU context switch, hard-float build.

## Long Stability Test

The F411 long-run firmware is selected with `MCU_TEST_LONG_STABILITY=1`.
It repeatedly stresses:

- SysTick driven ISR wakeup through `task_notify()`, `eventset_set()`, and
  `sem_give()`.
- `task_delay()` and cooperative `task_yield()`.
- `msgq` fixed-size message transfer with sequence checking.
- `pipe` SPSC byte stream transfer with sequence checking.
- periodic soft timer callback and timer-driven task wakeup.
- idle stack watermark reporting.

Build command:

```powershell
Set-Location 'tests/mcu/NUCLEO_F411RE_cmake/Test_STM32F411'
$toolchain = (Resolve-Path 'cmake/gcc-arm-none-eabi.cmake').Path
cmake -S . -B build/LongStability -G Ninja `
  "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
  -DCMAKE_BUILD_TYPE=Debug `
  "-DMICAOS_EXTRA_DEFINES=MCU_TEST_LONG_STABILITY=1"
cmake --build build/LongStability
```

Flash command:

```powershell
STM32_Programmer_CLI `
  -c port=SWD sn=0670FF323535474B43021337 mode=UR freq=4000 `
  -w '.\build\LongStability\Test_STM32F411.elf' `
  -v -rst
```

Expected serial output:

```text
[F411-LONG] boot
[F411-LONG] scheduler start
[F411-LONG] PASS: started
[F411-LONG] OK tick=60009 notify=12001 event_isr=8572 event_timer=3529 sem=5455 msgq_tx=960111 msgq_rx=960111 pipe_tx=1920256 pipe_rx=1920224 timer=3529 delay=6000 yield=1073870 idle_used=64 idle_unused=64 err=0
```

Pass criteria for the first 30-minute run:

- No `[F411-LONG] FAIL:` line.
- No `[F411-LONG] ASSERT:` line.
- One `[F411-LONG] OK ... err=0` line appears every 60 seconds.
- `tick`, `notify`, `event_isr`, `sem`, `msgq_tx`, `msgq_rx`, `pipe_tx`,
  `pipe_rx`, `timer`, `delay`, and `yield` keep increasing.
- `msgq_tx` and `msgq_rx` should stay equal in each printed snapshot.
- `pipe_tx - pipe_rx` may be nonzero because bytes can be buffered, but the
  difference must stay within `LONG_PIPE_SIZE` bytes.
- `idle_used + idle_unused` must equal `SCHED_IDLE_STACK_SIZE`.

Current smoke result:

- [x] Long stability firmware builds without warnings.
- [x] First 60-second report was received on NUCLEO-F411RE.
- [x] First report showed `err=0`.

Final run result:

- [x] 4-hour long stability run passed.
- [x] `FAIL count: 0`.
- [x] `ASSERT count: 0`.
- [x] `OK count: 243`.
- [x] Final `msgq_tx` matched `msgq_rx`:
      `233554223 == 233554223`.
- [x] Final pipe buffered byte count stayed within `LONG_PIPE_SIZE`:
      `467108480 - 467108448 = 32`.
- [x] Final idle stack watermark was stable:
      `idle_used=64`, `idle_unused=64`, `SCHED_IDLE_STACK_SIZE=128`.

Final 4-hour output summary:

```text
first OK: [F411-LONG] OK tick=60009 notify=12001 event_isr=8572 event_timer=3529 sem=5455 msgq_tx=960111 msgq_rx=960111 pipe_tx=1920256 pipe_rx=1920224 timer=3529 delay=6000 yield=1073870 idle_used=64 idle_unused=64 err=0
last OK: [F411-LONG] OK tick=14597141 notify=2919428 event_isr=2085305 event_timer=858655 sem=1327012 msgq_tx=233554223 msgq_rx=233554223 pipe_tx=467108480 pipe_rx=467108448 timer=858655 delay=1459713 yield=260948403 idle_used=64 idle_unused=64 err=0
```

## Scheduler Stress Test

The F411 high-frequency scheduler stress firmware is selected with
`MCU_TEST_SCHED_STRESS=1`. It reuses the long-run self-checking data paths, but
increases scheduler pressure:

- `task_notify()` wakeup from SysTick every 1 tick.
- `eventset_set()` wakeup from SysTick every 2 ticks.
- `sem_give()` wakeup from SysTick every 3 ticks.
- periodic soft timer every 3 ticks.
- continuous `task_yield()` and `task_delay(1)` pressure.
- `msgq` and `pipe` sequence checks continue to run in parallel.

Build command:

```powershell
Set-Location 'tests/mcu/NUCLEO_F411RE_cmake/Test_STM32F411'
$toolchain = (Resolve-Path 'cmake/gcc-arm-none-eabi.cmake').Path
cmake -S . -B build/SchedStress -G Ninja `
  "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
  -DCMAKE_BUILD_TYPE=Debug `
  "-DMICAOS_EXTRA_DEFINES=MCU_TEST_SCHED_STRESS=1"
cmake --build build/SchedStress
```

Flash command:

```powershell
STM32_Programmer_CLI `
  -c port=SWD sn=0670FF323535474B43021337 mode=UR freq=4000 `
  -w '.\build\SchedStress\Test_STM32F411.elf' `
  -v -rst
```

Expected serial output:

```text
[F411-SCHED] boot
[F411-SCHED] scheduler start
[F411-SCHED] PASS: started
[F411-SCHED] OK tick=60010 notify=60009 event_isr=30005 event_timer=20002 sem=20003 msgq_tx=960127 msgq_rx=960127 pipe_tx=1920256 pipe_rx=1920256 timer=20002 delay=6000 yield=805642 idle_used=64 idle_unused=64 err=0
```

Pass criteria for the first 3-hour run:

- No `[F411-SCHED] FAIL:` line.
- No `[F411-SCHED] ASSERT:` line.
- One `[F411-SCHED] OK ... err=0` line appears every 60 seconds.
- `notify`, `event_isr`, `event_timer`, `sem`, `timer`, `delay`, and `yield`
  keep increasing.
- `msgq_tx` and `msgq_rx` should stay equal in each printed snapshot.
- `pipe_tx - pipe_rx` may be nonzero because bytes can be buffered, but the
  difference must stay within `LONG_PIPE_SIZE` bytes.
- `idle_used + idle_unused` must equal `SCHED_IDLE_STACK_SIZE`.

Current smoke result:

- [x] Scheduler stress firmware builds without warnings.
- [x] First 60-second report was received on NUCLEO-F411RE.
- [x] First report showed `err=0`.

Final run result:

- [x] 2-hour scheduler stress run passed.
- [x] `FAIL count: 0`.
- [x] `ASSERT count: 0`.
- [x] `OK count: 121`.
- [x] Final `msgq_tx` matched `msgq_rx`:
      `116314447 == 116314447`.
- [x] Final `pipe_tx` matched `pipe_rx`:
      `232628896 == 232628896`.
- [x] Final idle stack watermark was stable:
      `idle_used=64`, `idle_unused=64`, `SCHED_IDLE_STACK_SIZE=128`.

Final 2-hour output summary:

```text
first OK: [F411-SCHED] OK tick=60010 notify=60009 event_isr=30005 event_timer=20002 sem=20003 msgq_tx=960127 msgq_rx=960127 pipe_tx=1920256 pipe_rx=1920256 timer=20002 delay=6000 yield=805642 idle_used=64 idle_unused=64 err=0
last OK: [F411-SCHED] OK tick=7269655 notify=7269654 event_isr=3634827 event_timer=2423217 sem=2423218 msgq_tx=116314447 msgq_rx=116314447 pipe_tx=232628896 pipe_rx=232628896 timer=2423217 delay=726965 yield=97473839 idle_used=64 idle_unused=64 err=0
```

## FPU-Specific Test Result

The current ARMv7M_FPU result is a hard-float register-preservation test plus
the normal kernel functional suite. The test firmware explicitly checks low FP
registers, high FP registers, FPSCR, FP task switching, FP wakeups, and mixed
integer/FP scheduling.

Validated items:

- [x] Hard-float build uses `ARMv7M_FPU/arch_context.c` and compiler flags
      `-mfpu=fpv4-sp-d16 -mfloat-abi=hard`.
- [x] Startup enables CP10/CP11 access before any task executes FP instructions.
- [x] A task can execute simple floating-point arithmetic without UsageFault.
- [x] Two FP-using tasks repeatedly yield and preserve independent FP local
      results across context switches.
- [x] FP state is preserved across `task_delay()` wakeup.
- [x] FP state is preserved when a higher-priority task preempts a lower-priority
      FP task.
- [x] FP state is preserved when a task is woken from ISR through
      `task_notify()`, `eventset_set()`, or `sem_give()`.
- [x] A task using only integer code can switch with FP-using tasks without
      corrupting either side.
- [x] Callee-saved high FP registers `S16-S31` are preserved across context
      switches. This is the critical software-saved FPU path in the port.
- [x] Hardware-stacked low FP registers `S0-S15` and `FPSCR` are preserved across
      exception entry/return.
- [x] Lazy FP stacking path is exercised: one task never touches FP, another task
      touches FP, and both switch repeatedly.
- [x] FPU test passes with trace disabled and diagnostics enabled.
- [x] FPU test passes with `OS_DIAGNOSTIC_ENABLE=0`.
- [x] Optional stress test: many FP context switches over several seconds without
      mismatch or fault.

Observed FPU test output:

- Diagnostics enabled hard-float build: `[F411-FPU] regs ok` and
  `[F411-PRIM] PASS`.
- `OS_DIAGNOSTIC_ENABLE=0` hard-float build: `[F411-FPU] regs ok` and
  `[F411-PRIM] PASS`.
- FPU test build idle stack watermark with 128-byte idle stack:
  used 92 bytes, unused 36 bytes.

## FPU Stress Test

The FPU stress firmware is selected with `MCU_TEST_FPU_STRESS=1`. It combines
the high-frequency scheduler stress workload with two hard-float tasks that
continuously write and verify `S16-S31`.

The long-run FPU stress focuses on `S16-S31` because these registers are
callee-saved by the hard-float ABI and software-saved by the Cortex-M FPU
context switch port. `S0-S15` and `FPSCR` are covered by the focused FPU
functional test above; they are not treated as persistent across arbitrary C
function calls because `S0-S15` are caller-saved ABI registers.

Build command:

```powershell
Set-Location 'tests/mcu/NUCLEO_F411RE_cmake/Test_STM32F411'
$toolchain = (Resolve-Path 'cmake/gcc-arm-none-eabi.cmake').Path
$arch = (Resolve-Path '../../../../MicaOS/arch/CortexM/ARMv7M_FPU/arch_context.c').Path
cmake -S . -B build/FpuStress -G Ninja `
  "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
  -DCMAKE_BUILD_TYPE=Debug `
  "-DMCU_TARGET_FLAGS=-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard " `
  "-DMICAOS_ARCH_CONTEXT_SOURCE=$arch" `
  "-DMICAOS_EXTRA_DEFINES=MCU_TEST_FPU_STRESS=1"
cmake --build build/FpuStress
```

Flash command:

```powershell
STM32_Programmer_CLI `
  -c port=SWD sn=0670FF323535474B43021337 mode=UR freq=4000 `
  -w '.\build\FpuStress\Test_STM32F411.elf' `
  -v -rst
```

Expected serial output:

```text
[F411-FPU-STRESS] boot
[F411-FPU-STRESS] scheduler start
[F411-FPU-STRESS] PASS: started
[F411-FPU-STRESS] OK tick=60021 notify=60019 event_isr=30010 event_timer=20005 sem=20007 msgq_tx=896287 msgq_rx=896287 pipe_tx=1920576 pipe_rx=1920576 timer=20005 delay=6001 yield=245805 fpu0=60017 fpu1=60017 idle_used=68 idle_unused=60 err=0
```

Pass criteria for the first 2-hour run:

- No `[F411-FPU-STRESS] FAIL:` line.
- No `[F411-FPU-STRESS] ASSERT:` line.
- One `[F411-FPU-STRESS] OK ... err=0` line appears every 60 seconds.
- `fpu0` and `fpu1` keep increasing.
- `msgq_tx` and `msgq_rx` should stay equal in each printed snapshot.
- `pipe_tx - pipe_rx` may be nonzero because bytes can be buffered, but the
  difference must stay within `LONG_PIPE_SIZE` bytes.
- `idle_used + idle_unused` must equal `SCHED_IDLE_STACK_SIZE`.

Current smoke result:

- [x] FPU stress firmware builds without warnings.
- [x] First 60-second report was received on NUCLEO-F411RE.
- [x] First report showed `err=0`.

Final run result:

- [x] 2-hour FPU stress run passed.
- [x] `FAIL count: 0`.
- [x] `ASSERT count: 0`.
- [x] `OK count: 121`.
- [x] Final `msgq_tx` matched `msgq_rx`:
      `108841871 == 108841871`.
- [x] Final `pipe_tx` matched `pipe_rx`:
      `233172096 == 233172096`.
- [x] Final FPU stress counters were still increasing:
      `fpu0=7286627`, `fpu1=7286627`.
- [x] Final idle stack watermark was stable:
      `idle_used=68`, `idle_unused=60`, `SCHED_IDLE_STACK_SIZE=128`.

Final 2-hour output summary:

```text
first OK: [F411-FPU-STRESS] OK tick=60021 notify=60019 event_isr=30010 event_timer=20005 sem=20007 msgq_tx=896287 msgq_rx=896287 pipe_tx=1920576 pipe_rx=1920576 timer=20005 delay=6001 yield=245805 fpu0=60017 fpu1=60017 idle_used=68 idle_unused=60 err=0
last OK: [F411-FPU-STRESS] OK tick=7286631 notify=7286629 event_isr=3643315 event_timer=2428875 sem=2428877 msgq_tx=108841871 msgq_rx=108841871 pipe_tx=233172096 pipe_rx=233172096 timer=2428875 delay=728662 yield=29741694 fpu0=7286627 fpu1=7286627 idle_used=68 idle_unused=60 err=0
```

## Bus Service Stress Test

The bus stress firmware is selected with `MCU_TEST_BUS_STRESS=1`. It exercises
two EVENT channels, two STATE channels, and two subscribers. Each subscriber
waits on its own channel-ready queue and drains or reads the reported channel.

1-hour result on NUCLEO-F411RE:

- [x] Bus stress firmware builds and flashes.
- [x] EVENT publish/read counters keep increasing for both subscribers.
- [x] STATE publish/read counters keep increasing for both subscribers.
- [x] `publish_fail=0`, `timeout0=0`, `timeout1=0`, and `err=0`.
- [x] No `FAIL` or `ASSERT` line was reported.

```
FAIL count: 0
ASSERT count: 0
OK count: 61
first OK: [F411-BUS] OK tick=60005 ev0_pub=135931 ev0_rx0=135931 ev0_rx1=135931 ev1_pub=135931 ev1_rx0=135931 ev1_rx1=135931 st0_pub=135931 st0_rx0=135931 st0_rx1=135931 st1_pub=135931 st1_rx0=135931 st1_rx1=135931 ready0=543724 ready1=543724 timeout0=0 timeout1=0 yield=135930 publish_fail=0 idle_used=88 idle_unused=40 err=0
last OK: [F411-BUS] OK tick=3661798 ev0_pub=8291587 ev0_rx0=8291587 ev0_rx1=8291587 ev1_pub=8291586 ev1_rx0=8291586 ev1_rx1=8291586 st0_pub=8291586 st0_rx0=8291586 st0_rx1=8291586 st1_pub=8291586 st1_rx0=8291586 st1_rx1=8291586 ready0=33166345 ready1=33166345 timeout0=0 timeout1=0 yield=8291586 publish_fail=0 idle_used=88 idle_unused=40 err=0
```

## First Batch: ARMv7M Non-FPU

- [x] UART banner.
- [x] UART heartbeat.
- [x] `scheduler_start()`.
- [x] Two same-priority tasks using `task_yield()`.
- [x] `task_delay()` wakeup through SysTick.
- [x] ISR wakeup using `task_notify()`.

Passing this batch proves the F411 non-FPU path:

`SysTick -> os_tick_advance() -> task wakeup -> ready queue -> PendSV -> ARMv7M context switch`

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

## Idle Stack Measurement

Test condition:

- Board: NUCLEO-F411RE
- Build: Debug
- `TASK_STACK_WATERMARK_ENABLE=1`
- `scheduler_idle_hook()` only sets test state and executes `__WFI()`

Results:

ARMv7M integer-only soft-float build:

- `SCHED_IDLE_STACK_SIZE=128`: used 88 bytes, unused 40 bytes.
- `SCHED_IDLE_STACK_SIZE=256`: used 88 bytes, unused 168 bytes.
- `SCHED_IDLE_STACK_SIZE=512`: used 88 bytes, unused 424 bytes.

ARMv7M_FPU hard-float build:

- `SCHED_IDLE_STACK_SIZE=128`: used 92 bytes, unused 36 bytes.

Conclusion:

- For the default idle hook / WFI-only use case, 128 bytes is sufficient on
  these F411 test builds.
- If user code overrides `scheduler_idle_hook()` to feed a watchdog, print logs,
  enter vendor HAL low-power calls, or do any nontrivial work, the project must
  increase `SCHED_IDLE_STACK_SIZE` and re-check the watermark.
