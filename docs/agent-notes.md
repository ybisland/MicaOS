# Agent Notes

This document is a compact knowledge base for future AI agents or maintainers.

## Read First

For MicaOS work, read:

1. `docs/index.md`
2. `docs/configuration-and-build.md`
3. the header of the module being changed
4. `docs/architecture-notes.md` if changing kernel behavior

## Project Layout

```text
MicaOS/
  arch/
  common/
  data_structure/
  kernel/
  memory/
  service/bus/
docs/
tests/
tools/
```

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

## Configuration

All build systems use:

```text
MicaOS/config.h
```

Do not reintroduce a separate generated config system unless requested.

## Assertion Policy

- Application-level assertion macro: `ASSERT`
- MicaOS public API misuse: `OS_ASSERT`
- deeper optional diagnostics: `OS_DIAG_ASSERT`

`OS_DIAG_ASSERT` is controlled by `OS_DIAGNOSTIC_ENABLE` and only matters when
`ASSERT_DEBUG` is enabled.

## Kernel Invariants

- no dynamic memory allocation
- task stacks are caller-owned
- task objects are caller-owned
- idle task and idle stack are kernel-owned
- ISR may wake tasks but must not block
- finite timeouts must be `<= OS_TICK_MAX_DELAY`
- equal-priority tasks do not time-slice
- no mutex or priority inheritance unless the design is explicitly reopened

## Architecture Selection

Use `MICAOS_ARCH_PORT`:

- `MICAOS_ARCH_PORT_ARMV6M`
- `MICAOS_ARCH_PORT_ARMV7M`
- `MICAOS_ARCH_PORT_ARMV7M_FPU`

All Cortex-M arch source files may be compiled together. Only the selected port
emits implementation.

## Bus Rules

Bus is an application service.

- static topology
- no dynamic allocation
- task-context runtime API only
- `bus_publish()` fail-fast on EVENT ring full via hook
- `bus_try_publish()` returns false for droppable EVENT
- no ISR publish in v0
- no async broker in v0

Offline bus config checker:

```powershell
python .\tools\check_bus_config.py .\app\app_bus.c
```

## Documentation Policy

User guides should explain usage, constraints, and examples.

Implementation details belong in:

- `docs/architecture-notes.md`
- clearly marked implementation sections in module headers

Do not mix deep implementation notes into first-use guides.
