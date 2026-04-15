# Operating System Kernel (Z502 Simulator)

A custom multitasking operating system kernel implemented in C, running on top of a simulated Z502 hardware platform. This project implements core OS subsystems from scratch: process scheduling, virtual memory with demand paging, disk I/O, and inter-process communication.

---

## Overview

The kernel runs on the Z502 hardware simulator (`z502.c`), which emulates a virtual CPU with physical memory, a timer, 8 disk devices, hardware interrupts, and memory-mapped I/O. The OS layer (`os_process.c`) sits above this hardware and implements everything a real OS would provide.

---

## Architecture

```
┌─────────────────────────────────────────┐
│           User Test Code                │
│     (test.c / sample.c)                 │
└─────────────────────────────────────────┘
                   │
      System Calls / Interrupts / Faults
                   │
┌─────────────────────────────────────────┐
│           OS Kernel (os_process.c)      │
│                                         │
│  Scheduler  │  IPC Engine  │  Memory    │
│  (Ready Q)  │  (Messages)  │  Manager   │
│  (Timer Q)  │  (Suspend Q) │  (Paging)  │
│  (Disk Q×8) │              │            │
└─────────────────────────────────────────┘
                   │
┌─────────────────────────────────────────┐
│       Z502 Hardware Simulator           │
│  64-frame physical memory, Timer,       │
│  8 Disk Devices, pthreads-based threads │
└─────────────────────────────────────────┘
```

---

## Features

### Process Management
- Supports up to 10 concurrent processes
- Priority-based preemptive scheduling via a sorted ready queue
- Process lifecycle: create, suspend, resume, terminate
- Each process runs in its own hardware thread (pthreads)

### Virtual Memory
- Per-process virtual address space: 1024 virtual pages
- Physical memory: 64 frames × 16 bytes = 1024 bytes
- Demand paging with FIFO page replacement
- 16-bit page table entries with VALID, MODIFIED, and REFERENCED bits
- Pages evicted to disk; shadow page table tracks disk locations

### Disk I/O
- 8 independent disk devices (devices 5–12)
- Interrupt-driven: calling process suspends until I/O completes
- Per-disk queues; disk interrupt wakes the next waiting process

### Inter-Process Communication
- Message passing with per-message source/target PIDs
- Broadcast support (target = -1)
- Processes block on receive if no message is available; woken immediately on send
- Message queue with up to 500-byte payloads

### Interrupt & Exception Handling
- Timer interrupt: wakes sleeping processes whose time has elapsed
- Disk interrupt: signals I/O completion, resumes blocked process
- Page fault handler: allocates frame or evicts a victim, then restarts instruction
- System call dispatcher: routes 16 syscall types via software interrupt

### Synchronization
- Hardware-level atomic `READ_MODIFY` locks
- Per-subsystem locks: `READYQ_LOCK`, `TIMERQ_LOCK`, `SUSPENDQ_LOCK`, `DISK_LOCK`, `FRAME_LOCK`

---

## File Structure

| File | Description |
|------|-------------|
| `os_process.c` | Core kernel: scheduler, IPC, memory manager |
| `os_pcb.h` | Process Control Block and queue definitions |
| `base.c` | Interrupt handlers and system call dispatch |
| `z502.c` | Z502 hardware simulator (threads, memory, devices) |
| `z502.h` | Hardware constants and register definitions |
| `syscalls.h` | System call macros (user-facing API) |
| `global.h` | Shared constants and typedefs |
| `state_printer.c` | Debug output utilities |
| `test.c` | Test suite (test0 through test2h) |
| `sample.c` | Example programs and API documentation |

---

## System Calls

| Category | Syscalls |
|----------|---------|
| Process | `CREATE`, `TERMINATE`, `SUSPEND`, `RESUME`, `SLEEP`, `CHANGE_PRIORITY` |
| Memory | `MEM_READ`, `MEM_WRITE`, `READ_MODIFY`, `DEFINE_SHARED_AREA` |
| IPC | `SEND_MESSAGE`, `RECEIVE_MESSAGE` |
| Disk | `DISK_READ`, `DISK_WRITE` |
| Time | `GET_TIME_OF_DAY` |

---

## Build & Run

```bash
# Linux / macOS
gcc -o os z502.c os_process.c os_pcb.c base.c state_printer.c test.c -lpthread

# Run
./os
```

---

## Implementation Highlights

- **FIFO page replacement**: victim frame encoded as a 32-bit integer (frame ID + PID + page ID) for efficient eviction tracking
- **Priority-ordered linked lists**: O(n) insertion maintains O(1) dequeue for the scheduler
- **Cascading timer wakeups**: a single timer interrupt can unblock multiple processes in one pass
- **Shadow page table**: two-level lookup (process × disk sector) tracks exactly where each evicted page lives on disk
- **Portable builds**: conditional `#ifdef` blocks support Linux, macOS, and Windows
