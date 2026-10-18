# LAB02 — Character Device Driver and Filesystem Management

**Course:** LOS201 — Embedded Linux Systems, FPT University  
**Contributor:** Võ Quốc Thịnh — SE200595  
**Target platform:** ARMv7-A / Cortex-A9, QEMU `vexpress-a9`, Linux 5.15.0  
**Development host:** Ubuntu Linux

## Overview

LAB02 builds on the ARM Linux environment from LAB01. It implements a loadable character device driver, exposes driver statistics through procfs and sysfs, experiments with simulated NAND flash and the JFFS2 filesystem, and integrates automatic driver loading into the BusyBox startup sequence.

The repository includes the driver source and compiled ARM module, selected root filesystem files, test scripts, recorded execution logs, and a [PDF report](report/SE200595_Lab02_BaoCao.pdf). **It does not include a complete LAB02 root filesystem, the Linux kernel source tree, or ready-to-boot LAB02 kernel/DTB/initramfs images.** The build and boot instructions below require the locally prepared LAB01/LAB02 workspaces.

## Contents

- [Platform and prerequisites](#platform-and-prerequisites)
- [Repository materials](#repository-materials)
- [Driver architecture and interfaces](#driver-architecture-and-interfaces)
- [Build the kernel module](#build-the-kernel-module)
- [Prepare BusyBox initramfs and boot QEMU](#prepare-busybox-initramfs-and-boot-qemu)
- [Character device verification](#character-device-verification)
- [procfs and sysfs verification](#procfs-and-sysfs-verification)
- [MTD and JFFS2 experiment](#mtd-and-jffs2-experiment)
- [Recorded results and evidence](#recorded-results-and-evidence)
- [Troubleshooting and limitations](#troubleshooting-and-limitations)
- [References](#references)

## Platform and prerequisites

| Component | Configuration recorded in this lab |
|---|---|
| Host | Ubuntu Linux (cross-compilation and host-side NAND simulation) |
| Guest architecture / CPU | ARMv7-A / Cortex-A9 |
| Emulator | `qemu-system-arm`, machine `vexpress-a9` |
| Guest Linux kernel | 5.15.0 |
| Cross-compiler prefix | `arm-linux-gnueabihf-` |
| Guest userspace | BusyBox and initramfs based on LAB01 |
| QEMU resources | 512 MiB RAM, two virtual CPUs in the documented boot command |
| Boot arguments | `console=ttyAMA0,115200 rdinit=/sbin/init mem=512M` |
| Driver module | `lab2_driver.ko` (ARM, external kernel module) |
| Character device | `/dev/lab2`; major `240`, minor `0` in the recorded run |
| Internal buffer | 1024 bytes |
| procfs | `/proc/lab2_info` |
| sysfs | `/sys/class/lab2_class/lab2/` |
| Host-side NAND experiment | `nandsim`, 32 MiB, 16 KiB eraseblock, JFFS2 |

**Host versus guest:** Run cross-compilation, QEMU, and the NAND simulator on **Ubuntu**. Run the `/dev/lab2`, procfs, sysfs, and test-script commands inside the **ARM QEMU guest**. The NAND/JFFS2 experiment documented here was performed on Ubuntu rather than inside QEMU.

The original LAB01 build workspace used by this LAB02 workflow is `~/embedded_lab1`. The LAB02 workspace is `~/embedded_lab2`. If your local layout differs, adjust the paths before running any commands.

On an Ubuntu host, the main userspace tools can be installed with:

```bash
sudo apt update
sudo apt install build-essential gcc-arm-linux-gnueabihf \
  qemu-system-arm cpio gzip mtd-utils
```

The host kernel must separately provide the `nandsim`, `mtdblock`, and JFFS2 functionality needed for the MTD experiment. Building the ARM kernel and BusyBox themselves is part of the LAB01 prerequisite workflow, not accomplished by installing these packages.

## Repository materials

| Repository path | Purpose |
|---|---|
| [`driver/lab2_driver.c`](driver/lab2_driver.c) | Linux character device driver implementation |
| [`driver/Makefile`](driver/Makefile) | Kbuild rules for compiling the ARM module |
| [`driver/lab2_driver.ko`](driver/lab2_driver.ko) | Previously compiled ARM module; kernel compatibility still must be checked |
| [`driver_architecture.png`](driver_architecture.png) | Driver architecture diagram shown in this README |
| [`rootfs/etc/inittab`](rootfs/etc/inittab) | BusyBox init actions |
| [`rootfs/etc/init.d/rcS`](rootfs/etc/init.d/rcS) | Startup mounts, hostname, and automatic module insertion |
| [`rootfs/usr/bin/test_driver.sh`](rootfs/usr/bin/test_driver.sh) | Character device read/write test |
| [`rootfs/usr/bin/test_procfs.sh`](rootfs/usr/bin/test_procfs.sh) | procfs and sysfs state test |
| [`evidence/`](evidence/) | Original execution transcripts |
| [`evidence/clean/`](evidence/clean/) | Readable excerpts formatted from the original logs |
| [`report/SE200595_Lab02_BaoCao.pdf`](report/SE200595_Lab02_BaoCao.pdf) | LAB02 PDF document in this submission |

The checked-in `rootfs/` directory contains **only selected files**. BusyBox binaries, applet links, `/dev` nodes, and other runtime directories must be provided by an existing LAB01-derived root filesystem. The `evidence/clean/` files were prepared on the `lab2` branch; their visibility on `main` depends on Pull Request merging.

## Driver architecture and interfaces

![LAB02 Character Device Driver Architecture and Interfaces](driver_architecture.png)

*Figure 1. LAB02 Character Device Driver Architecture and Interfaces.*

The diagram is a simplified logical view. Userspace programs access three filesystem interfaces: the character device (`/dev/lab2`), procfs (`/proc/lab2_info`), and sysfs (`/sys/class/lab2_class/lab2/`). Their handlers execute in kernel space and access driver-managed state. All three interfaces are served through the Linux VFS; the separated branches emphasize their different roles rather than distinct kernel subsystems outside VFS.

The character device accepts userspace read/write operations via `/dev/lab2`. The procfs entry presents a combined statistics view, while sysfs presents individual read-only attributes. The recorded test uses major `240`, minor `0`, and a 1024-byte internal buffer; those are **observed or configured for this lab**, not universal device numbers.

The driver uses kernel/user copy mechanisms (`copy_from_user()` and `copy_to_user()`) rather than treating userspace addresses as kernel pointers. Driver state, its lifetime, and concurrency handling are defined in the [source file](driver/lab2_driver.c).

## Build the kernel module

The repository's [Makefile](driver/Makefile) sets:

```makefile
KDIR := $(HOME)/embedded_lab1/kernel/linux-5.15
ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-
```

On the **Ubuntu host**, first confirm that the matching Linux 5.15 kernel source/build directory is configured and prepared for external module compilation. The path must correspond to the kernel image actually booted in QEMU.

```bash
cd ~/LOS201-Embedded-Linux-Group/lab02/driver
make
file lab2_driver.ko
modinfo lab2_driver.ko
```

For a different prepared kernel source tree, override `KDIR` from the driver directory:

```bash
make KDIR=/absolute/path/to/prepared/linux-5.15
```

`file` should identify an ARM relocatable module, and `modinfo` should show a `vermagic` compatible with the target kernel. **Do not use the host's x86-64 kernel headers to build a module intended for the ARM guest.** Rebuild the module if the guest kernel release/configuration differs; the included `.ko` is evidence of the recorded build, not a universally loadable binary.

## Prepare BusyBox initramfs and boot QEMU

### 1. Assemble the LAB02 guest files on Ubuntu

This workflow assumes the working LAB01-derived ARM root filesystem already exists at `~/embedded_lab2/rootfs/initramfs` and contains a functioning BusyBox installation, `/sbin/init`, and essential `/dev` nodes. If not, prepare that filesystem using the [LAB01 documentation](../lab01/README.md) first.

```bash
export REPO="$HOME/LOS201-Embedded-Linux-Group"
export LAB1="$HOME/embedded_lab1"
export LAB2="$HOME/embedded_lab2"
export ROOTFS="$LAB2/rootfs/initramfs"

# Check prerequisites before changing the guest tree.
ls -l "$ROOTFS/bin/busybox" "$ROOTFS/sbin/init"
ls -l "$LAB1/output/zImage" "$LAB1/output/vexpress-v2p-ca9.dtb"
ls -l "$REPO/lab02/driver/lab2_driver.ko"

# Copy only the selected LAB02 startup configuration and tests.
cp -a "$REPO/lab02/rootfs/." "$ROOTFS/"

# Install the module at the exact guest path expected by rcS.
install -D -m 0644 "$REPO/lab02/driver/lab2_driver.ko" \
  "$ROOTFS/lib/modules/5.15.0/lab2_driver.ko"
chmod 0755 "$ROOTFS/etc/init.d/rcS" \
  "$ROOTFS/usr/bin/test_driver.sh" \
  "$ROOTFS/usr/bin/test_procfs.sh"
```

Do not continue if any prerequisite check fails. The commands above prepare an existing root filesystem; they do **not** build BusyBox or the Linux kernel from source.

The [saved `rcS`](rootfs/etc/init.d/rcS) mounts procfs, sysfs, tmpfs, and attempts to mount devtmpfs; ensures basic device nodes; sets hostname `embedded-lab2`; then checks for `/lib/modules/5.15.0/lab2_driver.ko` **inside the guest** and inserts it with `insmod`. It also applies permissions to `/dev/lab2`.

### 2. Repackage the initramfs

After confirming `/dev/console` and `/dev/null` exist in the prepared root filesystem, package the guest tree **on Ubuntu**:

```bash
mkdir -p "$LAB2/output"
(
  set -o pipefail
  cd "$ROOTFS" || exit 1
  find . -print0 | cpio --null -o -H newc --owner=0:0 | gzip -9
) > "$LAB2/output/initramfs_lab2.cpio.gz"
gzip -t "$LAB2/output/initramfs_lab2.cpio.gz"
```

The `newc`/gzip archive should contain BusyBox, the LAB02 startup files, test scripts, and the module at `/lib/modules/5.15.0/lab2_driver.ko`. A successful `gzip -t` checks compression integrity but does not by itself prove bootability.

### 3. Boot the ARM guest with QEMU

The documented local workspace uses the following command **on Ubuntu**:

```bash
qemu-system-arm \
  -M vexpress-a9 \
  -cpu cortex-a9 \
  -m 512M \
  -smp 2 \
  -nographic \
  -kernel "$LAB1/output/zImage" \
  -dtb "$LAB1/output/vexpress-v2p-ca9.dtb" \
  -initrd "$LAB2/output/initramfs_lab2.cpio.gz" \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
```

These paths are based on the documented LAB01/LAB02 workspace; they are **not artifacts bundled under `lab02/` on GitHub**. The saved [QEMU boot log](evidence/boot_log.txt) records a successful `Run /sbin/init as init process` followed by `lab2_driver loaded OK` and the `/dev/lab2` message. Press **Ctrl+A**, release the keys, then press **X** to exit QEMU when using `-nographic`.

> **Observed startup nuance:** The boot log contains `mknod: /dev/lab2: File exists`. That means the node was already present; it does not mean `mknod` created a new node in that run. Module insertion nevertheless completed successfully according to the recorded log.

## Character device verification

Run **inside the QEMU guest shell**, not on Ubuntu:

```sh
lsmod
ls -l /dev/lab2
/usr/bin/test_driver.sh
dmesg | grep lab2
```

The `test_driver.sh` output documents a 30-byte write/read round trip with `Hello from userspace, LAB-02!`, followed by repeated writes and reads of `Message_1`, `Message_2`, and `Message_3`. Its recorded result is `=== Test PASSED ===`. Device node `/dev/lab2` showed major/minor `240, 0`.

The guest startup script may have already loaded the module; the test script handles or attempts module loading according to its own logic. If a manual reload is necessary, first verify the module state with `lsmod`. For ARM module metadata (`modinfo`), use the host command shown in the build section.

## procfs and sysfs verification

The LAB02 test script reads `/proc/lab2_info` before and after writing data to `/dev/lab2`, then reads the individual sysfs attributes. Run **inside the QEMU guest**:

```sh
# test_procfs.sh loads the module itself. If rcS already loaded it,
# unload the idle module first for a fresh, comparable test run.
lsmod
rmmod lab2_driver
/usr/bin/test_procfs.sh
cat /proc/lab2_info

SYSFS=/sys/class/lab2_class/lab2
cat "$SYSFS/buffer_len"
cat "$SYSFS/open_count"
cat "$SYSFS/last_data"
```

The recorded **isolated procfs/sysfs test** reported:

| Driver state | Before 30-byte write | After 30-byte write |
|---|---:|---:|
| Buffer size | 1024 bytes | 1024 bytes |
| Data length | 0 bytes | 30 bytes |
| Open count | 0 | 1 |
| Last data | Empty | `Embedded Linux Lab2 Test Data` |

The corresponding sysfs results were `buffer_len = 30`, `open_count = 1`, and `last_data = Embedded Linux Lab2 Test Data`. The test ended with `=== procfs/sysfs/ Test PASSED ===`. **Counters depend on the order of operations**; running additional commands or tests can change them. A clean before/after comparison requires a fresh module state as recorded in the evidence.

## MTD and JFFS2 experiment

This portion of LAB02 ran on the **Ubuntu host** with a disposable NAND simulator; it was separate from the QEMU character-driver tests. The recorded simulator produced `/dev/mtd1` with **32 MiB capacity**, **16 KiB eraseblocks**, **512-byte pages**, and **16-byte OOB**.

### 1. Initialize and identify the simulated device

The host-side workflow uses `mtd-utils` and the `nandsim`, `mtdblock`, and `jffs2` kernel modules:

```bash
sudo modprobe nandsim first_id_byte=0x20 second_id_byte=0x35
sudo modprobe mtdblock
sudo modprobe jffs2
cat /proc/mtd
sudo mtdinfo /dev/mtd1
```

**Safety check:** MTD indices are assigned by the running host and can differ. In the recorded run, `/dev/mtd1` was explicitly named **`NAND simulator partition 0`**. **Do not use `flash_erase` or `nandwrite` unless you have confirmed that your target is the disposable simulated NAND device.** These commands erase/overwrite the selected MTD device.

### 2. Build and write the JFFS2 image

These commands are for the **disposable NAND simulator only**. First prepare the sample files on Ubuntu:

```bash
mkdir -p /tmp/jffs2_data/logs
printf 'Hello from JFFS2!\n' > /tmp/jffs2_data/hello.txt
printf 'Embedded Linux Lab2\n' > /tmp/jffs2_data/info.txt
date > /tmp/jffs2_data/logs/boot.log
sudo mkfs.jffs2 \
  --root=/tmp/jffs2_data \
  --eraseblock=0x4000 \
  --output=/tmp/rootfs.jffs2 \
  --pad --no-cleanmarkers
ls -lh /tmp/rootfs.jffs2
```

**Do not erase or write before verifying the simulated partition.** The following block performs the destructive steps **only if** `/proc/mtd` identifies `mtd1` as the expected NAND simulator:

```bash
if grep -q '^mtd1: .*"NAND simulator partition 0"' /proc/mtd; then
  sudo flash_erase /dev/mtd1 0 0
  sudo nandwrite -p /dev/mtd1 /tmp/rootfs.jffs2
else
  echo 'STOP: /dev/mtd1 is not verified as the NAND simulator.'
fi
```

The recorded image was 16 KiB and the log contained:

```text
Erasing 32768 Kibyte @ 0 -- 100 % complete
Writing data to block 0 at offset 0x0
```

### 3. Mount, read, and verify persistence

**Only after successful write to the verified simulator**, run the following on Ubuntu:

```bash
sudo mkdir -p /mnt/jffs2
sudo mount -t jffs2 /dev/mtdblock1 /mnt/jffs2
ls -la /mnt/jffs2
cat /mnt/jffs2/hello.txt
cat /mnt/jffs2/info.txt
cat /mnt/jffs2/logs/boot.log

sudo sh -c 'echo "New runtime file" > /mnt/jffs2/runtime.txt'
cat /mnt/jffs2/runtime.txt
sudo umount /mnt/jffs2
sudo mount -t jffs2 /dev/mtdblock1 /mnt/jffs2
cat /mnt/jffs2/runtime.txt
```

The final `cat` again printed `New runtime file`. The recorded `nanddump` check reported **ECC failed: 0**, **ECC corrected: 0**, and **bad blocks: 0**. The [original MTD/JFFS2 transcript](evidence/mtd_jffs2.txt) and [formatted log](evidence/clean/mtd_jffs2.txt) provide the actual command/output sequence.

This persistence check covers **unmount/remount while the same simulated NAND instance remains loaded**. It does not prove persistence after unloading `nandsim` or rebooting the Ubuntu host.

## Recorded results and evidence

| Test area | Original transcript | Clean excerpt | Recorded result |
|---|---|---|---|
| QEMU boot / automatic loading | [Boot log](evidence/boot_log.txt) | [Clean boot log](evidence/clean/boot_log.txt) | Linux 5.15.0 reached init; module loaded |
| Driver read/write | [Driver log](evidence/driver_test.txt) | [Clean driver log](evidence/clean/driver_test.txt) | `Test PASSED` |
| procfs / sysfs | [procfs log](evidence/procfs_test.txt) | [Clean procfs log](evidence/clean/procfs_test.txt) | `Test PASSED` |
| MTD / JFFS2 | [MTD/JFFS2 log](evidence/mtd_jffs2.txt) | [Clean MTD/JFFS2 log](evidence/clean/mtd_jffs2.txt) | Erase, write, mount, read, and remount verified |

The original files in `evidence/` preserve the recorded Terminal sessions. The files in `evidence/clean/` contain selected outputs, normalized presentation of commands, and section headings; **they are not separate execution runs**. Use the original logs to verify any excerpt or investigate kernel/boot messages omitted from the shortened versions.

See the [submitted PDF](report/SE200595_Lab02_BaoCao.pdf) for the accompanying documentation and screenshots.

## Troubleshooting and limitations

| Symptom | Recommended check |
|---|---|
| `insmod`: invalid module format | Compare guest kernel release/configuration and module `vermagic`; rebuild using the matching ARM kernel tree. |
| `lab2_driver.ko not found` in `rcS` | Ensure the module was copied into the **guest** `/lib/modules/5.15.0/` before packaging the initramfs. |
| `mknod: /dev/lab2: File exists` | The node is already present, commonly from devtmpfs; confirm its major/minor and permissions. |
| procfs/sysfs entries are missing | Check module loading, `dmesg`, and whether procfs/sysfs were mounted by `rcS`. |
| Open counters differ from the example | They reflect the current run; an isolated test after reloading the module gives a more comparable starting state. |
| JFFS2 mount fails | Verify the **simulator** identity, eraseblock size, valid JFFS2 image, and matching MTD block device. |
| QEMU cannot find a kernel, DTB, or initramfs | The full boot artifacts are **local prerequisites** and are not included in this repository's `lab02/` folder. |

## References

- Lecturer-provided LOS201 LAB02 specification and LAB01 prerequisite workflow.
- [Linux kernel documentation](https://docs.kernel.org/).
- [Linux MTD documentation](https://www.kernel.org/doc/html/latest/driver-api/mtd/).
- [BusyBox](https://busybox.net/).
- [QEMU ARM system emulation](https://www.qemu.org/docs/master/system/arm/).

[Back to the group repository overview](../README.md)
