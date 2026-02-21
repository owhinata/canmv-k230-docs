# SD Card Partition Expansion

The default GPT on the CanMV K230 is truncated at sector 1,048,575 (approximately 512MB).
This guide expands **rootfs to 64GB and fat32appfs to the remaining space (FAT32)**.

Partition layout after expansion:

| Partition | Filesystem | Size |
|-----------|-----------|------|
| rootfs (mmcblk1p3) | ext4 | 64 GB |
| fat32appfs (mmcblk1p4) | FAT32 | ~174 GB |

## Prerequisites

- Serial console access to the K230 (`/dev/ttyACM0`, 115200 bps)
- `expect` installed on the host machine
- Tools available on K230: `parted`, `resize2fs`, `mkfs.vfat`

!!! danger "fdisk cannot be used"
    The busybox `fdisk` on K230 **does not support GPT**. Always use `parted`.

!!! warning "Data backup recommended"
    This procedure recreates the rootfs partition (p3).
    Data is preserved because the start sector remains unchanged, but a backup is recommended.
    fat32appfs (p4) data is protected by the p5 staging method.

## Partition Layout (Before → After)

### Before

```
Disk /dev/mmcblk1: 499744768 sectors (~238GB)
Logical sector size: 512 bytes

#  Start (sector)    End (sector)   Size    Name
1           20480          61439    20.0M   rtt
2           61440         163839    50.0M   linux
3          262144         524287    128M    rootfs
4          524288        1048575    256M    fat32appfs
```

The GPT backup header ends at sector 1,048,575, leaving the actual disk end (499,744,734) unused.

### After

```
#  Start (sector)    End (sector)   Size    Name
1           20480          61439    20.0M   rtt          ← unchanged
2           61440         163839    50.0M   linux         ← unchanged
3          262144      134479871    64GB    rootfs        ← expanded to 64GB
4       134479872      499742719    174G    fat32appfs    ← all remaining space
```

## Step 1: Check Current State and Fix GPT

Connect to the K230 serial console and confirm the current partition layout.
If the GPT is truncated (Fix/Ignore? appears), it is fixed at the same time.

```sh
expect -c '
  log_user 1
  set timeout 5
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect {
    -re "login:" {
      send "root\r"
      expect "]#"
    }
    "]#" { }
  }

  send "parted /dev/mmcblk1\r"
  expect "(parted)"

  send "print\r"
  expect {
    -re "Fix/Ignore\\?" {
      send "Fix\r"
      expect "(parted)"
      send "print\r"
      expect "(parted)"
    }
    "(parted)" { }
  }

  send "quit\r"
  expect "]#"
'
```

!!! info "About the Fix/Ignore? Prompt"
    If `print` shows "Fix/Ignore?", the GPT backup header is not at the end of the disk.
    Send `Fix` to relocate it. If no prompt appears, the GPT is already correct.

## Step 2: Create Temporary p5 Partition

To protect fat32appfs (p4) data on the K230, create a temporary p5 partition.
By using the same start sector (134479872s) as the final p4,
the vfat data on disk is preserved even after repartitioning in Step 4.

!!! tip "p5 and new p4 must share the same start sector"
    Set p5's start sector (134479872s) identical to the final p4's start sector.
    This ensures that after repartitioning in Step 4, the vfat data remains on disk
    — only the GPT entry is updated.
    If the sectors differ, data will be lost. **Always use 134479872s as the start sector.**

```sh
expect -c '
  log_user 1
  set timeout 5
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect {
    -re "login:" {
      send "root\r"
      expect "]#"
    }
    "]#" { }
  }

  send "parted /dev/mmcblk1\r"
  expect "(parted)"

  send "mkpart fat32appfs_tmp fat32 134479872s 100%\r"
  expect "(parted)"

  send "quit\r"
  expect "]#"
'
```

## Step 3: Format, Mount, and Copy Data to p5

Copy all fat32appfs (p4) data to p5.
This may take several minutes depending on the amount of data. Wait until the "]#" prompt returns.

```sh
expect -c '
  log_user 1
  set timeout 50
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect {
    -re "login:" {
      send "root\r"
      expect "]#"
    }
    "]#" { }
  }

  send "mkfs.vfat -F 32 /dev/mmcblk1p5\r"
  expect "]#"

  send "mkdir -p /mnt/p5_tmp\r"
  expect "]#"

  send "mount /dev/mmcblk1p5 /mnt/p5_tmp\r"
  expect "]#"

  send "cp -a /sharefs/. /mnt/p5_tmp/\r"
  expect "]#"
'
```

## Step 4: Unmount and Repartition

Unmount p5 and /sharefs, then repartition with parted.
The new p4 is created at the same start sector (134479872s) as p5,
so the copied data is preserved as-is. No reformatting is needed.

!!! danger "parted -s (script mode) cannot be used"
    parted -s refuses to delete partitions on the root filesystem.
    Always use **interactive mode**.

!!! warning "Do not change the start sector 262144"
    Changing the start sector of p3 (rootfs) will corrupt existing ext4 data.
    **Always start from sector 262144s.**

!!! warning "Partition names must match the originals"
    Always specify the **exact original partition name** in parted's mkpart command.
    For example, p4 must be named fat32appfs, not sharefs.
    Using an incorrect name can corrupt fstab entries and boot scripts.

```sh
expect -c '
  log_user 1
  set timeout 5
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect {
    -re "login:" {
      send "root\r"
      expect "]#"
    }
    "]#" { }
  }

  send "umount /mnt/p5_tmp\r"
  expect "]#"

  send "umount /sharefs\r"
  expect "]#"

  send "parted /dev/mmcblk1\r"
  expect "(parted)"

  # Delete p5
  send "rm 5\r"
  expect "(parted)"

  # Delete p4
  send "rm 4\r"
  expect "(parted)"

  # Delete p3 (Yes/No? + Ignore/Cancel? appear because rootfs is in use)
  send "rm 3\r"
  expect -re "Yes/No\\?"
  send "Yes\r"
  expect {
    -re "Ignore/Cancel\\?" {
      send "Ignore\r"
      expect "(parted)"
    }
    "(parted)" { }
  }

  # Recreate p3 (keep start sector 262144s, expand to 64GB)
  send "mkpart rootfs ext4 262144s 134479871s\r"
  expect {
    -re "Ignore/Cancel\\?" {
      send "Ignore\r"
      expect "(parted)"
    }
    "(parted)" { }
  }

  # Recreate p4 (same start sector as p5: 134479872s, all remaining space)
  send "mkpart fat32appfs fat32 134479872s 100%\r"
  expect "(parted)"

  send "print\r"
  expect "(parted)"

  send "quit\r"
  expect "]#"
'
```

!!! info "About the Kernel Notification Failure Error"
    If you see "Error: Partition(s) ... have been written, but we have been unable to inform the kernel",
    select Ignore. The kernel will be notified when you reboot in Step 5.

## Step 5: Reboot

Reboot so the kernel recognizes the new partition table.
Wait for boot to complete (approximately 35 seconds).

```sh
expect -c '
  log_user 1
  set timeout 5
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect {
    -re "login:" {
      send "root\r"
      expect "]#"
    }
    "]#" { }
  }

  send "reboot\r"
  sleep 1
'
```

## Step 6: Expand rootfs (resize2fs)

After rebooting, use `resize2fs` to expand the ext4 filesystem to the new partition size.
**This can be done online (while mounted).**

```sh
expect -c '
  log_user 1
  set timeout 120
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect {
    -re "login:" {
      send "root\r"
      expect "]#"
    }
    "]#" { }
  }

  send "resize2fs /dev/mmcblk1p3\r"
  expect "]#"
'
```

Expected output on success:

```
The filesystem on /dev/mmcblk1p3 is now 67108864 blocks long.
```

## Step 7: Verify

```sh
expect -c '
  log_user 1
  set timeout 5
  set serial [open /dev/ttyACM0 r+]
  fconfigure $serial -mode 115200,n,8,1 -translation binary -buffering none
  spawn -open $serial

  send "\r"
  expect {
    -re "login:" {
      send "root\r"
      expect "]#"
    }
    "]#" { }
  }

  send "df -h\r"
  expect "]#"
'
```

Expected output:

```
Filesystem        Size  Used Avail Use% Mounted on
/dev/root        62.0G   72.2M  61.9G   0% /
/dev/mmcblk1p4  174.1G   92.1M  174.0G   0% /sharefs
```

!!! info "Auto-mount"
    `/sharefs` is automatically mounted at boot (configured in `/etc/fstab`).

## Troubleshooting

### resize2fs returns an error

```sh
# Run fsck first, then retry resize2fs
e2fsck -f /dev/mmcblk1p3
resize2fs /dev/mmcblk1p3
```

### Partition size unchanged after Step 4

Verify that the reboot in Step 5 completed. The kernel retains the old partition table until reboot.

### parted shows "Error: Partition(s) 3, 4 on /dev/mmcblk1 have been written..."

This message means the kernel could not be notified of the partition changes.
**The reboot in Step 5 resolves this.** Running resize2fs before the reboot is still effective.

### parted shows "Partition is being used" when deleting p3

This warning appears because rootfs is mounted. Enter Yes to continue.
The kernel will recognize the new partition table after the next reboot.
