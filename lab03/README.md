# LAB03 — Debugging and Performance Analysis

**Contributor:** Đinh Quang Ngọc 

**Course:** LOS201 — Embedded Linux Systems, FPT University  
**Platform:** Ubuntu Linux (x86-64) / ARM Cortex-A9 (QEMU)  
**Language:** C  
**Development Tools:** GDB Multiarch, Valgrind, strace, perf, cyclictest
## Overview

This lab explores debugging techniques and performance analysis for Embedded Linux applications. The experiments focus on identifying programming errors, detecting memory leaks, analyzing system calls, profiling CPU performance, and measuring real-time scheduling latency.

The lab consists of four main parts:

- **Part A:** Remote Debugging with GDB
- **Part B:** Memory Leak Detection with Valgrind
- **Part C:** System Call Tracing and CPU Profiling
- **Part D:** Real-Time Latency Analysis with cyclictest

The practical work and experimental evidence are documented in the [Lab 03 Report](report/SE201573_DinhQuangNgoc_Lab03_Report.pdf).

## Contents

- [Platform](#platform)
- [System Architecture](#system-architecture)
- [Repository Materials](#repository-materials)
- [Quick Start](#quick-start)
- [Lab Workflow](#lab-workflow)
- [Testing and Verification](#testing-and-verification)
- [Troubleshooting](#troubleshooting)
- [References](#references)

## Platform

| Component | Configuration |
|---|---|
| Development Host | Ubuntu Linux (x86-64) |
| Target Architecture | ARMv7-A |
| Target CPU | Cortex-A9 |
| QEMU Machine | `vexpress-a9` |
| Cross-Compiler | `arm-linux-gnueabihf-gcc` |
| Remote Debugger | GDB Multiarch / gdbserver |
| Memory Analyzer | Valgrind |
| Performance Tools | strace, perf |
| Latency Measurement | cyclictest |

## System Architecture

<img width="1484" height="1060" alt="image" src="https://github.com/user-attachments/assets/45eb2317-13b2-4a0b-8a2b-33475b8722cb" />


The development environment consists of an Ubuntu host and an ARM target emulated using QEMU.

GDB Multiarch connects to `gdbserver` on the ARM target for remote debugging. Valgrind, strace, perf, and cyclictest are used for memory, system-call, performance, and latency analysis on the Ubuntu host.

## Repository Materials

| Path | Description |
|---|---|
| `apps/` | C source code and executable programs |
| `logs/` | Debugging and performance analysis results |
| `output/` | ARM initramfs and generated artifacts |
| `report/` | Lab report and experimental evidence |
| `README.md` | Project documentation |

### Repository Structure

```text
lab03/
├── apps/
│   ├── buggy_app.c
│   ├── buggy_app
│   ├── leaky_app.c
│   ├── leaky_app_fixed.c
│   └── perf_target.c
├── logs/
│   ├── gdb_session.txt
│   ├── valgrind_report.txt
│   ├── valgrind_fixed.txt
│   ├── strace_summary.txt
│   ├── perf_stat.txt
│   ├── perf_report.txt
│   └── hist_standard.txt
├── output/
│   └── initramfs_lab3.cpio.gz
├── report/
│   └── SE201573_DinhQuangNgoc_Lab03_Report.pdf
└── README.md
```

*The directory layout shows the main project materials. Actual generated files may vary.*

## Quick Start

### 1. Install Required Tools

Run on the Ubuntu host:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  gcc-arm-linux-gnueabihf \
  gdb-multiarch \
  qemu-system-arm \
  valgrind \
  strace \
  linux-perf \
  rt-tests
```

Package availability may vary by Ubuntu version.

### 2. Build Applications

Compile the ARM debugging application:

```bash
arm-linux-gnueabihf-gcc \
  -g -O0 apps/buggy_app.c \
  -o apps/buggy_app
```

Compile the host-native memory analysis application:

```bash
gcc -g -O0 \
  apps/leaky_app.c \
  -o apps/leaky_app
```

Compile the profiling application:

```bash
gcc -g -O2 \
  apps/perf_target.c \
  -o apps/perf_target
```

## Lab Workflow

### Part A — GDB Remote Debugging

**Objective:** Identify and fix programming errors using GDB.

The ARM application runs inside QEMU, while GDB Multiarch operates on the Ubuntu host.

On the ARM target:

```bash
gdbserver :1234 ./buggy_app
```

On the Ubuntu host:

```bash
gdb-multiarch ./apps/buggy_app
```

Inside GDB:

```gdb
target remote :1234
break main
continue
info locals
backtrace
```

The remote connection requires the correct QEMU network or port-forwarding configuration.

**Analysis:** Inspect variables, breakpoints, and call stacks to identify defects such as array out-of-bounds access, use-after-free, and integer overflow.

### Part B — Memory Analysis with Valgrind

**Objective:** Detect memory leaks and verify memory management corrections.

```bash
valgrind --leak-check=full \
  --show-leak-kinds=all \
  ./apps/leaky_app
```

Compare the original application with the corrected version:

```bash
gcc -g -O0 apps/leaky_app_fixed.c \
  -o apps/leaky_app_fixed

valgrind --leak-check=full \
  ./apps/leaky_app_fixed
```

**Analysis:** Compare memory allocations, leaked blocks, and the final Valgrind error summary.

### Part C — System Call and Performance Analysis

**Objective:** Investigate application behavior and identify performance bottlenecks.

System call tracing:

```bash
strace -c ./apps/perf_target
```

CPU performance measurement:

```bash
perf stat ./apps/perf_target
```

Performance profiling:

```bash
perf record ./apps/perf_target
perf report
```

**Analysis:** Examine system-call frequency, CPU execution statistics, and functions with high resource consumption.

### Part D — Real-Time Latency Analysis

**Objective:** Measure scheduling latency using cyclictest.

```bash
sudo cyclictest \
  --mlockall \
  --threads=4 \
  --priority=80 \
  --interval=1000 \
  --loops=10000
```

**Analysis:** Record minimum, average, and maximum latency values. Generate a histogram to examine the latency distribution and evaluate scheduling behavior.

Measurements may vary depending on CPU load, kernel configuration, and host environment.

## Testing and Verification

| Test | Verification |
|---|---|
| GDB Debugging | Breakpoints, variables, and backtraces inspected |
| Bug Fixing | Original and corrected application behavior compared |
| Valgrind | Memory leaks identified and corrected |
| strace | System-call summary collected |
| perf | CPU profiling data recorded |
| cyclictest | Scheduling latency measurements collected |
| Documentation | Logs, screenshots, and report organized |

Detailed experimental outputs should be checked in the `logs/` directory and the [Lab 03 Report](report/SE201573_DinhQuangNgoc_Lab03_Report.pdf).

## Troubleshooting

| Problem | Suggested Solution |
|---|---|
| GDB cannot connect | Check gdbserver, target IP address, and port forwarding |
| Executable format error | Verify ARM versus x86-64 binary architecture |
| Valgrind cannot execute ARM binary | Use the corresponding host-native executable |
| perf permission denied | Check Linux perf permissions and kernel restrictions |
| cyclictest requires privileges | Run with appropriate scheduling permissions |
| README image not displayed | Verify the image path and filename |
| Report link returns 404 | Confirm the report directory and exact PDF filename |

## References

- Lecturer-provided LAB03 specification.
- [GNU GDB Documentation](https://sourceware.org/gdb/documentation/)
- [Valgrind Documentation](https://valgrind.org/docs/manual/manual.html)
- [Linux perf Documentation](https://perf.wiki.kernel.org/)
- [Linux Real-Time Testing Tools](https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/rt-tests)
- [QEMU Documentation](https://www.qemu.org/docs/master/)

---

[Back to Group Repository](../README.md)
