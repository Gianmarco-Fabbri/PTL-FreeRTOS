# PTL - Periodic Task Layer for FreeRTOS

> **A lightweight middleware for deadline-aware periodic task scheduling on top of FreeRTOS**

[![FreeRTOS](https://img.shields.io/badge/FreeRTOS-V202212.00-green)](https://www.freertos.org/)

[![Platform](https://img.shields.io/badge/Platform-Cortex--M3-blue)]()

[![License](https://img.shields.io/badge/License-MIT-yellow)](LICENSE)

## Overview

The **Periodic Task Layer (PTL)** extends FreeRTOS with first-class support for periodic real-time tasks.

PTL adds native support for **periodicity**, **deadlines**, and **overrun handling** as a thin middleware layer, enabling:

- Declaration of tasks with explicit **period (T)** and **deadline (D)**
- Automatic **job releases** at correct times
- Detection and handling of **deadline misses** and **period overruns**
- Three configurable **overrun policies**: SKIP, KILL, CATCH_UP

The design prioritizes **minimal kernel intrusion** for easy portability to different FreeRTOS ports.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      User Application                        │
│   ┌──────────┐  ┌──────────┐  ┌──────────┐                 │
│   │  Task A  │  │  Task B  │  │  Task C  │   (Job Bodies)  │
│   └────┬─────┘  └────┬─────┘  └────┬─────┘                 │
├────────┼─────────────┼─────────────┼────────────────────────┤
│        └─────────────┼─────────────┘                        │
│                      ▼                                       │
│   ┌──────────────────────────────────────┐                  │
│   │     PTL - Periodic Task Layer        │                  │
│   │  ┌────────────┐  ┌─────────────────┐ │                  │
│   │  │ Supervisor │  │ Generic Wrapper │ │                  │
│   │  │ (Releases) │  │ (Deadline Check)│ │                  │
│   │  └────────────┘  └─────────────────┘ │                  │
│   │  ┌─────────────────────────────────┐ │                  │
│   │  │     Trace System (Events)       │ │                  │
│   │  └─────────────────────────────────┘ │                  │
│   └──────────────────────────────────────┘                  │
├─────────────────────────────────────────────────────────────┤
│                  FreeRTOS Kernel                             │
│         (Priority-based Preemptive Scheduling)              │
└─────────────────────────────────────────────────────────────┘
```
## Prerequisites
-**ARM GCC Toolchain**: `arm-none-eabi-gcc`
-**QEMU**: For Cortex-M3 emulation (`qemu-system-arm`)
-**Make**: Build automation
-**Python 3** (optional): For configuration generator
#### macOS Installation
```bash
brewinstallarm-none-eabi-gccqemu
```
#### Ubuntu/Debian Installation
```bash
sudoaptinstallgcc-arm-none-eabiqemu-system-arm
```
### Building
```bash
cdProject
makecleanall
```
### Running on QEMU
```bash
makeqemu_start
```
To run with GDB debugging enabled:
```bash
# Terminal 1: Start QEMU with debug server
makeqemu_debug

# Terminal 2: Connect GDB
makegdb_start
```

### Quick Demo

The default `main.c` demonstrates:

1.**Sensor task**: Normal periodic execution (10ms work, 100ms period)

2.**ImageProc task**: Intentionally exceeds deadline → KILL policy terminates it

3.**Logger task**: Runs late → SKIP policy allows completion, skips next release

## Testing

### Running All Tests

```bash
maketests
```
This executes the automated regression suite:
| Test                          | Description                                        |
| ----------------------------- | -------------------------------------------------- |
| `test_init_safety.c`          | Validates NULL/invalid config rejection            |
| `test_policy_kill_trace.c`    | Verifies KILL policy terminates overrunning tasks  |
| `test_policy_skip_trace.c`    | Verifies SKIP policy skips releases during overrun |
| `test_policy_catchup_trace.c` | Verifies CATCH_UP marks missed and continues       |
| `test_production_check.c`     | Stress test with CPU overhead validation           |
| `test_load_on_trace.c`        | High-frequency tasks, trace buffer wrap test       |
| `test_preemption.c`           | Validates priority-based preemption behavior       |

### Running a Single Test

```bash
makerun_single_testENTRY_POINT=Tests/test_policy_kill_trace.c
```

---

## Project Structure

```
group38/
├── README.md                 # This file
├── .gitignore                # Git ignore rules
├── Project-Assignment-2026.md # Assignment specification
├── WorkSplit.rtf             # Team task distribution
│
├── FreeRTOS/                 # FreeRTOS kernel (unmodified)
│   ├── include/
│   ├── portable/
│   └── *.c
│
└── Project/                  # Our implementation
    ├── FreeRTOSConfig.h      # FreeRTOS configuration + trace hooks
    ├── main.c                # Demo application
    ├── Makefile              # Build system
    ├── startup.c             # Cortex-M3 startup code
    ├── mps2_m3.ld            # Linker script
    │
    ├── PTL/                  # Periodic Task Layer
    │   ├── include/
    │   │   ├── ptl.h         # Core API
    │   │   ├── ptl_events.h  # Event types
    │   │   └── ptl_trace.h   # Trace API
    │   └── Source/
    │       ├── ptl_scheduler.c  # Supervisor task
    │       ├── ptl_wrapper.c    # Task wrapper & init
    │       └── ptl_trace.c      # Trace system
    │
    ├── utils/                # Utility modules
    │   ├── uart.c/h          # UART driver
    │   └── burner.c/h        # CPU cycle burner
    │
    ├── Tests/                # Automated test suite
    │   └── test_*.c
    │
    └── Tools/                # Optional tools
        └── framework.py      # Config generator
```

---

## Authors

**Riccardo Bartolini** | s362034@studenti.polito.it

**Rocco Caliandro**    | s359509@studenti.polito.it

**Gianmarco Fabbri**   | s361504@studenti.polito.it

**Davide Rossi**       | s359504@studenti.polito.it

### Group 38 - Real-Time Systems Project 2025/2026