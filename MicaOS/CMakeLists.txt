cmake_minimum_required(VERSION 3.16)

add_library(micaos STATIC)
add_library(MicaOS::micaos ALIAS micaos)

target_sources(micaos PRIVATE
    # Common implementation
    common/assert.c

    # Kernel
    kernel/task.c
    kernel/scheduler.c
    kernel/time.c
    kernel/eventset.c
    kernel/sem.c
    kernel/msgq.c
    kernel/pipe.c
    kernel/timer.c
    kernel/trace.c

    # Runtime data structures
    data_structure/bytebuf.c
    data_structure/packetbuf.c
    memory/slab.c

    # Application service
    service/bus/bus.c

    # Architecture ports. config.h selects the implementation that emits code.
    arch/CortexM/ARMv6M/arch_context.c
    arch/CortexM/ARMv7M/arch_context.c
    arch/CortexM/ARMv7M_FPU/arch_context.c
)

# Public headers use paths such as "kernel/kernel.h" and "service/bus/bus.h".
# PUBLIC makes this include directory available to applications that link
# MicaOS::micaos.
target_include_directories(micaos
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

# MicaOS requires GNU-compatible C11. Propagate the requirement to consumers
# because public headers also form part of the MicaOS API.
target_compile_features(micaos
    PUBLIC
        c_std_11
)

set_target_properties(micaos PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)
