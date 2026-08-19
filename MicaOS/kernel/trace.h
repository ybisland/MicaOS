#ifndef OS_TRACE_H
#define OS_TRACE_H

#include "config.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generic OS trace hooks
 *
 * This module provides tool-independent observation points for scheduler
 * behavior. It does not implement a trace backend by itself. Projects may
 * override these weak hooks or connect them to a backend such as SystemView,
 * Tracealyzer, or Event Recorder.
 *
 * Trace hooks run from kernel paths. They must be short, non-blocking, and must
 * not modify scheduler/task state.
 */

#if OS_TRACE_ENABLE
void os_trace_task_ready(const task_t *task);
void os_trace_task_block(const task_t *task);
void os_trace_task_switch(const task_t *from, const task_t *to);
void os_trace_task_exit(const task_t *task);
#endif

#ifdef __cplusplus
}
#endif

#endif /* OS_TRACE_H */
