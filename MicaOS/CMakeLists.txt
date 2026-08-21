cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(MicaOS C)
endif()

add_library(micaos STATIC)
add_library(MicaOS::micaos ALIAS micaos)

target_sources(micaos PRIVATE
    common/assert.c

    data_structure/bytebuf.c
    data_structure/packetbuf.c

    kernel/eventset.c
    kernel/msgq.c
    kernel/pipe.c
    kernel/scheduler.c
    kernel/sem.c
    kernel/task.c
    kernel/time.c
    kernel/timer.c
    kernel/trace.c

    memory/slab.c

    service/bus/bus.c

    arch/CortexM/ARMv6M/arch_context.c
    arch/CortexM/ARMv7M/arch_context.c
    arch/CortexM/ARMv7M_FPU/arch_context.c
)

target_include_directories(micaos PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(micaos PUBLIC c_std_99)
set_target_properties(micaos PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)
