# LAB03 — Debugging and Performance Analysis

**Course:** LOS201 — Embedded Linux Systems, FPT University

**Contributor:** Dinh Quang Ngoc — SE201573

## Overview

This lab investigates debugging techniques, memory management, performance profiling, and real-time scheduling behavior in Linux-based embedded systems.

The practical work covers remote debugging of an ARM application using GDB and QEMU, memory leak detection with Valgrind, system call tracing with strace, CPU profiling with perf, and scheduling latency measurement with cyclictest.

The repository contains application source code, executable files, debugging transcripts, profiling results, a latency histogram, and the QEMU initramfs used in the experiment.

## Contents

- [Platform](#platform)
- [Repository Materials](#repository-materials)
- [Quick Start](#quick-start)
- [GDB Debugging](#gdb-debugging)
- [Memory Analysis](#memory-analysis)
- [Performance Profiling](#performance-profiling)
- [Real-Time Latency Analysis](#real-time-latency-analysis)
- [Troubleshooting](#troubleshooting)
- [Conclusion](#conclusion)
- [References](#references)

## Platform

| Component | Configuration |
|---|---|
| Development host | Ubuntu Linux, x86-64 |
| Embedded target | ARM Linux |
| Emulation | QEMU |
| Programming language | C |
| Cross-debugger | GDB Multiarch |
| Memory analyzer | Valgrind |
| System call tracer | strace |
| CPU profiler | Linux perf |
| Latency measurement | cyclictest |
| Target filesystem | Compressed initramfs |

The experiments were performed on an Ubuntu host, with QEMU used for ARM application debugging. Host-based measurements and ARM guest experiments are distinguished in the following sections.

## Repository Materials

| Path | Purpose |
|---|---|
| [apps/buggy_app.c](apps/buggy_app.c) | ARM application source code after bug fixes |
| [apps/buggy_app](apps/buggy_app) | Compiled ARM application |
| [apps/leaky_app.c](apps/leaky_app.c) | Original memory analysis application |
| [apps/leaky_app](apps/leaky_app) | Original compiled application |
| [apps/leaky_app_fixed.c](apps/leaky_app_fixed.c) | Corrected memory management source |
| [apps/leaky_app_fixed](apps/leaky_app_fixed) | Corrected executable |
| [apps/perf_target.c](apps/perf_target.c) | Performance profiling source |
| [logs/gdb_session.txt](logs/gdb_session.txt) | GDB debugging transcript |
| [logs/valgrind_report.txt](logs/valgrind_report.txt) | Valgrind results before fixing |
| [logs/valgrind_fixed.txt](logs/valgrind_fixed.txt) | Valgrind results after fixing |
| [logs/strace_summary.txt](logs/strace_summary.txt) | System call statistics |
| [logs/perf_stat.txt](logs/perf_stat.txt) | CPU performance counters |
| [logs/perf_report.txt](logs/perf_report.txt) | CPU profiling results |
| [logs/hist_standard.txt](logs/hist_standard.txt) | Standard kernel latency measurements |
| [logs/latency_histogram.png](logs/latency_histogram.png) | Scheduling latency histogram |
| [output/initramfs_lab3.cpio.gz](output/initramfs_lab3.cpio.gz) | ARM initramfs used with QEMU |

The debugging transcript records the investigation of the original buggy program. The submitted `buggy_app.c` and executable contain the corrected implementation.

## Quick Start

Clone the repository:

```bash
git clone https://github.com/phuctran2001/LOS201-Embedded-Linux-Group.git
cd LOS201-Embedded-Linux-Group/lab03
```

Inspect the provided files:

```bash
ls apps/
ls logs/
ls output/
```

Read the experiment logs:

```bash
cat logs/gdb_session.txt
cat logs/valgrind_report.txt
cat logs/valgrind_fixed.txt
cat logs/strace_summary.txt
cat logs/perf_stat.txt
cat logs/hist_standard.txt
```

The ARM executable requires a compatible ARM Linux environment. The included initramfs is intended for use with the corresponding QEMU kernel and device tree, which are not bundled in this LAB-03 directory.

## GDB Debugging

### Objective

Use GDB Multiarch and QEMU to identify and correct three programming errors in an ARM application.

### Identified Bugs

| Bug | Technical Cause | Correction |
|---|---|---|
| Array out-of-bounds | The loop accessed an index beyond the valid array range | Change `i <= len` to `i < len` |
| Use-after-free | A function returned a pointer to already-freed memory | Keep memory valid until the caller finishes using it |
| Integer overflow | A 32-bit integer could not represent the multiplication result | Use a wider integer type for the calculation |

### Debugging Results

| Operation | Before Fix | After Fix |
|---|---|---|
| Array sum | Incorrect output | `150` |
| String handling | Invalid message | `Hello Embedded Linux` |
| Multiplication | `1410065408` | `10000000000` |

The corrected program produces the expected output:

```text
Sum = 150
Message: Hello Embedded Linux
calculate(100000, 100000) = 10000000000
```

The full recorded debugging session is available in [gdb_session.txt](logs/gdb_session.txt).

## Memory Analysis

### Objective

Detect memory leaks in a C application using Valgrind and verify the results after correcting memory management.

### Original Valgrind Results

The initial analysis reported:

```text
definitely lost: 100 bytes in 1 blocks
indirectly lost: 0 bytes in 0 blocks
possibly lost: 0 bytes in 0 blocks
still reachable: 0 bytes in 0 blocks

ERROR SUMMARY: 3 errors from 3 contexts
```

The confirmed leaked allocation was associated with `malloc` in `leaky_app.c:9`.

### Results After Fixing

```text
in use at exit: 0 bytes in 0 blocks
total heap usage: 3 allocs, 3 frees

All heap blocks were freed -- no leaks are possible

ERROR SUMMARY: 0 errors from 0 contexts
```

### Comparison

| Metric | Before Fix | After Fix |
|---|---:|---:|
| Definitely lost | 100 bytes | 0 bytes |
| Indirectly lost | 0 bytes | 0 bytes |
| Allocations | 3 | 3 |
| Frees | 2 | 3 |
| Valgrind errors | 3 | 0 |

The corrected application releases all allocated memory before exiting.

**Evidence:**
- [Original Valgrind report](logs/valgrind_report.txt)
- [Fixed Valgrind report](logs/valgrind_fixed.txt)

## Performance Profiling

### System Call Analysis with strace

The `strace -c` command summarizes the number of system calls and their execution times.

The five system calls with the highest recorded time percentages were:

| System Call | Time (%) | Calls |
|---|---:|---:|
| execve | 65.99 | 1 |
| mmap | 11.13 | 8 |
| mprotect | 4.15 | 3 |
| write | 3.95 | 4 |
| brk | 2.43 | 3 |

The results show that process execution and memory-related operations account for a significant portion of the measured system call time.

See [strace_summary.txt](logs/strace_summary.txt) for the complete output.

### CPU Profiling with perf

The `perf` utility was used to examine CPU activity and execution hotspots.

Selected recorded results:

| Metric | Result |
|---|---:|
| CPU cycles | 400,663,324 |
| Instructions | 2,044,198,217 |
| Instructions per cycle | Approximately 5.1 |
| Context switches | 3 |
| CPU migrations | 1 |
| Page faults | 63 |
| Elapsed time | Approximately 0.095 seconds |

The profiling report attributes approximately 99.68% of sampled execution overhead to the application's `main` function, suggesting a predominantly CPU-bound workload.

**Evidence:**
- [perf_stat.txt](logs/perf_stat.txt)
- [perf_report.txt](logs/perf_report.txt)

## Real-Time Latency Analysis

### Objective

Measure scheduling latency on a standard Linux kernel and evaluate whether the observed behavior meets a real-time response requirement.

### Standard Kernel Results

| Metric | Latency |
|---|---:|
| Minimum | 10 µs |
| Average | 76 µs |
| Maximum | 550 µs |
| Histogram overflows | 0 |

The maximum recorded latency was substantially higher than the average.

For an application requiring response latency below 500 µs, the observed maximum of 550 µs exceeds the specified threshold.

A PREEMPT_RT kernel is a potential improvement because it makes more kernel execution paths preemptible and reduces certain sources of scheduling delay. However, meeting a strict deadline requires additional testing under representative system loads.

### Latency Histogram

![Standard Kernel Latency Histogram](logs/latency_histogram.png)

The original measurement data is available in [hist_standard.txt](logs/hist_standard.txt).

## Troubleshooting

| Issue | Explanation or Solution |
|---|---|
| Incorrect array calculation | Check loop boundaries and ensure array indices remain within valid limits |
| Invalid string output | Avoid accessing dynamically allocated memory after calling `free()` |
| Incorrect multiplication result | Use an appropriate integer type and matching format specifier |
| Valgrind reports memory leaks | Check allocation paths and ensure allocated memory is released |
| ARM executable cannot run on Ubuntu x86-64 | Execute the binary in a compatible ARM environment, such as QEMU |
| perf reports limited symbol information | Build with debug symbols and inspect the profiling report |
| High maximum scheduling latency | Examine system load and consider PREEMPT_RT for time-sensitive applications |

## Conclusion

This lab demonstrated several important tools for debugging and performance analysis in Embedded Linux development.

GDB helped locate programming errors, Valgrind identified memory management problems, strace revealed system call activity, perf highlighted CPU execution hotspots, and cyclictest measured scheduling latency.

The experiments emphasize the importance of correct memory handling, systematic debugging, performance measurement, and real-time behavior verification when developing reliable embedded applications.

## References

- Lecturer-provided LAB-03 specification.
- [GNU GDB Documentation](https://sourceware.org/gdb/documentation/)
- [Valgrind Documentation](https://valgrind.org/docs/manual/manual.html)
- [strace Project](https://strace.io/)
- [Linux perf Documentation](https://perfwiki.github.io/main/)
- [QEMU Documentation](https://www.qemu.org/docs/master/)
- [Linux Real-Time Documentation](https://docs.kernel.org/core-api/real-time/index.html)

---

[Back to the group repository overview](../README.md)
