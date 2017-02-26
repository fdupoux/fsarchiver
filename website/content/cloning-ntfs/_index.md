+++
weight = 1200
title = "Cloning NTFS"
nameInMenu = "Cloning NTFS"
draft = false
+++

## Overview
FSArchiver has experimental support for NTFS partitions, so it can be used to
clone partitions where Windows is installed or where Windows data are saved. It
can be used to make a backup of your Windows installation, or it can be used to
clone a Windows installation to another computer. FSArchiver will preserve all
the files and their attributes with the limitations listed below. FSArchiver is
able to do flexible backups. It means you will be able to restore the ntfs
filesystem to a partition with is bigger or smaller than the original one, as
long as it's big enough to store all the data. It's very different from
partimage which is doing a static copy at the block level, and then partimage is
not able to restore a filesystem to a smaller partition. Tests have been made
with Windows XP, 2003, Vista, and fsarchiver has been able to save and restore
the ntfs filesystem. After the restoration Windows was still bootable even if
the partition is smaller or bigger. You must be aware that Windows 2000/XP/2003
may not be able to boot if you clone it to a computer which has a different
hardware (motherboard), but you may try. It should work when you clone Vista on
a different hardware. Also this feature has not been tested on Windows 7 and
more recent versions of Windows. This is experimental, use at your own risk.
 
## Requirements
To clone NTFS partitions, you need ntfs-3g-2009.11.14 or more recent, and a
recent fsarchiver release. It's recommended to use the latest version of these
two programs. The most convenient way to use it is to run a recent fsarchiver
version from [SystemRescueCd](http://www.system-rescue-cd.org). It's a livecd that
comes with fsarchiver and all the filesystems tools.

## Current status
The ntfs support for fsarchiver is not stable, hence you must be careful: use at
your own risk. There is a risk that data are not preserved or that you don't get
what you expect when you restore your partition. Fortunately there is no risk to
damage your original partition when you just save its ntfs filesystem to an
archive. The partitions are mounted read-only during the backup, so you can save
an NTFS partition to an archive, and try to restore it somewhere else.

The ntfs support in fsarchiver is still experimental and incomplete so you must
be careful and you should not use it on production systems. If you use it on a
very standard Windows XP and you restore to the same partition it will probably
work, but there can be problems when you try to restore it on a partition
located somewhere else (eg: disk number 1 -> disk number 2, or partition
number 1 -> partition number 2).

## Limitations

* FSArchiver is unable to save Alternate-Data-Streams larger than 64k which are
associated with files. Streams are additional contents which are associated with
files. In general it's used to store extra info about a file: for instance you
can set comments on files from the explorer, these comments will be stored in
Streams, and the normal contents of the file won't be altered. An error message
like that one will be displayed if large streams are seen:
 create.c#265,createar_item_xattr(): file [/Temp/file-with-large-stream.txt] has
 an xattr [user.mystream] with data too big (size=71157, maxsize=64k)
* FSArchiver will recreate the compressed files as uncompressed files: you will
have to recompress files by hand from the explorer. It only affects the files
which are transparently compressed by the NTFS filesystem. All the files which
are compressed by an application will not be affected (zip, rar, jpg, ...).
There is no data loss, so it's not a critical limitation.

## Bootable partitions
If you are trying to save and restore partitions where Windows is installed, you
must be sure that the partition will still be active and that the boot.ini will
be consistent after you restore the ntfs partition. Also, the Windows filesystem
must be restored on a primary partition: it won't boot if it's installed on a
logical partition (inside an extended partition). The MBR has up to four primary
partition. Only one may be active. The MBR can only boot Windows if the
partition is marked as active. You can change this using tools such as Parted
or GParted.

If you have more than one partition on the hard disk, you have to check the
number of each partition. In general the partition number 1 is the first one on
the disk, but the situation may be different. The numbers associated with the
partition may not be in the expected order. You have to check that the number of
the partition where you restored the Windows filesystem match the number which
is written in boot.ini (boot.ini only exists in Windows 2000, 2003, XP). To
check it's correct and to edit boot.ini, you have to mount the ntfs partition
which has been restored. You have to used ntfs-3g to do that, and then use an
editor such as nano or vim to modify boot.ini.

In the following example, the partition where Windows is installed is partition
number 1 (*/dev/sda1*).
```
# fsarchiver probe simple
[=====DEVICE=====] [==FILESYS==] [=====LABEL=====] [====SIZE====] [MAJ] [MIN]
[/dev/sda1       ] [ntfs       ] [winxp32        ] [    16.00 GB] [  8] [  1]
[/dev/sda2       ] [ext3       ] [boot           ] [   976.55 MB] [  8] [  2]
[/dev/sda3       ] [reiserfs   ] [gentoo         ] [    16.00 GB] [  8] [  3]
[/dev/sda4       ] [lvm2pv     ] [<unknown>      ] [   898.56 GB] [  8] [  4]
```

We can mount this partition from SystemRescueCd using ntfs-3g:
```
# ntfs-3g /dev/sda1 /mnt/windows
```
And then we check that the boot partition is set to partition 1 in boot.ini:

```
# cat /mnt/windows/boot.ini
[boot loader]
timeout=1
default=multi(0)disk(0)rdisk(0)partition(1)\windows
[operating systems]
multi(0)disk(0)rdisk(0)partition(1)\windows="Microsoft Windows XP" /noexecute=optin /fastdetect
```

If we want to edit this file, we can use and editor such as nano or vim:
```
# nano /mnt/windows/boot.ini
```

After these changes, we have to unmount the partition: 
```
# umount /mnt/windows
```
