+++
weight = 800
title = "Quick Start guide"
nameInMenu = "Quick Start"
draft = false
+++

## About this guide
You may first read the page about [installation](/installation) if you
plan to use FSArchiver from the Linux system which is installed on your
computer. This sections tells you how to use FSArchiver once it has been
installed, or from a livecd

## How to save filesystems to an archive
Here is **how to use FSArchiver** to backup a partition of your disk. Let's
consider your linux operating system is installed on **/dev/sda1** and
you want to back it up to a file on **/mnt/backup**. You can run this
command from a livecd:
```
fsarchiver savefs /mnt/backup/gentoo-rootfs.fsa /dev/sda1
```
You can also archive several filesystems in a single archive file:
```
fsarchiver savefs /mnt/backup/gentoo-rootfs.fsa /dev/sda1 /dev/sda2 /dev/volgroup/lv01
```

Here is an example of output when we save two filesystems to an archive:
```
# fsarchiver savefs -o /backup/backup-fsa/backup-fsa025-gentoo-amd64-20090103-01.fsa /dev/sda1 /dev/sda2 -v -j4 -A
filesystem features: [has_journal,resize_inode,dir_index,filetype,sparse_super,large_file]
============= archiving filesystem /dev/sda1 =============
-[00][REGFILE ] /vmlinuz-2.6.25.20-x64-fd13
-[00][REGFILE ] /sysresccd/memdisk
-[00][REGFILE ] /sysresccd/pxelinux.0
-[00][REGFILE ] /sysresccd/initram.igz
-[00][REGFILE ] /sysresccd/boot.cat
.....
-[00][DIR     ] /mkbootcd-gentoo64
-[00][REGFILE ] /System.map-2.6.25.20-x64-fd13
-[00][REGFILE ] /config-2.6.25.20-x64-fd13
-[00][REGFILE ] /config-2.6.27.09-x64-fd16
-[00][DIR     ] /
============= archiving filesystem /dev/sda2 =============
-[01][SYMLINK ] /bin/bb
-[01][REGFILE ] /bin/dd
-[01][REGFILE ] /bin/cp
-[01][REGFILE ] /bin/df
.....
-[01][REGFILE ] /fdoverlay/profiles/repo_name
-[01][DIR     ] /fdoverlay/profiles
-[01][DIR     ] /fdoverlay
-[01][DIR     ] /
```

## How to extract filesystems from an archive
FSArchiver supports multiple filesystems per archive. For that reason, you have
to specify which filesystem you want to restore. Each filesystem has a number
starting at 0. The first filesystem in the archive will be filesystem number 0,
the second will be filesystem number 1, ... You can restore either one filesystem
at a time, or several filesystems with just one command.

Here is how to restore a filesystem from an archive when there is only one
filesystem in that archive:
```
fsarchiver restfs /mnt/backup/gentoo-rootfs.fsa id=0,dest=/dev/sda1
```
There is how to restore the second filesystem from an archive (second = number 1):
```
fsarchiver restfs /mnt/backup/archive-multple-filesystems.fsa id=1,dest=/dev/sdb1
```
You can also restore both the first and the second filesystem in the same time: (numbers 0 and 1)
```
fsarchiver restfs /mnt/backup/archive-multple-filesystems.fsa id=0,dest=/dev/sda1 id=1,dest=/dev/sdb1
```

Option **-F** was used to convert a filesystem in old version. For
instance, it allows to restore a filesystem which was ext2 when it was saved as
reiserfs on the new partition. Now, you have to specify option *mkfs=xxx* with
the destination partition. Here is how to restore the first filesystem from an
archive to /dev/sda1 and to convert it to reiserfs in the same time:
```
fsarchiver restfs /mnt/backup/gentoo-rootfs.fsa id=0,dest=/dev/sda1,mkfs=reiserfs
```

## Display info about an archive
It may be useful to know what has been saved in an archive. You can do this using
**archinfo**. It will tell you how many filesystems there are, their properties,
the original size of the filesystem and how much space is used:
```
fsarchiver archinfo /backup/backup-fsa/sysimg-t3p5g965-debian-20100131-0716.fsa
```

Here is an example of output:
```
# fsarchiver archinfo /backup/backup-fsa/sysimg-t3p5g965-debian-20100131-0716.fsa
====================== archive information ======================
Archive type:                   filesystems
Filesystems count:              2
Archive id:                     4b610c6e
Archive file format:            FsArCh_002
Archive created with:           0.6.6
Archive creation date:          20100131-07:16:35
Archive label:                  debian-backup
Compression level:              7 (lzma level 1)
Encryption algorithm:           none

===================== filesystem information ====================
Filesystem id in archive:       0
Filesystem format:              ext3
Filesystem label:               boot
Filesystem uuid:                d76278bf-5e65-4568-a899-9558ce61bf06
Original device:                /dev/sda1
Original filesystem size:       961.18 MB (1007869952 bytes)
Space used in filesystem:       356.86 MB (374190080 bytes)

===================== filesystem information ====================
Filesystem id in archive:       1
Filesystem format:              ext3
Filesystem label:               debian
Filesystem uuid:                4b0da78f-7f02-4487-a1e2-774c9b412277
Original device:                /dev/vgmain/snapdeb
Original filesystem size:       11.81 GB (12682706944 bytes)
Space used in filesystem:       7.11 GB (7635599360 bytes)
```

## Multi-thread compression
FSArchiver also supports multi-threaded [compression](/compression/). If you have
a multi-core processor (eg: dual-core or quad-core) you should create several
compression jobs so that all the cores are used. It will make the compression or
decompression a lot faster. For instance, if you have a dual-core, you should
use option **-j2** to create two compression threads to use the power of the two
cores. If you have a quad-core cpu, option **-j4** is recommended,
except if you want to leave one core idle for other programs. In that case you
can use **-j3**. Here is an example of multi-threaded compression:
```
fsarchiver -j3 -o savefs /mnt/backup/gentoo-rootfs.fsa /dev/sda1
```

## Splitting the archive into several volumes
If the archive file is very big, you may want to split it into several small
files. For instance, if the size of your backup is 8GB and you want to save it
on DVD+RW discs, it may be useful to split the archive into volumes of 4.3GB.
File splitting is supported in FSArchiver-0.3.0 and newer. To use it when you
create an archive, you just have to use option **-s** to specific the
size you want for each volume, in mega-bytes.
```
fsarchiver savefs -s 4300 /data/backup-rhel-5.2-fsa033.fsa /dev/sda1
```
The first volume always have an **.fsa** extension. The names of the
next volumes will terminate with **.f01**, **.f02**, **.f03**, ...
When you restore the archive, you just have to specify the path to the first
volume on the command line, and it will automatically use the next volumes if
they are in the same directory. Else it will display a prompt, where you can
specify another location for a volume.

## Execution environment
FSArchiver requires the file-system tools to be installed to save the filesystem
attributes (when you do a **fsarchiver savefs**) and it also requires these tools
to recreate the file-system when you do a **fsarchive restfs**. Anyway, you only
need the tools of the current file-system to be installed. In other words, you
don't require xfsprogs to be installed if you only work on an ext3 file-system. 

For these reasons, it's a good idea to run FSArchiver from an environment with
all the system tools installed. The best environment is
[SystemRescueCd](http://www.system-rescue-cd.org), since it comes
with all the linux file-system tools and a very recent FSArchiver version.

It's also important that you make sure that SELinux is not enabled in the kernel
running FSArchiver when you save a file-system which has been labeled by SELinux,
or you can use FSArchiver with SELinux enabled if you are sure that the context
where it's running has enough privileges to read the extended-attributes related
to SELinux. In the other cases, the kernel could return *unlabeled* instead of
the real value of the *security.selinux* attribute. Then FSArchiver would not
preserve these attributes and then the system would not work when you restore
your root filesystem, or you would have to ask the SELinux to relabel the
file-system. The SELinux support is disabled by default if you use FSArchiver
from SystemRescueCd-1.1.3 or newer, so your SELinux labels will be preserved if
you use FSArchiver from that environment.

## Detection of the filesystems
FSArchiver is able to detect the filesystems which are installed on all the
disks of a computer. This is very useful when you want to work on a partition
when you don't know what is its device name.
```
# fsarchiver probe simple
[=====DEVICE=====] [==FILESYS==] [=====LABEL=====] [====SIZE====] [MAJ] [MIN]
[/dev/sda1       ] [ext3       ] [boot           ] [   768.72 MB] [  8] [  1]
[/dev/sda2       ] [reiserfs   ] [gentoo         ] [    12.00 GB] [  8] [  2]
[/dev/sda3       ] [ext3       ] [data           ] [   350.00 GB] [  8] [  3]
[/dev/sda4       ] [ext3       ] [backup         ] [   300.00 GB] [  8] [  4]
[/dev/sda5       ] [lvm2pv     ] [               ] [   134.38 GB] [  8] [  5]
[/dev/sda6       ] [lvm2pv     ] [               ] [   106.24 GB] [  8] [  6]
[/dev/sdb1       ] [reiserfs   ] [usb8gb         ] [     7.46 GB] [  8] [ 17]
```

## Command line and its arguments
```
====> fsarchiver version 0.6.12 (2010-12-25) - http://www.fsarchiver.org <====
Distributed under the GPL v2 license (GNU General Public License v2).
 * usage: fsarchiver [<options>] <command> <archive> [<part1> [<part2> [...]]]
<commands>
 * savefs: save filesystems to an archive file (backup a partition to a file)
 * restfs: restore filesystems from an archive (overwrites the existing data)
 * savedir: save directories to the archive (similar to a compressed tarball)
 * restdir: restore data from an archive which is not based on a filesystem
 * archinfo: show information about an existing archive file and its contents
 * probe [detailed]: show list of filesystems detected on the disks
<options>
 -o: overwrite the archive if it already exists instead of failing
 -v: verbose mode (can be used several times to increase the level of details)
 -d: debug mode (can be used several times to increase the level of details)
 -A: allow to save a filesystem which is mounted in read-write (live backup)
 -a: allow running savefs when partition mounted without the acl/xattr options
 -e <pattern>: exclude files and directories that match that pattern
 -L <label>: set the label of the archive (comment about the contents)
 -z <level>: compression level from 1 (very fast)  to  9 (very good) default=3
 -s <mbsize>: split the archive into several files of <mbsize> megabytes each
 -j <count>: create more than one compression thread. useful on multi-core cpu
 -c <password>: encrypt/decrypt data in archive, "-c -" for interactive password
 -h: show help and information about how to use fsarchiver with examples
 -V: show program version and exit
<information>
 * Support included for: lzo=yes, lzma=yes
 * support for ntfs filesystems is unstable: don't use it for production.
<examples>
 * save only one filesystem (/dev/sda1) to an archive:
   fsarchiver savefs /data/myarchive1.fsa /dev/sda1
 * save two filesystems (/dev/sda1 and /dev/sdb1) to an archive:
   fsarchiver savefs /data/myarchive2.fsa /dev/sda1 /dev/sdb1
 * restore the first filesystem from an archive (first = number 0):
   fsarchiver restfs /data/myarchive2.fsa id=0,dest=/dev/sda1
 * restore the second filesystem from an archive (second = number 1):
   fsarchiver restfs /data/myarchive2.fsa id=1,dest=/dev/sdb1
 * restore two filesystems from an archive (number 0 and 1):
   fsarchiver restfs /data/arch2.fsa id=0,dest=/dev/sda1 id=1,dest=/dev/sdb1
 * restore a filesystem from an archive and convert it to reiserfs:
   fsarchiver restfs /data/myarchive1.fsa id=0,dest=/dev/sda1,mkfs=reiserfs
 * save the contents of /usr/src/linux to an archive (similar to tar):
   fsarchiver savedir /data/linux-sources.fsa /usr/src/linux
 * save a filesystem (/dev/sda1) to an archive split into volumes of 680MB:
   fsarchiver savefs -s 680 /data/myarchive1.fsa /dev/sda1
 * save a filesystem and exclude all files/dirs called 'pagefile.*':
   fsarchiver savefs /data/myarchive.fsa /dev/sda1 --exclude='pagefile.*'
 * generic exclude for 'share' such as '/usr/share' and '/usr/local/share':
   fsarchiver savefs /data/myarchive.fsa --exclude=share
 * absolute exclude valid for '/usr/share' but not for '/usr/local/share':
   fsarchiver savefs /data/myarchive.fsa --exclude=/usr/share
 * save a filesystem (/dev/sda1) to an encrypted archive:
   fsarchiver savefs -c mypassword /data/myarchive1.fsa /dev/sda1
 * Same as before but prompt for password in the terminal:
   fsarchiver savefs -c - /data/myarchive1.fsa /dev/sda1
 * extract an archive made of simple files to /tmp/extract:
   fsarchiver restdir /data/linux-sources.fsa /tmp/extract
 * show information about an archive and its file systems:
   fsarchiver archinfo /data/myarchive2.fsa
```
