# Configuration and Build

MicaOS uses one configuration file for all build systems:

```text
config/micaos_config.h
```

Users are expected to edit this file directly.

When the repository is copied to `third_part/micaos`, edit:

```text
third_part/micaos/config/micaos_config.h
```

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
add_subdirectory(path/to/MicaOS)

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
- adds `include` as the `PUBLIC` include path;
- adds `config` as a `PUBLIC` configuration include path;
- keeps `src` private to the MicaOS target;
- adds all current MicaOS `.c` source files to the static library; and
- requires GNU-compatible C11.

The public include path means application code can use the normal MicaOS
include paths without adding another include-directory command:

```c
#include <micaos/kernel.h>
#include <micaos/service/bus.h>
#include <micaos/data_structure/bytebuf.h>
```


## Manual Build

For Keil, IAR, STM32CubeIDE, Makefile, or other systems:

```text
1. Add include/ to the application include path.
2. Add config/ to the application include path.
3. Add src/ to the include path while compiling MicaOS sources.
4. Add the MicaOS .c files used by your project.
5. Add either all architecture context files, or only the one selected by
   MICAOS_ARCH_PORT.
```

Common source set:

```text
src/common/assert.c
src/kernel/task.c
src/kernel/scheduler.c
src/kernel/time.c
src/kernel/eventset.c
src/kernel/sem.c
src/kernel/msgq.c
src/kernel/pipe.c
src/kernel/timer.c
src/kernel/trace.c
src/data_structure/bytebuf.c
src/data_structure/packetbuf.c
src/memory/slab.c
src/service/bus.c
src/arch/arch_context_armv6m.c
src/arch/arch_context_armv7m.c
src/arch/arch_context_armv7m_fpu.c
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
