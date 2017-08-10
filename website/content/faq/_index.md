+++
weight = 600
title = "Frequently asked questions"
nameInMenu = "FAQ"
draft = false
+++

## Can the restored file-system be a boot filesystem (like the root partition) ?
Yes, FSArchiver can backup the root file-system, but you may have to run
grub-installer again after you restore the file-system where the boot-loader
(grub) is installed. FSArchiver has been successfully able to save and restore
the root file-system of a Fedora-8 with SELinux on it as you can see in the
[status page](/status).

## When I save the root file system, and restore it as another file-system, is the fstab modified ?
FSArchiver is working at the file-system, it's not supposed to know anything
about the contents of the file-system. It's just supposed to preserve the
files/directories and their attributes, and the human has to think about the
changes required to get it to work (support for file-system in the kernel,
fstab, ...). FSArchiver also aims to preserve the important file-system
attributes, such as the file-system label / UUID. We assume the system
administrator knows what he is doing, and fixing the fstab is trivial, so the
administrator will just have to edit fsatb before he reboots. You can also set
the file-system as **auto** in **fstab** so that it can boot
on any file-system.

## What file-systems are currently supported ?
FSArchiver is able to save/restore all Linux file-systems (ext2, ext3, ext4,
reiserfs, reiser4, xfs, jfs, btrfs) as well as FAT16/FAT32 for backing up an EFS
(EFI file system). The specific file-system attributes are supported for all
these file-systems. It means that both the data (file contents, file attributes)
and file-system attributes (label, uuid, block size) are preserved as well as
several advanced file-system attributes such as inode-size on ext4. Anyway,
several advanced file-systems attributes still need to be implemented, such as
the journal attributes for ext4.
