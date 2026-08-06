#include "timer.h"

#if OS_TIMER_ENABLE

#include "kernel_debug.h"
#include "scheduler_internal.h"
#include "arch/arch_context.h"

static dlist_t timer_list_ = dlist_static_init(timer_list_);

static void timer_insert_locked_(soft_timer_t *timer)
{
    dlist_node_t *node;

    OS_DIAG_ASSERT((timer != NULL) &&
                   dlist_node_is_detached(&timer->node));

    dlist_for_each(node, &timer_list_) {
        soft_timer_t *entry = dlist_entry(node, soft_timer_t, node);

        if (!os_tick_after_eq(timer->expiry, entry->expiry)) {
            dlist_insert_before(node, &timer->node);
            return;
        }
    }

    dlist_push_back(&timer_list_, &timer->node);
}

void kernel_timer_init(void)
{
    dlist_init(&timer_list_);
}

void kernel_timer_advance(os_tick_t now)
{
    for (;;) {
        uint32_t key;
        dlist_node_t *node;
        soft_timer_t *timer;
        timer_callback_t callback;
        void *arg;

        key = arch_irq_lock();

        node = dlist_peek_front(&timer_list_);
        if (node == NULL) {
            arch_irq_unlock(key);
            return;
        }

        timer = dlist_entry(node, soft_timer_t, node);
        if (!os_tick_after_eq(now, timer->expiry)) {
            arch_irq_unlock(key);
            return;
        }

        dlist_remove(&timer->node);

        callback = timer->callback;
        arg = timer->arg;

        if (timer->period != 0U) {
            timer->expiry += timer->period;
            timer_insert_locked_(timer);
        } else {
            timer->running = false;
        }

        arch_irq_unlock(key);

        callback(arg);
    }
}

void timer_init(soft_timer_t *timer,
                timer_callback_t callback,
                void *arg)
{
    ASSERT((timer != NULL) && (callback != NULL));

    dlist_init(&timer->node);
    timer->expiry = 0U;
    timer->period = 0U;
    timer->callback = callback;
    timer->arg = arg;
    timer->running = false;
}

void timer_start(soft_timer_t *timer,
                 os_tick_t delay,
                 os_tick_t period)
{
    uint32_t key;

    ASSERT((timer != NULL) &&
           (timer->callback != NULL) &&
           (delay != 0U) &&
           (delay <= OS_TICK_MAX_DELAY) &&
           (period <= OS_TICK_MAX_DELAY));

    key = arch_irq_lock();

    if (timer->running) {
        OS_DIAG_ASSERT(!dlist_node_is_detached(&timer->node));
        dlist_remove(&timer->node);
    } else {
        OS_DIAG_ASSERT(dlist_node_is_detached(&timer->node));
    }

    timer->expiry = os_tick_get_locked() + delay;
    timer->period = period;
    timer->running = true;
    timer_insert_locked_(timer);

    arch_irq_unlock(key);
}

void timer_stop(soft_timer_t *timer)
{
    uint32_t key;

    ASSERT(timer != NULL);

    key = arch_irq_lock();

    if (timer->running) {
        OS_DIAG_ASSERT(!dlist_node_is_detached(&timer->node));
        dlist_remove(&timer->node);
        timer->running = false;
    } else {
        OS_DIAG_ASSERT(dlist_node_is_detached(&timer->node));
    }

    arch_irq_unlock(key);
}

bool timer_is_running(const soft_timer_t *timer)
{
    uint32_t key;
    bool running;

    ASSERT(timer != NULL);

    key = arch_irq_lock();
    running = timer->running;
    arch_irq_unlock(key);

    return running;
}

#endif /* OS_TIMER_ENABLE */
