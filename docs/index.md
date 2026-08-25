# MicaOS Documentation Index

Documentation Boundaries:

- User guides explain what users should do.
- Architecture notes explain why the system is designed that way.

## If You Want to Use MicaOS

Read these first:

1. [Getting started](getting-started.md)
2. [Configuration and build](configuration-and-build.md)
3. [Kernel guide](kernel-guide.md)
4. [IPC guide](ipc-guide.md)

These documents explain how to configure, build, create tasks, start the
scheduler, use delays, and use synchronization or communication primitives.

## If You Want to Use the Bus Service

Read:

- [Message bus guide](message-bus-guide.md)

The bus is an application-level event/state publish-subscribe service built on
top of the kernel.

## If You Want to Use Utility Modules

Read:

- [Memory and data structures](memory-and-data-structures.md)

This document explains where to find slab, dlist, slist, bitmap, bytebuf, and
packetbuf usage information.

## If You Want to Debug

Read:

1. [Configuration and build](configuration-and-build.md)
2. [Porting guide](porting-guide.md)
3. [Debugging guide](debugging-guide.md)
4. [Testing guide](testing-guide.md)

## If You Are Maintaining or Extending MicaOS

Read:

- [Architecture notes](architecture-notes.md)

This document is not a first-use guide. It explains design boundaries, project
invariants, and safe modification rules.
