# Configuration and Build

MicaOS uses one configuration file for all build systems:

```text
MicaOS/config.h
```

Users are expected to edit this file directly.

## Common Options

| Option | Meaning |
| --- | --- |
| `MICAOS_ARCH_PORT` | Selects the architecture |
| `SCHED_PRIORITY_LEVELS` | Number of task priority levels, valid range `1..32` |
| `SCHED_IDLE_STACK_SIZE` | Internal idle task stack size in bytes, 8-byte aligned |
| `OS_TIMER_ENABLE` | Enables soft timer support |
| `ASSERT_DEBUG` | Enables assertion handling in debug builds |
| `OS_DIAGNOSTIC_ENABLE` | Enables deeper diagnostic assertions |
| `TASK_STACK_WATERMARK_ENABLE` | Enables task stack watermark estimation |
| `OS_TRACE_ENABLE` | Enables generic trace hooks |
| `SLAB_ALLOC_FAILED_HOOK_ENABLE` | Calls the slab allocation failure hook on allocation failure |

## Architecture Selection

```c
#define MICAOS_ARCH_PORT MICAOS_ARCH_PORT_ARMV7M
```

Available values:

| Target | Config value |
| --- | --- |
| Cortex-M0 / M0+ | `MICAOS_ARCH_PORT_ARMV6M` |
| Cortex-M3 / M4 / M7 without hard-float ABI | `MICAOS_ARCH_PORT_ARMV7M` |
| Cortex-M4F / M7 with hard-float ABI | `MICAOS_ARCH_PORT_ARMV7M_FPU` |

The FPU selection must match the compiler ABI. A chip with FPU hardware should
still use `MICAOS_ARCH_PORT_ARMV7M` if the project is built with soft-float ABI.

## CMake Build

MicaOS is exposed as one static CMake target. From a user project:

```cmake
add_subdirectory(path/to/MicaOS)
target_link_libraries(app PRIVATE MicaOS::micaos)
```

Once the application target has been created, link it to MicaOS:

```cmake
add_subdirectory(MicaOS)

add_executable(app
    main.c
)

target_link_libraries(app
    PRIVATE
        MicaOS::micaos
)
```

The MicaOS target:

- builds a static library named `micaos`;
- provides alias target `MicaOS::micaos`;
- adds `MicaOS/` as a `PUBLIC` include path;
- adds all current MicaOS `.c` source files to the static library; and
- requires GNU-compatible C11.

The public include path means application code can use the normal MicaOS
include paths without adding another include-directory command:

```c
#include "kernel/kernel.h"
#include "service/bus/bus.h"
#include "data_structure/bytebuf.h"
```


## Manual Build

For Keil, IAR, STM32CubeIDE, Makefile, or other systems:

```text
1. Add MicaOS/ to the include path.
2. Add the MicaOS .c files used by your project.
3. Add either all arch_context.c files, or only the one selected by
   MICAOS_ARCH_PORT.
```

Common source set:

```text
MicaOS/common/assert.c
MicaOS/kernel/task.c
MicaOS/kernel/scheduler.c
MicaOS/kernel/time.c
MicaOS/kernel/eventset.c
MicaOS/kernel/sem.c
MicaOS/kernel/msgq.c
MicaOS/kernel/pipe.c
MicaOS/kernel/timer.c
MicaOS/kernel/trace.c
MicaOS/data_structure/bytebuf.c
MicaOS/data_structure/packetbuf.c
MicaOS/memory/slab.c
MicaOS/service/bus/bus.c
MicaOS/arch/CortexM/ARMv6M/arch_context.c
MicaOS/arch/CortexM/ARMv7M/arch_context.c
MicaOS/arch/CortexM/ARMv7M_FPU/arch_context.c
```

`timer.c` is controlled by `OS_TIMER_ENABLE`, so it is safe to compile even
when soft timer is disabled. Each architecture file checks `MICAOS_ARCH_PORT`,
so it is also safe to compile all current Cortex-M ports together.

Header-only modules such as `dlist`, `slist`, and `bitmap` do not need source
files.

## Compiler Requirement

Current requirement:

```text
GNU C11 or compatible compiler mode
```

GCC and Clang usually use:

```text
-std=gnu11
```

Cortex-M ports additionally require compiler support for inline assembly and
common attributes used by embedded compilers.
