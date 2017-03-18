+++
weight = 1000
title = "Live system backup"
nameInMenu = "Live-backup"
draft = false
+++

## About
FSArchiver can be used to backup linux operating systems when they are running.
In other words, if you have linux installed on your hard disk, and it's currently
running, you can make a backup of that disk using fsarchiver. It's called a
**live backup** or *hot backup*. All you need is an fsarchiver binary, an another
filesystem where to save the archive. It can be on another partition on the
hard-disk, or a network file-system such as Samba of NFS.
 
## Backup with a snapshot
If the partitions you want to save are LVM (Logical volume Manager)
Logical-Volumes, and it you have free extents in the Volume-Group, then you can
make a filesystem snapshot of these Logical-Volumes. A snapshot if a frozen copy
of a Logical-Volume made at a given time. After that time, the original
partition can still be modified normally, and the snapshot provides a consistent
filesystem that can be backed up. To create an LVM snapshot, you have to use
lvcreate with option `--snapshot`, and then the snapshot can be mounted
read-only on a directory. By default, all the partitions except /boot are
configured as Logical-Volumes with recent Redhat-Enterprise and Fedora
distributions. You can have the list of all your Logical-Volumes with the
command called `lvdisplay`. If this command is not installed on your system,
you are probably not using LVM.
```
# lvdisplay -c
  /dev/vgmain/distfiles:vgmain:3:1:-1:1:12582912:1536:-1:0:-1:251:21
  /dev/vgmain/misc:vgmain:3:1:-1:1:50331648:6144:-1:0:-1:251:29
  /dev/vgmain/tftpboot:vgmain:3:1:-1:1:1048576:128:-1:0:-1:251:30
  /dev/vgmain/vdisk:vgmain:3:1:-1:1:209715200:25600:-1:0:-1:251:31
  /dev/vgmain/chrooti386:vgmain:3:1:-1:2:1572864:192:-1:0:-1:251:32
```
You can use [rubackup](http://www.rubackup.org/) to make fsarchiver backups, and
it can automatically create an LVM Snapshot just before the backup, and it will
delete just after the backup has been done.

## Backup with no snapshot
If your partition are not LVM Logical-Volumes, you can't make a snapshot. If the
partition are not used, it's recommended to remount it as read-only, with the
following command:
```
mount -o remount,ro /dev/xxx
```
If the partition cannot be remounted read-only (which is the case of the root
filesystem in general), it's still possible to use fsarchiver to make a backup,
but you will have to take extra care. By default, fsarchiver complains if you
try to save a filesystem which is mounted in read-write mode. This is because
in cannot guarantee that the data will be consistent because files may change
during the backup of the filesystem. This is the reason why it shows the
following error message, and stops:
```
# fsarchiver savefs /mnt/archives/gentoo-backup-20090328-01.fsa /dev/sda2 -v
create.c#0642,filesystem_mount_partition(): partition [/dev/sda2] is mounted read/write. 
     please mount it read-only and then try again. you can do "mount -o remount,ro /dev/sda2". 
     you can also run fsarchiver with option '-A' if you know what you are doing. 
removing /mnt/archives/gentoo-backup-20090328-01.fsa
```
When a filesystem is writeable during the backup, it means changes can be done
in files during that time, and there may be inconsistencies in the data. For
instance, if you are backing up a web server which is running both Apache and
Mysql, the Mysql database refers to files that can be uploaded in the Apache
directory from the website. In that case the backup could contains a reference
in the database but not the referred file because these files have been backed
up already. So you have to know whether or not your system may have such
inconsistencies. 

If there is no risk of inconsistency, then you can use fsarchiver with option
**-A** to continue the backup of a filesystem which is mounted in read-write mode.

## Restoration
If you have a problem on your filesystems, you may want to restore the
live-backup you made. You cannot restore a filesystem which is mounted, so
it's necessary to restore from a Linux-Rescue system. We recommend that you
use [SystemRescueCd](http://www.system-rescue-cd.org) for multiple reasons:

* it comes with a recent version of fsarchiver
* you can boot it from the cdrom, an [usb stick]
(http://www.system-rescue-cd.org/Installing-SystemRescueCd-on-a-USB-stick/),
or [from the network](http://www.system-rescue-cd.org/manual/PXE_network_booting/).
* it contains [all the filesystem tools](http://www.system-rescue-cd.org/System-tools)
that fsarchiver may need
