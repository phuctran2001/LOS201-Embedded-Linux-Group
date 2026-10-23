# Assignment 01 — Writing Linux Device Drivers

**Course:** LOS201 Embedded Linux Systems

**Contributor:** Nguyễn Huy Hoàng (SE203347)

**Target:** ARMv7-A, QEMU `vexpress-a9`, Linux 5.15.0

This assignment traces a userspace request through the Virtual File System (VFS) to a Linux kernel driver. Two independent modules implement an ioctl-controlled character device and a misc counter device. The report compares character, block, and network drivers and answers four conceptual questions. Start with the two examples below, then follow the build steps and inspect the matching logs.

## Contents

- [Deliverables](#deliverables)
- [System architecture](#system-architecture)
- [Task 1: ioctl character device](#task-1-ioctl-character-device)
- [Task 2: misc counter device](#task-2-misc-counter-device)
- [Task 3: driver classes and design choices](#task-3-driver-classes-and-design-choices)
- [Task 4: conceptual analysis](#task-4-conceptual-analysis)
- [Build and run](#build-and-run)
- [Recorded verification](#recorded-verification)
- [Submission and limitations](#submission-and-limitations)
- [Technical references](#technical-references)

## Deliverables

| Task | Deliverable | Location |
|---|---|---|
| 1 | Character driver, shared ioctl ABI, ARM test program | [`task1_ioctl/`](task1_ioctl/) |
| 2 | Misc counter driver, procfs statistics, ARM partial-read test | [`task2_misc/`](task2_misc/) |
| 3 | Comparison of character, block, and network drivers; choice of registration for these modules | [Report, section 6](bao_cao/SE203347_NguyenHuyHoang_Asgn01_BaoCao.pdf) |
| 4 | Answers on module references, user memory, MTD, and VFS offsets | [Report, section 7](bao_cao/SE203347_NguyenHuyHoang_Asgn01_BaoCao.pdf) |

The repository includes the C sources, Makefiles, test scripts, built ARM modules and test executables, original QEMU transcript, extracted test logs, full `dmesg` capture, PDF report, and submission archive. Tasks 3 and 4 are written analyses, not additional drivers.

| Environment | Role | Typical files or commands |
|---|---|---|
| Ubuntu host (x86-64) | Cross-compile the ARM modules and programs; package the root filesystem; launch QEMU | `make`, `cpio`, `qemu-system-arm` |
| ARM guest (QEMU) | Load and test the modules against the running kernel | `insmod`, `/dev/asgn1`, `/dev/counter`, `cat /proc/counter_info` |
| Repository | Preserve source, recorded observations, and the written report | `task1_ioctl/`, `task2_misc/`, `logs/`, `bao_cao/` |

## System architecture

The two device nodes do not share state. In both cases the call path is: a program invokes a system call; VFS finds the open file's `file_operations`; the appropriate callback runs in the module; its return value and any copied data go back to the program.

### Complete host-to-driver path

| Step | Component | Action and result |
|---|---|---|
| 1 | Ubuntu host | Cross-compile two kernel modules and two ARM test programs against the kernel used by QEMU. |
| 2 | Initramfs and QEMU | Package the modules and programs, then boot the matching `zImage`, DTB, and ARM filesystem. |
| 3 | ARM test scripts | Load one module with `insmod`, prepare its device node, and run its userspace test. |
| 4 | System calls and VFS | `open()` selects the driver's operations using the device number; `read()`, `write()`, or `ioctl()` calls the relevant callback. |
| 5 | Kernel module | Check arguments, protect shared state, copy data across the userspace boundary, and return bytes, an ioctl result, EOF, or an error. |
| 6 | Evidence and cleanup | The test checks the result and captures log messages. Once file descriptors have closed, the script unloads the module with `rmmod`. |

Reading `/proc/counter_info` is a separate VFS path into Task 2's `seq_file` callbacks. It reports the counter's current state without opening `/dev/counter` or incrementing its value.

| Boundary | Information crossing it | Mechanism in this assignment |
|---|---|---|
| Host → ARM guest | Kernel image, device tree, initramfs, `.ko` and ARM executables | QEMU boot arguments and packaged filesystem |
| ARM process → kernel | Device path, read/write arguments, ioctl command and argument | System calls, VFS, device number, `file_operations` |
| Process memory ↔ driver | Input bytes, output bytes, ioctl statistics | `copy_from_user()`, `copy_to_user()`; `simple_read_from_buffer()` in Task 2 |
| Driver state → diagnostic reader | Counter and operation totals | `/proc/counter_info` and `seq_file` |

**Task 1 path:** `SET_MODE(0)` selects prefix mode; a later write of `abc` stores `[KERNEL]abc`, and a read returns 11 bytes. See the [Task 1 walkthrough](#follow-one-task-1-test).

**Task 2 path:** Opening `/dev/counter` creates per-file state. With a shared counter of 100, the first successful data read snapshots `100\n` and increments the shared value to 101. Partial reads finish the saved line before EOF; procfs then reports 101. See the [Task 2 walkthrough](#follow-one-task-2-test).

`open()` connects a file descriptor to its driver's callbacks. The kernel uses `copy_from_user()` and `copy_to_user()` for application buffers. A reader advances `*ppos` and returns zero at EOF; otherwise a program such as `cat` could keep reading forever. The Task 2 test reads procfs after its device read completes.

## Task 1: ioctl character device

[`asgn1_driver.c`](task1_ioctl/asgn1_driver.c) registers major **241** and exposes `/dev/asgn1`. It has a 1024-byte array, with one byte reserved for a terminating NUL. Each successful `write()` replaces the previous data. `read()` starts at the current file offset and returns zero at EOF.

| Interface | Behavior |
|---|---|
| `open` / `release` | Count successful opens; close the file instance |
| `write` | Replace stored data; reject input that exceeds capacity |
| `read` | Return bytes and advance `*ppos`; count successful nonempty calls |
| `SET_MODE(0)` | Prefix subsequent writes with `[KERNEL]` |
| `SET_MODE(1)` | Store input unchanged; this is the initial mode |
| `GET_STATS` | Return open, write, and read counts, buffer length, and last input size |
| `RESET_BUFFER` | Clear the buffer and read/write statistics; retain open count and mode |
| `GET_VERSION` | Return the fixed driver version string |

The four ioctl commands and `struct asgn1_stats` are defined in the [shared header](task1_ioctl/asgn1_ioctl.h), used by the module and the [ARM test application](task1_ioctl/asgn1_test.c). An invalid mode returns `-EINVAL`; an unknown command returns `-ENOTTY`. A mutex protects the buffer, mode, and statistics. `last_write_size` counts input bytes, excluding the prefix.

### Follow one Task 1 test

1. `test_asgn1.sh` loads `asgn1_driver.ko`, ensures `/dev/asgn1` exists with major 241 and minor 0, and verifies the initial echo mode by writing and reading `hello`.
2. `asgn1_test` sends `SET_MODE(0)`. The ioctl callback checks that the value is valid and changes the protected `mode` field.
3. Writing three input bytes, `abc`, replaces the buffer with eight prefix bytes plus three input bytes: `[KERNEL]abc`. A new file descriptor reads all **11** bytes; the next read at EOF returns zero.
4. `GET_STATS` copies a five-field snapshot to the ARM test program. The [recorded output](logs/test_asgn1_output.txt) shows `write_count=2`, `read_count=2`, `buffer_len=11`, and `last_write_size=3` at this checkpoint. These totals include the earlier `hello` test.
5. The program restores mode 1, verifies that writing `xyz` yields `xyz`, then calls `RESET_BUFFER`. The [later snapshot](logs/test_asgn1_output.txt) has `buffer_len=0`, `write_count=0`, `read_count=0`, and `last_write_size=0`; `open_count=8` remains. The script unloads the module after the checks.

Each new `open()` has its own file offset, while the driver's buffer and statistics are shared. The mutex protects those shared fields. `read_count` counts successful **nonempty** read calls; a read returning EOF does not increase it. `RESET_BUFFER` does not reset the mode, so its post-reset value depends on the most recent `SET_MODE`.

**Interface boundary:** This Task 1 driver has no sysfs attribute. `GET_STATS` provides its statistics; `/proc/counter_info` belongs to Task 2.

## Task 2: misc counter device

[`counter_driver.c`](task2_misc/counter_driver.c) registers `/dev/counter` using the miscdevice framework. Linux assigns a **dynamic minor** under misc major **10**; a minor number observed in one boot is not a fixed interface.

| Action | Behavior |
|---|---|
| First data read of an open file | Return the current integer and newline; increment the counter once |
| Subsequent short reads of that file | Continue the same snapshot without another increment |
| Read past the end of the line | Return zero (EOF) |
| Write a nonnegative decimal integer | Set the counter to that value |
| Write negative or nonnumeric input | Return `-EINVAL` |
| Read `/proc/counter_info` | Show `counter`, `total_reads`, and `total_writes` |

A per-open snapshot lives in `file->private_data`. `simple_read_from_buffer()` advances `*ppos`, so short reads do not create multiple increments. An `atomic_t` stores the counter; a mutex guards the compound read/increment, write, and statistics operations. Procfs uses `seq_file` for its diagnostic view. `total_reads` counts the first successful data read of each open file, not every fragment.

### Follow one Task 2 test

| Event in the test script | Value returned or observed | Counter after event | Why |
|---|---:|---:|---|
| Load module, inspect procfs | `counter: 0` | 0 | Module state starts at zero |
| First `cat /dev/counter` | `0` | 1 | First read snapshots zero, then increments |
| Second `cat /dev/counter` | `1` | 2 | `cat` opens a new file, so it takes a new snapshot |
| Write `100`, then read | `100` | 101 | A valid write sets the value; a later read increments it |
| Write `0`, then read | `0` | 1 | Writing zero resets the value, not the read/write totals |
| Write `100`, read one byte at a time | `100\n` | 101 | All byte reads consume one per-open snapshot |

The [Task 2 log](logs/test_counter_output.txt) records the values and `/proc/counter_info` counters. Negative and nonnumeric writes do not increase `total_writes`. The final partial-read test lives in [`counter_test.c`](task2_misc/counter_test.c); it checks that the value after reading fragments is 101. Unlike Task 1, this device has no ioctl command or sysfs attribute.

## Task 3: driver classes and design choices

The [PDF report](bao_cao/SE203347_NguyenHuyHoang_Asgn01_BaoCao.pdf) contains the complete written comparison in section 6.

The assignment compares **character, block, and network** drivers. A character interface transfers a byte stream or handles device commands; a block interface works with addressable sectors and block requests; a network interface transmits and receives packets. Both modules here are character devices. `misc_register()` simplifies registration of the counter under major 10, while `register_chrdev()` gives Task 1 a fixed major 241 with class/device creation and corresponding cleanup. Raw NAND is discussed separately under MTD; eMMC presents sectors through the block layer.

## Task 4: conceptual analysis

The four questions below are answered in [report section 7](bao_cao/SE203347_NguyenHuyHoang_Asgn01_BaoCao.pdf). Each answer links a kernel rule to code or a recorded test in this repository.

### 4.1. Module references and safe unload

**Why can `rmmod` fail while a device is open?** A file's `file_operations` contains function pointers into the module. Both drivers set `.owner = THIS_MODULE`; normal file operations hold the module while an open file may still call those functions. A normal unload must wait until no such references remain. A driver can also manage extra references explicitly with `module_get()` and `module_put()`, but neither module does so in its ordinary data path.

**In this assignment:** The shell scripts open the device through their commands and test programs, let the programs close it, and only then call `rmmod`. Task 1's exit path destroys the device and class and unregisters major 241. Task 2's exit path removes the proc entry and unregisters the misc device. Their respective [Task 1](logs/test_asgn1_output.txt) and [Task 2](logs/test_counter_output.txt) logs end with unload messages. A forced unload is not a way to fix an open file: it can leave a live caller pointing at unloaded code, and it depends on kernel configuration.

### 4.2. Why ioctl cannot write directly to a userspace pointer

**What is the address boundary?** The `arg` passed to `unlocked_ioctl` is a process virtual address, not a trusted kernel pointer. The process's page tables and access permissions determine whether its memory can be reached; a bad or unmapped address must not be treated as an ordinary kernel allocation. The copying helpers handle the user access and report bytes that could not be copied.

**In this assignment:** `GET_STATS` builds a snapshot while holding `data_lock`, releases the lock, then calls `copy_to_user()` to deliver the five fields. `GET_VERSION` also copies a fixed-size array to userspace. `SET_MODE` takes an integer in the opposite direction with `copy_from_user()` and rejects any value other than 0 or 1. A nonzero copy result becomes `-EFAULT`; an unsupported ioctl becomes `-ENOTTY`. These are different failure causes, so they must not be collapsed into a generic error. The shared [ABI header](task1_ioctl/asgn1_ioctl.h) ensures the ARM application and driver agree on the command numbers and structure layout.

### 4.3. Raw flash MTD versus the block layer

**Why do they expose different interfaces?** A block device presents addressable sectors, which the block layer can queue and cache for filesystems. Raw flash has constraints such as erase blocks and, for NAND, bad blocks and sometimes out-of-band metadata. Treating raw NAND as an ordinary sector device would hide operations the software must manage. In contrast, an eMMC controller normally presents sectors to Linux and handles its internal flash management.

**In this assignment:** `/dev/asgn1` and `/dev/counter` are character interfaces implemented in software. Neither driver manages a flash chip, sector queue, packet stream, hardware interrupt, or DMA engine. The MTD question tests the choice of **kernel subsystem** for a hypothetical device; it does not claim that the two submitted modules implement MTD. The [Task 3 comparison](#task-3-driver-classes-and-design-choices) provides the broader context.

### 4.4. File offset, repeated reads, and EOF

**Why does `cat` stop?** A file descriptor carries an offset. `cat` repeatedly calls `read()`: a positive result supplies bytes, zero signals EOF, and a negative result signals an error. If a driver returned the same text forever without consuming the file offset, `cat` could loop and print indefinitely.

**In Task 1:** `asgn1_read()` calculates the bytes available after `*ppos`, calls `copy_to_user()`, advances `*ppos` on success, and returns zero once the offset reaches `buffer_len`. A new `open()` starts with a fresh offset; it can read the same shared buffer again.

**In Task 2:** `counter_open()` allocates state for one file instance. The first successful data read formats the current value, returns the first bytes, increments the shared counter once, and marks the snapshot prepared. Further one-byte reads use the same text and advance `*ppos`; EOF does not increment again. The [partial-read test](task2_misc/counter_test.c) and its [log](logs/test_counter_output.txt) show `100\n` returned in fragments while the shared counter ends at 101. A later `open()` creates a new snapshot and may observe a different value.

## Build and run

### 1. Build on the host

Use an ARM cross-compiler and the **built** Linux 5.15 tree that produced the guest kernel. The Makefiles default to `$HOME/embedded_lab1/kernel/linux-5.15`. Run all host blocks below from this `assignment01/` directory in **the same terminal**, since the later blocks reuse `KDIR`.

Set the kernel tree path and confirm that it has been configured and built:

```sh
KDIR="$HOME/embedded_lab1/kernel/linux-5.15"
test -f "$KDIR/.config" && test -f "$KDIR/include/generated/autoconf.h"
```

Build Task 1 and Task 2 separately. If either command fails, resolve its error before continuing:

```sh
make -C task1_ioctl KDIR="$KDIR"
make -C task2_misc KDIR="$KDIR"
```

Inspect the output architecture and each module's version metadata:

```sh
file task1_ioctl/asgn1_driver.ko task1_ioctl/asgn1_test \
     task2_misc/counter_driver.ko task2_misc/counter_test
modinfo task1_ioctl/asgn1_driver.ko | grep vermagic
modinfo task2_misc/counter_driver.ko | grep vermagic
```

All four outputs must be ARM binaries. `file` checks their architecture; `modinfo` checks module metadata. Compare each module's `vermagic` with the guest's `uname -r` and kernel settings. The Makefiles contain `-march=armv7-a` after an ARM instruction mismatch found during the original build. `make clean` removes the modules and test executables.

### 2. Prepare an ARM initramfs

Copy a working LAB01/LAB02 ARM root filesystem. Replace `ROOTFS` with the directory that exists on your host; the example path is machine-specific. Confirm it exists **before** the copy. If `rootfs_assignment/` already exists, inspect it rather than copying another directory inside it.

```sh
ROOTFS="$HOME/embedded_lab_redo/rootfs/initramfs"
test -d "$ROOTFS"
cp -a "$ROOTFS" ./rootfs_assignment
```

Calculate the target kernel release from the same build tree and prepare the destination directories. The test scripts expect modules in `/lib/modules/$(uname -r)/` and the ARM programs and scripts in `/usr/bin/`:

```sh
KVER="$(make -s -C "$KDIR" kernelrelease ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-)"
mkdir -p "rootfs_assignment/lib/modules/$KVER" rootfs_assignment/usr/bin output
```

Copy the two modules, two test programs, and two shell scripts into the ARM rootfs:

```sh
cp task1_ioctl/asgn1_driver.ko task2_misc/counter_driver.ko \
  "rootfs_assignment/lib/modules/$KVER/"
cp task1_ioctl/asgn1_test task1_ioctl/test_asgn1.sh \
   task2_misc/counter_test task2_misc/test_counter.sh rootfs_assignment/usr/bin/
chmod +x rootfs_assignment/usr/bin/asgn1_test \
         rootfs_assignment/usr/bin/test_asgn1.sh \
         rootfs_assignment/usr/bin/counter_test \
         rootfs_assignment/usr/bin/test_counter.sh
```

Finally, create the initramfs from the copied directory:

```sh
(cd rootfs_assignment && find . -print | cpio -H newc -o | gzip > ../output/initramfs_assignment.cpio.gz)
```

`rootfs_assignment/` and `output/initramfs_assignment.cpio.gz` are **not currently tracked**. Ensure the copy has a working `/sbin/init` and startup configuration. In the recorded test session, `/etc/init.d/rcS` was missing, so `/proc`, `/sys`, and `/dev` were mounted manually. The report describes a separate later boot after repairing startup; it does not present that boot as part of the test transcript. The copied rootfs must contain the ARM runtime libraries required by its executables, unless those executables were linked statically.

### 3. Boot QEMU and verify in the guest

Use a `zImage` and DTB from the **same kernel build** as the modules:

```sh
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel "$HOME/embedded_lab1/output/zImage" \
  -dtb "$HOME/embedded_lab1/output/vexpress-v2p-ca9.dtb" \
  -initrd "$PWD/output/initramfs_assignment.cpio.gz" \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
```

Inside QEMU, first check the kernel release and whether `/proc` is mounted. If the old rootfs lacks startup mounts, mount `/proc` before running the scripts; they use `/proc/modules` and `/proc/misc`. Mount `/sys` and `/dev` as required by that rootfs. Then run as root:

```sh
uname -r
mount | grep ' on /proc ' || mount -t proc proc /proc
test_asgn1.sh
test_counter.sh
```

Each script loads the module, creates its device node if needed, exercises normal and error paths, then unloads it. The counter script prints `/proc/counter_info` **before** unloading; `cat /proc/counter_info` after the script ends would fail because the proc entry has been removed. For an additional manual inspection, load `counter_driver.ko` again, read procfs, and unload it when finished. With `-nographic`, exit QEMU by pressing `Ctrl+A`, then `X`.

## Recorded verification

| Check | Recorded result | Evidence |
|---|---|---|
| Task 1 prefix | Writing `abc` in mode 0 returned `[KERNEL]abc`, length 11 | [Task 1 log](logs/test_asgn1_output.txt) |
| Task 1 reset | Buffer and read/write statistics became zero; open count stayed at 8 | [Task 1 log](logs/test_asgn1_output.txt) |
| Task 1 errors | Invalid mode produced `EINVAL`; unknown ioctl produced `ENOTTY` | [Task 1 log](logs/test_asgn1_output.txt) |
| Task 2 sequence | Consecutive opens returned `0`, then `1`; writing `100` made the next read return `100` | [Task 2 log](logs/test_counter_output.txt) |
| Task 2 partial read | Reading `100\n` in fragments advanced the counter only once, to 101 | [Task 2 log](logs/test_counter_output.txt) |
| Task 2 errors | Negative and nonnumeric writes were rejected | [Task 2 log](logs/test_counter_output.txt) |
| Lifecycle | Both scripts printed `SCRIPT PASSED`; module load and unload were recorded | [QEMU transcript](logs/qemu_session.txt), [full dmesg](logs/dmesg_full.txt) |

The shorter test logs were extracted from the same [QEMU transcript](logs/qemu_session.txt), not from independent reruns. The [report](bao_cao/SE203347_NguyenHuyHoang_Asgn01_BaoCao.pdf) discusses the original build and rootfs fixes and includes the Task 3 and Task 4 answers. There are no recorded simultaneous-access, performance, or physical-hardware measurements.

## Submission and limitations

The tracked [submission archive](SE203347_NguyenHuyHoang_Asgn01.tar.gz) is a packaged artifact. [`finalize_submission.sh`](finalize_submission.sh) checks required files and pass markers and creates a fresh archive; inspect its contents with `tar -tzf` before submission. The [PDF report](bao_cao/SE203347_NguyenHuyHoang_Asgn01_BaoCao.pdf) is available separately.

The archive does not include a ready-to-boot assignment initramfs. Recreate one from a compatible ARM root filesystem as described above. Passing tests demonstrate the behavior of this specific QEMU build and session, not untested workloads.

## Technical references

- [Linux kernel: ioctl-based interfaces](https://docs.kernel.org/driver-api/ioctl.html) — command encoding and userspace ABI design.
- [Linux kernel: miscellaneous devices](https://docs.kernel.org/driver-api/misc_devices.html) — misc device registration and minor allocation.
- [Linux kernel: kernel hacking guide](https://docs.kernel.org/kernel-hacking/hacking.html) — module ownership and safe copying across the userspace boundary.
- [Linux kernel: UBIFS and MTD](https://docs.kernel.org/filesystems/ubifs.html) — the distinction between raw-flash MTD and block devices.

[Back to the group repository](../README.md)
