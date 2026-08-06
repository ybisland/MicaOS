#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H

#include "kernel_config.h"
#include "common/assert.h"

/*
 * OS_DIAG_ASSERT is for kernel-internal consistency checks.
 *
 * Public API contract checks should use ASSERT directly so user
 * misuse is still caught in debug builds. Internal checks may be disabled after
 * the kernel has been tested to keep debug firmware size smaller.
 */
#if OS_DIAGNOSTIC_ENABLE
#define OS_DIAG_ASSERT(cond) ASSERT(cond)
#else
#define OS_DIAG_ASSERT(cond) ((void)sizeof(cond))
#endif

#endif /* KERNEL_DEBUG_H */
