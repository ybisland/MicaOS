cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Test_STM32G070)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

set(MICAOS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../)

if(NOT DEFINED MICAOS_ARCH_CONTEXT_SOURCE)
    set(MICAOS_ARCH_CONTEXT_SOURCE ${MICAOS_ROOT}/src/arch/arch_context_armv6m.c)
endif()

if(NOT DEFINED MICAOS_ARCH_PORT)
    if(MICAOS_ARCH_CONTEXT_SOURCE MATCHES "arch_context_armv6m\\.c$")
        set(MICAOS_ARCH_PORT MICAOS_ARCH_PORT_ARMV6M)
    elseif(MICAOS_ARCH_CONTEXT_SOURCE MATCHES "arch_context_armv7m_fpu\\.c$")
        set(MICAOS_ARCH_PORT MICAOS_ARCH_PORT_ARMV7M_FPU)
    else()
        set(MICAOS_ARCH_PORT MICAOS_ARCH_PORT_ARMV7M)
    endif()
endif()

if(NOT DEFINED OS_DIAGNOSTIC_ENABLE)
    set(OS_DIAGNOSTIC_ENABLE 1)
endif()

if(NOT DEFINED MICAOS_TRACE_ENABLE)
    set(MICAOS_TRACE_ENABLE 0)
endif()

if(NOT DEFINED MICAOS_TIMER_ENABLE)
    set(MICAOS_TIMER_ENABLE 1)
endif()

if(NOT DEFINED MICAOS_STACK_WATERMARK_ENABLE)
    set(MICAOS_STACK_WATERMARK_ENABLE 1)
endif()

if(NOT DEFINED MICAOS_IDLE_STACK_SIZE)
    set(MICAOS_IDLE_STACK_SIZE 128)
endif()

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${MICAOS_ROOT}/src/common/assert.c
    ${MICAOS_ROOT}/src/data_structure/bytebuf.c
    ${MICAOS_ROOT}/src/service/bus.c
    ${MICAOS_ROOT}/src/kernel/task.c
    ${MICAOS_ROOT}/src/kernel/scheduler.c
    ${MICAOS_ROOT}/src/kernel/time.c
    ${MICAOS_ROOT}/src/kernel/eventset.c
    ${MICAOS_ROOT}/src/kernel/sem.c
    ${MICAOS_ROOT}/src/kernel/msgq.c
    ${MICAOS_ROOT}/src/kernel/pipe.c
    ${MICAOS_ROOT}/src/kernel/trace.c
    ${MICAOS_ARCH_CONTEXT_SOURCE}
)

if(MICAOS_TIMER_ENABLE)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${MICAOS_ROOT}/src/kernel/timer.c
    )
endif()

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${MICAOS_ROOT}/include
    ${MICAOS_ROOT}/config
    ${MICAOS_ROOT}/src
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    MICAOS_ARCH_PORT=${MICAOS_ARCH_PORT}
    OS_DIAGNOSTIC_ENABLE=${OS_DIAGNOSTIC_ENABLE}
    OS_TRACE_ENABLE=${MICAOS_TRACE_ENABLE}
    OS_TIMER_ENABLE=${MICAOS_TIMER_ENABLE}
    TASK_STACK_WATERMARK_ENABLE=${MICAOS_STACK_WATERMARK_ENABLE}
    SCHED_IDLE_STACK_SIZE=${MICAOS_IDLE_STACK_SIZE}
)

if(DEFINED MICAOS_EXTRA_DEFINES)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        ${MICAOS_EXTRA_DEFINES}
    )
endif()

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
