+++
weight = 500
title = "Comparison with partimage"
nameInMenu = "Partimage"
draft = false
+++

Here is a table that summarizes the differences between partimage and
fsarchiver. This page shows the pros and cons of each solution.

|                            **Description**                                          |    **fsarchiver**    |    **partimage**     |
|:------------------------------------------------------------------------------------|:--------------------:|:--------------------:|
| Ability to save/restore standard linux filesystems (ext2, ext3, reiserfs, xfs, jfs) | Yes                  | Yes                  |
| Ability to save/restore new generation linux filesystems (ext4, reiser4, btrfs)     | Yes                  | No                   |
| Ability to save/restore windows ntfs filesystems                                    | Yes (experimental)   | Yes (experimental)   |
| Requires kernel filesystem or fuse support for a filesystem to be supposed          | Yes                  | No                   |
| Ability to restore the filesystem to a partition which is smaller than the original | Yes                  | Requires resizefs    |
| Ability to restore the filesystem to a partition which is bigger than the original  | Yes                  | No                   |
| Requires filesystem tools such as mkfs to be installed to save the filesystem       | No                   | No                   |
| Requires filesystem tools such as mkfs to be installed to restore the filesystem    | Yes                  | No                   |
| Checksumming of the data and ability to restore corrupt archives                    | Yes                  | No                   |
| Compression algorithms which are supported                                          | lzo, gzip, bzip2, xz | gzip, bzip2          |
| Multi-threaded compression to make it faster on computers with multiple cores/cpu   | Yes                  | No                   |
| Ability to encrypt the data with a password                                         | Yes                  | No                   |
| Information taken into account to save the filesystem                               | Files                | Blocks               |
| User interface that comes with the program by default                               | Text                 | Semi-graphical       |
