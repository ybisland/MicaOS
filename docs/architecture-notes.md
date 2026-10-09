# Architecture Notes

This document is for maintainers. It explains design decisions that users do
not need for normal API usage.

## Design Goals

MicaOS prioritizes:

- static allocation
- predictable behavior
- small code size
- low runtime overhead
- simple debugging
- explicit user contracts

It intentionally does not try to match feature-rich RTOS designs.

## Scheduler Model

The scheduler is static-priority based.

- lower numeric priority is higher priority
- higher-priority READY tasks preempt lower-priority tasks
- equal-priority tasks do not preempt each other
- equal-priority tasks run FIFO at cooperative scheduling points
- no time slicing

This model avoids timer-driven same-priority round-robin overhead and keeps
task ordering explicit.

## No Mutex / Priority Inheritance

MicaOS currently does not provide mutex or priority inheritance.

The intended style is:

- keep shared-state ownership explicit
- use message passing or direct synchronization
- avoid long shared critical sections
- avoid hidden blocking inside shared resources

Adding mutex and priority inheritance would change scheduler complexity and
should be treated as a major design change.

## Wait Model

Delay, notification, eventset, semaphore, msgq, pipe, and timer wakeups share
one internal task wait model.

A task can wait on only one object at a time.

Timeouts are handled through an ordered sleep list using absolute wake ticks.
Wraparound-safe comparisons require real time differences to stay within half
of the `uint32_t` tick range.

## Ready Queues

The scheduler uses:

- one ready list per priority
- bitmap to find non-empty priorities

This keeps highest-priority lookup bounded and simple.

## Strong-Contract Style

MicaOS is not defensive in release builds.

Public API misuse is diagnosed in debug builds with `OS_ASSERT`.
Deeper optional checks use `OS_DIAG_ASSERT`.

The goal is to catch incorrect usage during development without permanently
adding heavy checks to hot paths.

## Bus Position

The bus is a service layer, not a kernel primitive.

It is synchronous and task-context only in v0.

ISR publish and async broker designs are intentionally deferred because they
increase fanout and scheduling complexity.

## Architecture Ports

Cortex-M ports implement the same private `src/internal/arch/arch_context.h`
interface.

The current build model allows all arch port `.c` files to be compiled. The
selected implementation is controlled by `MICAOS_ARCH_PORT`.

This keeps manual build systems simple because users do not need to know which
single source file to include.
