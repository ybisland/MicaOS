#include "tests/test_all.h"
#include "tests/suites/test_suites.h"

void test_all_run(void)
{
    test_minmax_run();
    test_dlist_run();
    test_slist_run();
    test_bitmap_run();
    test_bytebuf_run();
    test_packetbuf_run();
    test_slab_run();
    test_time_timer_run();
    test_coro_run();
    test_task_event_sem_run();
    test_msgq_pipe_run();
    test_message_bus_run();
    test_crc_run();
}
