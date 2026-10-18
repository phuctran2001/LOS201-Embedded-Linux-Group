LAB03 — Debugging and Performance Analysis
Course: Embedded Linux
Student: Dinh Quang Ngoc
Student ID: SE201573
Host environment: Ubuntu Linux
Target environment: ARM Linux running in QEMU
Project directory: lab03/
1. Overview
This laboratory investigates software debugging, dynamic memory management, system-call tracing, CPU profiling, and real-time scheduling latency in an Embedded Linux workflow. The experiments use GDB/GDB Multiarch, Valgrind, strace, perf, and cyclictest. ARM debugging is performed against an emulated target, while memory and performance measurements are collected on the Ubuntu host unless explicitly noted.
The repository preserves source code, executable artifacts, raw tool output, and the latency histogram so that results can be inspected independently.
2. Objectives
- Locate and explain defects in a C application using GDB.
- Compare program behavior before and after fixing the defects.
- Detect and resolve memory leaks using Valgrind.
- Identify frequently used system calls and CPU hotspots.
- Measure scheduling latency and assess real-time suitability.
- Preserve reproducible build instructions, commands, and evidence.
3. Repository Structure
lab03/
├── README.md
├── apps/
│   ├── buggy_app               # ARM executable; current submitted build is corrected
│   ├── buggy_app.c             # Corrected ARM source
│   ├── leaky_app               # Original Ubuntu executable
│   ├── leaky_app.c             # Original source with a memory leak
│   ├── leaky_app_fixed         # Corrected Ubuntu executable
│   ├── leaky_app_fixed.c       # Corrected source
│   └── perf_target.c          # CPU profiling workload
├── logs/
│   ├── gdb_session.txt
│   ├── valgrind_report.txt
│   ├── valgrind_fixed.txt
│   ├── strace_summary.txt
│   ├── perf_stat.txt
│   ├── perf_report.txt
│   ├── hist_standard.txt
│   └── latency_histogram.png
├── output/
│   └── initramfs_lab3.cpio.gz
└── baocao/
    └── SE201573_Lab03_BaoCao.pdf   # Add when the final report is ready
Important reproducibility limitation: The currently submitted buggy_app.c and buggy_app are corrected versions. To reproduce the original three defects, the original buggy source and its matching ARM binary must also be retained (for example, apps/buggy_app_original.c and apps/buggy_app_original). Do not treat the corrected build as the original buggy executable.

4. Prerequisites
4.1 Ubuntu host tools
The following packages cover the typical workflow:
sudo apt update
sudo apt install -y \
  build-essential gcc-arm-linux-gnueabihf gdb-multiarch \
  qemu-system-arm valgrind strace linux-perf rt-tests \
  python3 python3-matplotlib
Package names and availability may vary with the Ubuntu release. The ARM toolchain shown targets 32-bit ARM hard-float (armhf); use a compiler that matches the actual target ABI.
4.2 Additional ARM target requirements
- A compatible ARM Linux kernel image (zImage or the format required by QEMU).
- A matching Device Tree Blob (.dtb) when required by the selected machine.
- A root filesystem/initramfs containing a shell, required libraries, and the target application.
- gdbserver on the ARM target for remote debugging.
- A working communication channel between host GDB and target gdbserver (network forwarding or a serial transport).
These kernel/DTB paths and the original remote-debug transport are not included in the available LAB03 evidence; configure them using the files and settings from your earlier Embedded Linux environment.
5. Build Instructions
Run the following commands from inside lab03/.
5.1 Build Ubuntu applications
gcc -O0 -g -Wall -Wextra apps/leaky_app.c -o apps/leaky_app
gcc -O0 -g -Wall -Wextra apps/leaky_app_fixed.c -o apps/leaky_app_fixed
gcc -O0 -g -Wall -Wextra apps/perf_target.c -o apps/perf_target
If the profiling source uses mathematical library functions, append -lm to its compilation command.
-g preserves debugging symbols; -O0 avoids optimizations that can complicate source-level inspection. Compiler warnings should be reviewed, especially for the intentionally defective examples.
5.2 Build the corrected ARM application
arm-linux-gnueabihf-gcc -O0 -g -Wall -Wextra \
  apps/buggy_app.c -o apps/buggy_app
file apps/buggy_app
ABI note: The target may need compatible dynamic libraries. If the original LAB used a statically linked binary, reproduce that build configuration and verify the resulting executable with file and readelf before booting.
5.3 Preserve and build the original defective ARM version
Once the actual original buggy source has been recovered:
arm-linux-gnueabihf-gcc -O0 -g \
  apps/buggy_app_original.c -o apps/buggy_app_original
The original source file is not yet part of the verified submission. Do not create a synthetic replacement and present it as the source used in the recorded GDB session.
5.4 Executable permissions
chmod +x apps/buggy_app apps/leaky_app apps/leaky_app_fixed
ls -l apps/
Git tracks the executable bit. When files are uploaded through the GitHub website, verify that executable permissions remain correct after a fresh clone; if necessary, commit permission changes from a local Git working tree.
6. Boot the ARM System in QEMU
The repository includes the generated initramfs:
output/initramfs_lab3.cpio.gz
An illustrative command for a vexpress-a9 target is shown below. It is not a verified copy of the original LAB03 boot command:
qemu-system-arm \
  -M vexpress-a9 -cpu cortex-a9 -m 512M \
  -nographic \
  -kernel /ABSOLUTE/PATH/TO/zImage \
  -dtb /ABSOLUTE/PATH/TO/vexpress-v2p-ca9.dtb \
  -initrd output/initramfs_lab3.cpio.gz \
  -append 'console=ttyAMA0 rdinit=/sbin/init'
Replace the kernel and DTB placeholders with the actual compatible images, and preserve any additional QEMU flags used in the original environment. If the initramfs expects a different init process, update rdinit accordingly.
Terminal convention:
- Terminal 1 — QEMU/ARM target: run ARM-side commands at the / # prompt.
- Terminal 2 — Ubuntu host: compile, launch host tools, and connect gdb-multiarch.
7. Part A — GDB Remote Debugging
7.1 Workflow
1. Boot the ARM image in QEMU.
2. Start gdbserver on the target using the actual connection transport.
3. Open the matching unstripped original ARM executable in gdb-multiarch on Ubuntu.
4. Connect to the remote target, set breakpoints, and inspect the affected expressions.
5. Save the full debugging transcript and compare the results with the corrected program.
Example for a target with reachable TCP networking (only if TCP networking and port forwarding have been configured):
On the ARM target:
gdbserver :1234 /path/to/buggy_app_original
On the Ubuntu host:
gdb-multiarch apps/buggy_app_original
Inside GDB:
target remote localhost:1234
break main
continue
backtrace
info registers
info locals
print i
print len
print sum
If your setup uses a serial transport, substitute its actual target remote endpoint; the TCP example cannot be used without a corresponding connection.
7.2 Defects identified
Defect	Original behavior	Root cause	Correction
Out-of-bounds array access	Loop reaches i = 5 with len = 5; observed sum = 150 before the invalid access	Loop condition i <= len accesses one element past the end	Change to i < len
Use-after-free	Pointer initially refers to Hello Embedded Linux, but is accessed after free	A function returns a pointer to freed heap storage	Return a valid live allocation and free it in the caller
Signed integer overflow	100000 * 100000 prints 1410065408 in the observed run	The mathematical product exceeds the range of a 32-bit int; signed overflow is undefined behavior in C	Use a sufficiently wide integer type such as long long and print with %lld


Selected recorded GDB values:
Array: i = 5, len = 5, sum = 150
Invalid element read observed: data[i] = 1457003264
Before free: buf = 0x422010, "Hello Embedded Linux"
After free:  buf = 0x422010, contents no longer valid
Overflow: a = 100000, b = 100000, result = 1410065408
These are observations from a particular run. Out-of-bounds reads, use-after-free, and signed overflow are not guaranteed to reproduce identical values on every build or system.
7.3 Program output comparison
Original defective run	Corrected run
Sum = -1972667754	Sum = 150
Corrupted message output	Message: Hello Embedded Linux
calculate(100000, 100000) = 1410065408	calculate(100000, 100000) = 10000000000


Evidence: [`logs/gdb_session.txt`](logs/gdb_session.txt)
The recorded GDB transcript is the authoritative source for actual commands and stack/register observations. Do not claim that every command in the example above was captured unless it appears in that log.
8. Part B — Valgrind Memory Analysis
8.1 Analyze the original application
valgrind --leak-check=full --show-leak-kinds=all \
  ./apps/leaky_app 2>&1 | tee logs/valgrind_report.txt
8.2 Analyze the corrected application
valgrind --leak-check=full --show-leak-kinds=all \
  ./apps/leaky_app_fixed 2>&1 | tee logs/valgrind_fixed.txt
8.3 Recorded results
Metric	Before correction	After correction
Allocations	3	3
Frees	2	3
Total allocated	4,216 bytes	4,216 bytes
Heap in use at exit	100 bytes	0 bytes
Definitely lost	100 bytes in 1 block	0 bytes
Indirectly lost	0 bytes	0 bytes
Valgrind errors	3 errors / 3 contexts	0 errors / 0 contexts


The original leak report identifies a 100-byte allocation from malloc in main at leaky_app.c:9 that is not released. The corrected program releases all allocated blocks before exit.
Evidence: [`logs/valgrind_report.txt`](logs/valgrind_report.txt) and [`logs/valgrind_fixed.txt`](logs/valgrind_fixed.txt).
Scope of evidence: The available Valgrind summary confirms one definitely lost allocation, not two distinct leaked allocations. A requirement to analyze two leaks needs additional genuine evidence; it should not be satisfied by inventing a second leak.
8.4 ARM process memory (VmRSS)
To record resident memory for a long-running ARM process, inspect its /proc/<PID>/status while the process is alive:
pidof leaky_app_arm
cat /proc/<PID>/status | grep -E '^(Name|VmRSS|VmSize):'
Replace <PID> with the real process ID. To compare before and after correction, build/run comparable original and fixed ARM applications, sample them under equivalent workloads, and record the observed values. No verified ARM VmRSS comparison is currently included in this repository.
9. Part C — System Call and CPU Profiling
9.1 System-call tracing
strace -c ./apps/perf_target 2> logs/strace_summary.txt
Five highest recorded syscall entries by percentage of traced syscall time:
Rank	System call	Time	Calls
1	execve	65.99%	1
2	mmap	11.13%	8
3	mprotect	4.15%	3
4	write	3.95%	4
5	brk	2.43%	3


The traced execution contains 37 syscall calls and 1 syscall error in total. The percentages describe the syscall-tracing summary for this particular run, not the percentage of overall application runtime.
Evidence: [`logs/strace_summary.txt`](logs/strace_summary.txt).
9.2 CPU counters
perf stat ./apps/perf_target 2> logs/perf_stat.txt
Recorded perf measurements:
Counter	Result
Program result	4800000242
Task clock	94.16 ms
Elapsed time	0.095184738 s
User CPU time	0.093199000 s
System CPU time	0.002004000 s
Instructions	2,044,198,217
CPU cycles	400,663,324
Instructions per cycle	~5.1
Branches	102,412,690
Branch misses	4,061
Page faults	63
Context switches	3
CPU migrations	1


The much larger user-mode CPU time compared with system-mode CPU time suggests that this workload is predominantly CPU-bound, rather than dominated by operating-system calls.
9.3 Function-level sampling
perf record -g -- ./apps/perf_target
perf report --stdio > logs/perf_report.txt
The saved report shows main as the dominant hotspot at approximately 99.68% of recorded samples. The available evidence does not establish five distinct meaningful functions with reliable overhead values; consult the raw report for any additional entries instead of fabricating a top-five ranking.
Potential optimization: Inspect the computational loop inside main, reduce redundant work where mathematically valid, and compare optimized and baseline builds using the same workload and measurement conditions.
Evidence: [`logs/perf_stat.txt`](logs/perf_stat.txt) and [`logs/perf_report.txt`](logs/perf_report.txt).
10. Part D — Real-Time Scheduling Latency
10.1 Measurement tool
cyclictest from the rt-tests package measures scheduling wake-up latency. A reproducible run should record the exact command, CPU load, thread priority, interval, number of iterations, and kernel version.
Example command (not verified as the exact original invocation):
sudo cyclictest -p 80 -i 1000 -l 10000 -h 5000 -q \
  > logs/hist_standard.txt
Consult cyclictest --help for options supported by your installed version. Preserve the original log rather than overwriting it when conducting a new run.
10.2 Recorded standard-kernel results
Metric	Observed latency
Minimum	10 µs
Average	76 µs
Maximum	550 µs
Histogram overflows	0


Evidence: [`logs/hist_standard.txt`](logs/hist_standard.txt).

10.3 Interpretation
The average latency describes typical wake-up behavior, while the maximum indicates the largest delay observed in the measured run. For a time-sensitive control loop, rare high-latency events may violate deadlines even when the average latency is low.
For a robot with a strict response-time requirement below 500 µs, the observed 550 µs maximum means that this standard-kernel measurement does not demonstrate compliance. The result does not establish a universal worst-case bound; more runs under representative CPU, I/O, and interrupt loads are required.
10.4 PREEMPT_RT comparison
The PREEMPT_RT patch set reduces non-preemptible sections and makes more kernel activity schedulable with real-time priorities. This can improve scheduling predictability, but a PREEMPT_RT measurement is not present in the submitted evidence. A fair comparison would run the same cyclictest configuration on standard and PREEMPT_RT kernels using identical hardware and workload conditions.
10.5 Reproducing the histogram
The image in logs/latency_histogram.png is the existing experiment artifact. The original plotting script has not been verified as part of the submission. To make histogram generation fully reproducible, add a script such as scripts/plot_latency.py that parses the actual cyclictest histogram records in logs/hist_standard.txt, and document its invocation here. Do not synthesize histogram bins from only the minimum, average, and maximum values.
11. Summary of Results
Experiment	Key finding	Evidence
GDB	Array bounds, use-after-free, and integer overflow defects identified	logs/gdb_session.txt
Valgrind	100 bytes definitely lost before fix; zero bytes after fix	logs/valgrind_*.txt
strace	execve accounts for 65.99% of traced syscall time	logs/strace_summary.txt
perf	Workload dominated by user-space computation in main	logs/perf_*.txt
cyclictest	10 µs min, 76 µs avg, 550 µs max	logs/hist_standard.txt
Visualization	Recorded standard-kernel latency histogram	logs/latency_histogram.png


12. Known Limitations and Remaining Deliverables
The current artifacts support the results described above, but the following items are still needed for full experimental reproducibility and report completeness:
1. Add the original defective ARM source and its matching binary.
2. Record the exact kernel/DTB paths and the QEMU boot command used during LAB03.
3. Record the exact gdbserver transport and corresponding GDB connection command.
4. Verify that the saved GDB log contains all required backtrace, info registers, and variable inspections.
5. Collect genuine ARM VmRSS measurements before and after the memory fix.
6. Provide additional evidence if the assignment specifically requires analysis of two distinct leaks or five separate perf functions.
7. Add the actual histogram-generation script or exact command.
8. Add the final PDF report and answers to the assignment's Section VI analysis questions.
9. Record host kernel and installed tool versions for the report.
These limitations are stated explicitly to distinguish completed measurements from unverified or missing deliverables.
13. Environment Information
Run these commands on the Ubuntu host and include their output in the final report:
uname -a
gdb-multiarch --version | head -n 1
valgrind --version
perf --version
cyclictest --version 2>&1 | head -n 2
qemu-system-arm --version | head -n 1
gcc --version | head -n 1
arm-linux-gnueabihf-gcc --version | head -n 1
date '+%Y-%m-%d %H:%M:%S %Z'
14. Report and Submission
The intended report path is:
lab03/baocao/SE201573_Lab03_BaoCao.pdf
The final report should include:
- Student information and exact software versions.
- Three GDB bug analyses with original logs and technical explanations.
- Valgrind before/after summaries and allocation stack traces.
- ARM VmRSS comparison when collected.
- strace and perf results with bottleneck discussion.
- Cyclictest measurements, histogram, and real-time kernel assessment.
- Responses to the laboratory's Section VI questions.
- Personal reflections and at least two challenges with their solutions.
15. Reproducibility Notes
Tool outputs depend on compiler options, CPU architecture, kernel configuration, background system load, and whether an application runs on the Ubuntu host or the emulated ARM target. Use the raw files in logs/ as the authoritative measurements and record any changes in configuration before comparing runs.
16. Author
Dinh Quang Ngoc
Student ID: SE201573
Laboratory: LAB03 — Debugging and Performance Analysis
