cmake_minimum_required(VERSION 3.16)

add_library(micaos STATIC)
add_library(MicaOS::micaos ALIAS micaos)

target_sources(micaos PRIVATE
    # Common implementation
    src/common/assert.c

    # Kernel
    src/kernel/task.c
    src/kernel/scheduler.c
    src/kernel/time.c
    src/kernel/eventset.c
    src/kernel/sem.c
    src/kernel/msgq.c
    src/kernel/pipe.c
    src/kernel/timer.c
    src/kernel/trace.c

    # Runtime data structures
    src/data_structure/bytebuf.c
    src/data_structure/packetbuf.c
    src/memory/slab.c

    # Application service
    src/service/bus.c
    src/service/smf.c

    # Architecture ports. micaos_config.h selects the implementation that emits code.
    src/arch/arch_context_armv6m.c
    src/arch/arch_context_armv7m.c
    src/arch/arch_context_armv7m_fpu.c
)

# PUBLIC exposes only the stable <micaos/...> application headers.
target_include_directories(micaos
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/config
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
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
