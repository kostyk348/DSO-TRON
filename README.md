# DSO-TRON — Deterministic Systems Optimization for μITRON 4.0

[![CI](https://github.com/kostyk348/DSO-TRON/actions/workflows/ci.yml/badge.svg)](https://github.com/kostyk348/DSO-TRON/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C](https://img.shields.io/badge/C-99-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))

**DSO-TRON** — это детерминированное ядро RTOS, построенное на базе референсной реализации μITRON 4.0 (URIBO).  
Основные принципы: **Static Allocation**, **Lock-free IPC**, **Time-Triggered Scheduling**.

```
┌─────────────────────────────────────────────┐
│            DSO-TRON Kernel                   │
├─────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────┐│
│  │  Time-Triggered Scheduler               ││
│  │  (no priorities, no preemption)         ││
│  ├─────────────────────────────────────────┤│
│  │  Lock-free SPSC FIFO (IPC)              ││
│  │  (no mutexes, no spinlocks)             ││
│  ├─────────────────────────────────────────┤│
│  │  Static Arenas (TCB, Stack, Objects)    ││
│  │  (no malloc, no heap fragmentation)     ││
│  ├─────────────────────────────────────────┤│
│  │  μITRON 4.0 API (cre_tsk, snd_dtq...)  ││
│  └─────────────────────────────────────────┘│
└─────────────────────────────────────────────┘
```

## Why DSO-TRON?

Standard μITRON (URIBO) uses:
- **Dynamic memory allocation** (malloc) for TCBs, stacks, queues
- **Mutex-based IPC** with wait queues and context switches
- **Priority-based preemptive scheduling** with bitmap search

This makes timing unpredictable — exactly what's *not* needed in hard real-time systems.

**DSO-TRON replaces all three with deterministic alternatives:**

| Feature | μITRON (URIBO) | DSO-TRON |
|---------|----------------|----------|
| Memory | Heap (malloc) | Static Arenas |
| IPC | Mutex + wait queue | Lock-free SPSC FIFO |
| Scheduler | Priority + preemption | Time-Triggered |
| Timing | Unpredictable (cache, locks) | Deterministic (O(1) per tick) |
| Interrupts | Multiple IRQs | Single PIT timer |

## Architecture

### 1. Static Arenas (`dso_arena.h`)

No `malloc` anywhere. All kernel objects live in pre-allocated pools:

```
DSO_POOL(_kernel_tcb_pool, T_TCB, DSO_MAX_TSK);
DSO_ARENA(_dso_stack_arena, DSO_MAX_TSK * DSO_STACK_SIZE);
```

- **TCB pool** — fixed-size array of TCBs, allocated at compile time
- **Stack arena** — contiguous memory region, bump-allocated at `cre_tsk`
- **No free() needed** — objects live forever (embedded RTOS style)

### 2. Lock-free SPSC FIFO (`dso_fifo.h`)

Replaces μITRON's `dtq` (data queue) which uses mutexes, wait queues, and critical sections.

```c
// Producer (Task A)
dso_fifo_push(&queue, &data);   // writes tail, reads head (volatile)
// Consumer (Task B)  
dso_fifo_pop(&queue, &data);    // reads tail, writes head (volatile)
```

- **Single Producer, Single Consumer** — no locks needed
- **Power-of-2 ring buffer** — masking instead of division
- **Non-blocking** — returns error if full/empty, never waits
- **No interrupt disable** — safe even from ISR

### 3. Time-Triggered Scheduler (`dso_sched.h`)

Replaces μITRON's priority queue + bitmap scheduler.

```c
static const dso_slot schedule[] = {
    DSO_SLOT(1, 0, 10, 0,  500, sensor_task),   // every 100ms
    DSO_SLOT(2, 1, 10, 5,  500, control_task),  // 50ms after sensor
    DSO_SLOT(3, 2, 10, 8,  500, output_task),   // 30ms after control
};
```

- **Static schedule table** — computed at compile time
- **O(slots) per tick** — just iterate and check `tick % period == offset`
- **No priorities** — execution order = table order within a tick
- **No preemption** — tasks run to completion (or budget limit)
- **Single interrupt source** — PIT at 100Hz

### 4. Resource Contracts

Each slot has a `deadline` (max cycles) and optional `stack_bytes` budget.
Future: cycle counters, stack watermark, budget enforcement.

## Comparison: URIBO vs DSO-TRON

### Code comparison: creating a data queue

**URIBO (μITRON)** — dynamic allocation, mutex, waiting queue:
```c
ER cre_dtq(ID dtqid, const T_CDTQ *pk_cdtq)
{
    T_DTQ *dtq = malloc(sizeof(T_DTQ));
    dtq->buf   = malloc(pk_cdtq->dtqcnt * sizeof(VP_INT));
    dtq->rque  = (T_LNK){0};   // receive wait queue
    dtq->sque  = (T_LNK){0};   // send wait queue (MUTEX!)
    ...
}
```

**DSO-TRON** — static, lock-free:
```c
ER cre_dtq(ID dtqid, const T_CDTQ *pk_cdtq)
{
    dso_fifo_init(&_dso_dtq_table[idx].fifo,
                   _dso_dtq_table[idx].buf,
                   DSO_DTQ_ELEMENT_SIZE, DSO_DTQ_SIZE);
    ...
}
```

### Code comparison: sending data

**URIBO** — disables interrupts, acquires mutex, may block:
```c
ER snd_dtq(ID dtqid, VP_INT data)
{
    psw = dis_psw();
    if (queue full) {
        enqueue caller to waiting list;  // BLOCK!
        dispatch();
    }
    write to buffer;
    if (receiver waiting) dequeue_rcv();
    set_psw(psw);
}
```

**DSO-TRON** — lock-free, single write:
```c
ER snd_dtq(ID dtqid, VP_INT data)
{
    dso_fifo_push(q, &data);  // returns -1 if full
    ...
}
```

### Memory comparison

| Object | URIBO | DSO-TRON |
|--------|-------|----------|
| TCB | malloc(32) | Pool slot (static) |
| Stack | malloc(4096) | Arena slot (static) |
| DTQ | malloc(20+64) | Static array entry |
| DTQ buf | malloc(64) | Static array entry |
| Total | 3×malloc + 2×free | 0 malloc, 0 free |

## Building and Running

### Prerequisites

```bash
# Arch Linux / Debian / Ubuntu
sudo apt install build-essential lld grub2-common xorriso qemu-system-x86
```

### Build

```bash
cd dso-tron
make clean && make && make iso
```

### Run

```bash
make run                    # QEMU with VGA (GUI)
timeout 15 qemu-system-x86_64 -cdrom dso-tron.iso -nographic -serial mon:stdio
```

### Demo output

```
DSO-TRON v0.1 booting
IDT ok
PIC ok
PIT ok
DSO kernel init ok
Tasks created
DTQ created
TT schedule registered
Running...
T1:0x00000000 T2:0x00000000 T1:0x00000001 T2:0x00000001 ...
```

Each `T1:N T2:N` pair confirms the lock-free SPSC FIFO:
- Task1 pushes value N every 100ms (period=10, offset=0)
- Task2 pops value N every 100ms (period=10, offset=5, 50ms later)
- No data loss, no corruption, no races

## Project Structure

```
dso-tron/
├── include/           # DSO header files
│   ├── dso.h          # Main DSO header
│   ├── dso_arena.h    # Static arena allocator
│   ├── dso_config.h   # Compile-time configuration
│   ├── dso_fifo.h     # Lock-free SPSC FIFO
│   └── dso_sched.h    # Time-Triggered scheduler
├── kernel/            # μITRON-compatible kernel
│   ├── kernel.h       # μITRON types, TCB, syscall decls
│   ├── dso_common.c   # Scheduler + dispatch core
│   ├── dso_init.c     # Kernel initialization
│   ├── dso_tsk.c      # Task management (cre/sta/ext/ter...)
│   ├── dso_dtq.c      # Data queue (SPSC FIFO wrapper)
│   ├── dso_stubs.c    # Stubs for unsupported syscalls
│   └── main.c         # Demo application
├── sysdepend/x86_64/  # x86_64 architecture layer
│   ├── boot.S         # Multiboot2 + Long Mode entry
│   ├── ctx_switch.S   # Context switch assembly
│   ├── cpu.h          # CPU macros (DS_PSW, INI_CTX, SW...)
│   ├── gdt.c          # GDT (currently boot-only)
│   ├── idt.c          # IDT setup
│   ├── pic.c          # PIC 8259 setup
│   ├── pit.c          # PIT timer + IRQ handler
│   ├── vga.c          # VGA text mode (0xB8000)
│   ├── serial.c/h     # Serial port (COM1)
│   └── linker.ld      # Linker script (KERNEL_BASE=0xFFFFFF8000000000)
├── Makefile
├── LICENSE
└── README.md
```

## Origin

- **URIBO** — reference implementation of μITRON 4.0 by Hiroaki Takada Lab
  (used under BSD-like license for educational purposes)
- **μT-Kernel 3.0** — TRON Forum standard (IEEE 2050-2018), inspiration for dispatch architecture

DSO-TRON is not a fork of URIBO. It uses URIBO's API surface and type system
as a specification reference, reimplementing all kernel services from scratch
with deterministic design.

## License

Apache 2.0. See `LICENSE` for details.
