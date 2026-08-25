# IPC Guide

This guide explains MicaOS synchronization and communication primitives.

## Timeout Model

All blocking APIs use the same timeout model:

| Timeout | Meaning |
| --- | --- |
| `OS_NO_WAIT` | Check once and never block |
| finite tick value | Wait up to that many ticks |
| `OS_WAIT_FOREVER` | Wait without a deadline |

Finite timeout values must be no larger than `OS_TICK_MAX_DELAY`.

## ISR Rules

In ISR context, only non-blocking APIs are allowed(APIs with timeout must use `OS_NO_WAIT`).

Allowed from ISR:

- `task_notify()`
- `eventset_set()`
- `sem_give()`
- `msgq_send(..., OS_NO_WAIT)`
- `msgq_recv(..., OS_NO_WAIT)`
- `pipe_write(..., OS_NO_WAIT)`
- `pipe_read(..., OS_NO_WAIT)`

Not allowed from ISR:

- `task_delay()`
- `task_yield()`
- `task_notify_wait()`
- `eventset_wait_any()` / `eventset_wait_all()`
- blocking `sem_take()`
- blocking `msgq_send()` / `msgq_recv()`
- blocking `pipe_write()` / `pipe_read()`

## Choosing a Primitive

| Need | Use |
| --- | --- |
| Wake one known task | `task_notify` |
| Wait for named bits | `eventset` |
| Count resources or events | `sem` |
| Transfer fixed-size messages | `msgq` |
| Transfer SPSC byte stream | `pipe` |
| Publish events/states to multiple modules | `bus` |

## Task Notification

```c
void task_notify(task_t *task);
bool task_notify_wait(os_tick_t timeout);
```

Use notification when one producer needs to wake one known task.

Notification stores one pending flag. It does not count how many times notify
was called.

## Eventset

An eventset stores 32 bits of state.

```c
void eventset_init(eventset_t *eventset);
void eventset_set(eventset_t *eventset, eventset_bits_t bits);
void eventset_clear(eventset_t *eventset, eventset_bits_t bits);
eventset_bits_t eventset_get(eventset_t *eventset);
eventset_bits_t eventset_wait_any(eventset_t *eventset,
                                  eventset_bits_t mask,
                                  os_tick_t timeout);
eventset_bits_t eventset_wait_all(eventset_t *eventset,
                                  eventset_bits_t mask,
                                  os_tick_t timeout);
```

Example:

```c
#define RX_READY (1UL << 0)
#define TX_DONE  (1UL << 1)

static eventset_t uart_events;

eventset_init(&uart_events);

void USART_IRQHandler(void)
{
    eventset_set(&uart_events, RX_READY);
}

eventset_bits_t bits =
    eventset_wait_any(&uart_events, RX_READY | TX_DONE, OS_WAIT_FOREVER);
```

Waiting does not clear bits. Clear consumed bits explicitly:

```c
eventset_clear(&uart_events, RX_READY);
```

Multiple waiters may wait on the same eventset. `eventset_set()` wakes every
waiter whose condition is satisfied. If bits are used as consumable events, the
application must define who clears them.

## Semaphore

Semaphore represents token count.

```c
void sem_init(sem_t *sem, uint16_t initial, uint16_t limit);
void sem_give(sem_t *sem);
bool sem_take(sem_t *sem, os_tick_t timeout);
```

Example:

```c
static sem_t rx_sem;

sem_init(&rx_sem, 0, 16);

void USART_IRQHandler(void)
{
    sem_give(&rx_sem);
}

if (sem_take(&rx_sem, OS_WAIT_FOREVER)) {
    consume_rx_packet();
}
```

If tasks are already waiting, `sem_give()` wakes the oldest waiter. If no task
is waiting, the token count increases up to `limit`.

Multiple consumers can wait on one semaphore. Each `sem_give()` wakes at most
one waiter.

## Message Queue

Message queue transfers fixed-size messages by copy.

```c
#define msgq_storage(name, msg_type, capacity) ...

void msgq_init(msgq_t *q, void *buffer, uint16_t msg_size, uint16_t capacity);
bool msgq_send(msgq_t *q, const void *msg, os_tick_t timeout);
bool msgq_recv(msgq_t *q, void *msg, os_tick_t timeout);
uint16_t msgq_count(msgq_t *q);
uint16_t msgq_space(msgq_t *q);
```

Example:

```c
typedef struct app_msg {
    uint16_t id;
    uint16_t value;
} app_msg_t;

static msgq_t app_msgq;
static msgq_storage(app_msgq_storage, app_msg_t, 8);

msgq_init(&app_msgq, app_msgq_storage, sizeof(app_msg_t), 8);
```

Send:

```c
app_msg_t msg = { 1, 123 };
bool ok = msgq_send(&app_msgq, &msg, 100);
```

Receive:

```c
app_msg_t msg;
bool ok = msgq_recv(&app_msgq, &msg, OS_WAIT_FOREVER);
```

`msgq` supports multiple producers and multiple consumers. Waiters are selected
FIFO, and scheduler priority decides when the woken task actually runs.

## Pipe

Pipe is a strict SPSC byte stream.

```c
#define pipe_storage(name, size) ...

void pipe_init(pipe_t *pipe, void *buffer, uint16_t size);
uint16_t pipe_write(pipe_t *pipe,
                    const void *data,
                    uint16_t len,
                    os_tick_t timeout);
uint16_t pipe_read(pipe_t *pipe,
                   void *data,
                   uint16_t len,
                   os_tick_t timeout);
uint16_t pipe_count(pipe_t *pipe);
uint16_t pipe_space(pipe_t *pipe);
void pipe_reset(pipe_t *pipe);
```

Example:

```c
static pipe_t uart_pipe;
static pipe_storage(uart_pipe_storage, 128);

pipe_init(&uart_pipe, uart_pipe_storage, sizeof(uart_pipe_storage));
```

ISR writer:

```c
void USART_IRQHandler(void)
{
    uint8_t byte = USART_READ_BYTE();
    (void)pipe_write(&uart_pipe, &byte, 1, OS_NO_WAIT);
}
```

Task reader:

```c
uint8_t buf[32];
uint16_t n = pipe_read(&uart_pipe, buf, sizeof(buf), OS_WAIT_FOREVER);
```

Pipe has no message boundary. A read or write may transfer fewer bytes than
requested.

Only one writer and one reader are allowed. If you need MPMC byte streaming,
build it explicitly with `bytebuf` plus synchronization.

## Soft Timer

Soft timer is optional and controlled by:

```c
#define OS_TIMER_ENABLE 1
```

API:

```c
void timer_init(soft_timer_t *timer, timer_callback_t callback, void *arg);
void timer_start(soft_timer_t *timer, os_tick_t delay, os_tick_t period);
void timer_stop(soft_timer_t *timer);
bool timer_is_running(const soft_timer_t *timer);
```

Callbacks run from the tick processing path. Keep callbacks short and
non-blocking. Prefer using them to notify a task rather than doing heavy work.
