# amd64 UEFI Platform
This platform provides support for booting on any UEFI-compatible 64-bit PC. It expects to be booted with the [Limine bootloader](https://limine-bootloader.org) or any other bootloader that implements the stivale2 protocol.

## Configuration
There are no build configuration options yet.

## Boot Arguments
The kernel's command line can be augmented with the following keys to control the platform at runtime:

- `console=`: Specify additional console devices, such as an IO port or 16650-compatible UART.

## Disk Image
Building a disk image to hold the EFI partition, and a data partition for the kernel is trivial. For example, on macOS, follow these steps:

First, create a disk image:

```
$ hdiutil create -size 400m -layout GPTSPUD disk
created: /private/tmp/disk.dmg
$ open disk.dmg
```

The size isn't critical, though the EFI partition should be at least 200M; but smaller doesn't seem to hurt things. Figure out which disk device the disk image is mounted as, which we'll refer to as `diskX` going forward. Check the partition table of this disk:

```
$ gpt show diskX
   start    size  index  contents
       0       1         PMBR
       1       1         Pri GPT header
       2      32         Pri GPT table
      34       6
      40  819120      1  GPT part - 48465300-0000-11AA-AA11-00306543ECAC
  819160       7
  819167      32         Sec GPT table
  819199       1         Sec GPT header
```

Your output will vary, but start off by deleting all partitions:

```
$ gpt remove -i 1 diskX
diskXs1 removed
$ gpt show diskX
   start    size  index  contents
       0       1         PMBR
       1       1         Pri GPT header
       2      32         Pri GPT table
      34  819133
  819167      32         Sec GPT table
  819199       1         Sec GPT header
```

With a blank disk, we'll go ahead and create the EFI and FAT data partitions now; note that they're not yet formatted, so your system will likely complain about unreadable media. Click the "Ignore" button on any such alerts.

```
$ gpt add -i 1 -b 40 -s 204800 -t C12A7328-F81F-11D2-BA4B-00A0C93EC93B diskX
diskXs1 added
$ gpt add -i 2 -b 204840 -t EBD0A0A2-B9E5-4433-87C0-68B6B72699C7 diskX
diskXs2 added
$ gpt show diskX
   start    size  index  contents
       0       1         PMBR
       1       1         Pri GPT header
       2      32         Pri GPT table
      34       6
      40  204800      1  GPT part - C12A7328-F81F-11D2-BA4B-00A0C93EC93B
  204840  614327      2  GPT part - EBD0A0A2-B9E5-4433-87C0-68B6B72699C7
  819167      32         Sec GPT table
  819199       1         Sec GPT header
```

Next, we'll have to format the partitions. This is also relatively simple; the EFI partition is forced to be FAT32, whereas the data partition will be either FAT16 or FAT32, depending on its size and how the filesystem formatting tools are feeling. (This partition is just used by the bootloader right now, which supports either.)

```
$ newfs_msdos -F 32 -v EFI diskXs1
512 bytes per physical sector
/dev/rdiskXs1: 201616 sectors in 201616 FAT32 clusters (512 bytes/cluster)
bps=512 spc=1 res=32 nft=2 mid=0xf8 spt=32 hds=16 hid=40 drv=0x80 bsec=204800 bspf=1576 rdcl=2 infs=1 bkbs=6
$ newfs_msdos -v KUSHDATA diskXs2
512 bytes per physical sector
/dev/rdisk9s2: 613984 sectors in 38374 FAT16 clusters (8192 bytes/cluster)
bps=512 spc=16 res=1 nft=2 rde=512 mid=0xf8 spf=150 spt=32 hds=54 hid=204840 drv=0x80 bsec=614327
```

At this point, you should be able to mount each of the partitions. The data partition is where you'll copy the kernel binary after building. Mount the EFI partition, and then prepare it with the following directory structure:
- EFI/BOOT
    - BOOTX64.EFI: This is the Limine loader, you can get this by following the link to binaries on the [release page](https://github.com/limine-bootloader/limine/releases)

Additionally, in the root of the data partition, you'll need to add a bootloader config file, at `/boot/limine.cfg`. An example file is as follows:

```
TIMEOUT=5
GRAPHICS=yes
VERBOSE=yes

:Kush
COMMENT=Kush operating system kernel
PROTOCOL=stivale2
KERNEL_PATH=boot://2/Boot/kernel-x86_64-pc-uefi.elf
KERNEL_CMDLINE=-console=debugcon,0xE9 -console=serial,0x3F8,115200
```
