# Memory and Data Structures

MicaOS provides small static-memory modules that can be used by the kernel or
by application code.

The detailed API comments live in the module headers. This document explains
when to choose each module.

## Slab

Location:

```text
include/micaos/memory/slab.h
src/memory/slab.c
```

Use slab when you need a fixed-size object pool:

- task object pool
- timer object pool
- packet descriptor pool
- driver request pool

The backing storage is provided by the user. There is no dynamic allocation.

Typical pattern:

```c
typedef struct request {
    uint32_t id;
    uint8_t payload[32];
} request_t;

static slab_t request_pool;
static slab_storage(request_pool_storage, request_t, 8);

slab_init(&request_pool,
          "request_pool",
          request_pool_storage,
          sizeof(request_t),
          8);
```

## Dlist

Location:

```text
include/micaos/data_structure/dlist.h
```

Intrusive doubly linked list.

Use it when:

- objects need to be removed from the middle in O(1)
- an object can own its list node
- no allocation should happen inside the container

The kernel uses dlist for ready queues, wait queues, and timeout lists.

## Slist

Location:

```text
include/micaos/data_structure/slist.h
```

Intrusive singly linked list.

Use it when:

- forward traversal is enough
- you want smaller node storage than dlist
- removal from arbitrary middle positions is uncommon or can be handled by
  scanning

## Bitmap

Location:

```text
include/micaos/data_structure/bitmap.h
```

Fixed-size bitmap helper.

Use it when:

- tracking used/free indexes
- tracking priority groups
- quickly finding set or clear bits

The scheduler uses bitmap to find non-empty priority queues.

## Bytebuf

Location:

```text
include/micaos/data_structure/bytebuf.h
src/data_structure/bytebuf.c
```

SPSC byte ring buffer without blocking semantics.

Use it when:

- one producer writes bytes
- one consumer reads bytes
- synchronization is handled elsewhere

If used from MPMC context, add your own critical section.

## Packetbuf

Location:

```text
include/micaos/data_structure/packetbuf.h
src/data_structure/packetbuf.c
```

SPSC variable-size packet buffer without blocking semantics.

Use it when:

- packet boundaries matter
- one producer writes packets
- one consumer reads packets
- synchronization is handled elsewhere

`packetbuf` may add padding near ring wraparound so each packet remains
contiguous. This can temporarily waste storage, but keeps read/write logic
simple and predictable.

## Coroutine

Location:

```text
include/micaos/coroutine/coro.h
```

Lightweight stackless switch-case coroutine.

Use it when:

- a small state machine reads better as step-by-step code
- the code does not need an independent task stack
- the coroutine is driven explicitly by caller code

`coro` stores only a resume state. Automatic local variables do not survive a
yield or wait, so persistent values should be stored in the owner object. Put
each yield/wait/restart/exit macro on its own source line.

## Choosing a Module

| Need | Module |
| --- | --- |
| Fixed-size object pool | `slab` |
| O(1) remove from linked object lists | `dlist` |
| Small forward-only intrusive list | `slist` |
| Track bits or priority groups | `bitmap` |
| SPSC byte buffer | `bytebuf` |
| SPSC variable-size packet buffer | `packetbuf` |
| Small stackless coroutine | `coro` |
| Blocking fixed-size message passing | `msgq` |
| Blocking SPSC byte stream | `pipe` |

## Synchronization

These modules are mostly data containers. They do not automatically protect
against concurrent access unless the module explicitly says so.

When using them between tasks or between ISR/task contexts, choose an
appropriate synchronization method:

- short critical section
- `task_notify`
- `eventset`
- `sem`
- `msgq`
- `pipe`
