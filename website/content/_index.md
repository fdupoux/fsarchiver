+++
weight = 100
title = "Filesystem Archiver for Linux"
nameInMenu = "Homepage"
draft = false
+++

## About FSArchiver
FSArchiver is a system tool that allows you to **save the contents of a 
file-system to a compressed archive file**. The file-system can be restored on 
a partition which has a different size and it can be restored on a different 
file-system. Unlike tar/dar, **FSArchiver also creates the file-system when it 
extracts the data** to partitions. Everything is **checksummed** in the 
archive in order to protect the data. If the archive is corrupt, you just loose 
the current file, not the whole archive. Fsarchiver is released under the 
GPL-v2 license. You should read the [Quick start guide](/quickstart/)
if you are using FSArchiver for the first time

## Detailed description
The purpose of this project is to provide a **safe and flexible file-system 
backup/deployment tool**. Other open-source file-systems tools such as partimage 
already exist. These tools are working at the filesystem blocks level, so it is 
not possible to restore the backup to a smaller partition, and restoring to a 
bigger partition forces you to resize the filesystem by hand. To have more 
details about it, read [comparison with partimage](/partimage/)

The purpose is to have a **very flexible** program. FSArchiver can extract an 
archive to a partition which is *smaller that the original one* as long as 
there is enough space to store the data. It can also **restore the data on a 
different file-system**, so it can use it when you want to **convert your 
file-system**: you can backup an ext3 file-system, and restore it as a reiserfs.

FSArchiver is working at the file level. It can make an archive of filesystems 
(ext4, ext3, xfs, btrfs, reiserfs, ntfs, ...) that the running kernel can mount 
with a read-write support. It will **preserve all the standard file attributes** 
(permissions, timestamps, symbolic-links, hard-links, 
extended-attributes, ...), as long as the kernel has support for it enabled. 
It allows to preserve all the windows file attributes (ACL, standard 
attributes, ...). It can be used with LVM snapshots in order to 
[make consistent backups of all filesystems](http://www.system-rescue-cd.org/lvm-guide-en/Making-consistent-backups-with-LVM/)
including the root filesystem.

FSArchiver has been **packaged by most popular Linux distributions** (Fedora, 
Debian, Ubuntu, OpenSUSE, ArchLinux, Gentoo) hence it can be installed very 
easily from the standard package repositories using the standard yum / apt-get 
/ emerge / pacman commands. It can also be used from SystemRescueCd, as it 
comes with all run-time dependencies, so that you can restore your system and 
data after a problem.

## Implemented features
The following features have already been implemented in the current version:

* Support for basic file attributes (permissions, ownership, ...)
* Support for basic file-system attributes (label, uuid, block-size) for all
linux file-systems
* Support for multiple file-systems per archive
* Support for extended file attributes (they are used by SELinux)
* Support for all major Linux filesystems (extfs, xfs, btrfs, reiserfs, etc)
* Support for FAT filesystems (in order to backup/restore EFI System Partitions)
* Experimental support for [cloning ntfs filesystems](/cloning-ntfs/)
* Checksumming of everything which is written in the archive (headers, data
blocks, whole files)
* Ability to restore an archive which is corrupt (it will just skip the current
file)
* Multi-threaded lzo, gzip, bzip2, lzma/xz [compression](/compression/):
if you have a dual-core / quad-core it will use all the power of your cpu
* Lzma/xz [compression](/compression/) (slow but very efficient algorithm)
to make your archive smaller.
* Support for splitting large archives into several files with a fixed maximum size
* Encryption of the archive using a password. Based on blowfish from libgcrypt.

## Limitations
There are several limitations anyway: it cannot preserve filesystem attributes 
that are very specific. For instance, if you create a snapshot in a btrfs 
volume (the new-generation file system for linux), FSArchiver won't know 
anything about that, and it will just backup the contents seen when you mount 
the partition.

FSArchiver is safe when it makes backups of partitions which are not mounted or
mounted read-only. There is an option to force the backup of a read-write
mounted volume, but there may be problems with the files that changed during the
backup. If you want to backup partition which are in use, the best thing to do
is to make an LVM snapshot of the partition using **lvcreate -s**, which is part
of the LVM userland tools. Unfortunately you can only make snapshots of
partitions which are LVM Logical Volumes.

You can have more details about the [current status](/status/) of that project.

## Protection against data loss
FSArchiver is using two levels of checksums to protect your data against
corruption. Each block of each file has a 32bit checksum written in the archive.
That way we can identify which block of your file is damaged. Once a file has
been restored, the md5 checksum of the whole file is compared to the original
md5. It's a 128bit checksum, so it's will detect all file corruptions. In case
one file is damaged, FSArchiver will restore all the other files from your
archive, so you won't loose all your data. It's very different from tar.gz where
the whole tar is compressed with gzip. In that case, the data which are written
after the corruption are lost.

## Download
* You can download either [the sources](https://github.com/fdupoux/fsarchiver/releases/download/0.8.2/fsarchiver-0.8.2.tar.gz)
or a static binary from the [github releases page](https://github.com/fdupoux/fsarchiver/releases).
* You can also download [SystemRescueCd](http://www.system-rescue-cd.org/) which is
a livecd that provides a recent FSArchiver and all the file-system tools and
libraries required. So if you want to use FSArchiver to save or restore your
root file-system, the best thing to do is to run FSArchiver from this livecd.
* You can track the changes using the [detailed ChangeLog](/changelog/) page.

## Contact
You can ask technical questions, and general questions in the [forums](https://forums.fsarchiver.org/).
