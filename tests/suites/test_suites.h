#ifndef TEST_SUITES_H
#define TEST_SUITES_H

#ifdef __cplusplus
extern "C" {
#endif

void test_dlist_run(void);
void test_slist_run(void);
void test_bitmap_run(void);
void test_bytebuf_run(void);
void test_packetbuf_run(void);
void test_slab_run(void);
void test_minmax_run(void);
void test_coro_run(void);
void test_time_timer_run(void);
void test_task_event_sem_run(void);
void test_msgq_pipe_run(void);
void test_message_bus_run(void);
void test_crc_run(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_SUITES_H */
