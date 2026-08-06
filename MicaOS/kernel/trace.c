#include "trace.h"

#if OS_TRACE_ENABLE

#include "common/compiler.h"

__WEAK void os_trace_task_ready(const task_t *task)
{
    /*
     * Example:
     *   debug_log("ready %s", task_get_name(task));
     */
    (void)task;
}

__WEAK void os_trace_task_block(const task_t *task)
{
    /*
     * Example:
     *   debug_log("block %s wait=%u",
     *             task_get_name(task),
     *             (unsigned)task_get_wait_type(task));
     */
    (void)task;
}

__WEAK void os_trace_task_switch(const task_t *from, const task_t *to)
{
    /*
     * Example:
     *   debug_log("switch %s -> %s",
     *             task_get_name(from),
     *             task_get_name(to));
     */
    (void)from;
    (void)to;
}

__WEAK void os_trace_task_exit(const task_t *task)
{
    /*
     * Example:
     *   debug_log("exit %s", task_get_name(task));
     */
    (void)task;
}

#endif /* OS_TRACE_ENABLE */
