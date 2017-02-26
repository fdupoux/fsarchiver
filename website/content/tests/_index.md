+++
weight = 400
title = "Testing"
nameInMenu = "Testing"
draft = false
+++

## Tests which have been made
Important tests are being done with FSArchiver to make sure it works as 
expected. Tests environments have been installed: different Linux distributions 
(CentOS-5.2, Fedora-8.0, Fedora-11, Ubuntu-9.04) have been installed using 
different file systems, in order to make sure FSArchiver can compile on various 
Linux flavors, and that it can save and restore on all these operating systems 
with no error. 

## How the basic test is done
Here is the **detailed test procedure** that we follow:

* install a Linux system with selinux enabled (when available) on /dev/sda1 (/boot) and /dev/sda2 (root filesystem)
* reboot the machine on a livecd with fsarchiver (the latest SystemRescueCd with FSArchiver on it)
* mount /dev/sda3 on /mnt/backup to store the FSArchiver archive.
* run **fsarchiver -o savefs /mnt/backup/backup-xxxx.fsa /dev/sda2**
* erase the root filesystem: **dd if=/dev/zero of=/dev/sda2 bs=1024k**
* run **fsarchiver restfs /mnt/backup/backup-xxxx.fsa id=0,dest=/dev/sda2**
* reboot on the linux system and test the system.

## Test with fedora and SELinux labels
During the development, **FSArchiver is tested with a Fedora-8** installation. 
FSArchiver-0.4.7 has been able to **successfully save and restore the ext3 root 
filesystem** of a Fedora-8 installation with selinux enabled. This fedora test 
system has been installed with selinux enabled just because selinux does file 
labelling on the filesystem using extended attributes, and then it's a good 
test to know whether or not these attributes have been preserved by FSArchiver. 
If you save and restore such a filesystem with a program that does not preserve 
the extended attributes, selinux will complain when you reboot, and it will 
make the system unusable. That's why this is a good test. 

## Test with rsync between two partitions
An important test is done using rsync. It requires two partitions: the original 
one, and a spare partition where to restore the archive. It allows to know 
whether or not there are differences between the original and the restored 
filesystem. rsync is able to compare both the files contents, and files 
attributes (timestamps, permissions, owner, extended attributes, acl, ...), so 
that's a very good test. The following command can be used to know whether or 
not files are the same (data and attributes) on two file-systems:
```
rsync -axHAXnP /mnt/part1/ /mnt/part2/
```

## Test with CentOS-3.9 and CentOS-4.7 on ext3
Tests with CentOS-3.9 and CentOS-4.7 have been done to make sure these quite 
old Linux systems are able to work on a filesystem restored by FSArchiver. When 
you restore a filesystem using FSArchiver, it may use a very recent version of 
mkfs to recreate the filesystem. There could be a problem with that. The ext3 
filesystem format has been improved over the time, and old Linux system such as 
CentOS-3.9 running linux-2.4 may not be able to mount the filesystem build with 
a recent mkfs.ext3. For that reason, FSArchiver preserves the filesystem 
attributes and features. In other words it excludes the recent ext3 features 
which may be enabled by default in mkfs.ext3 1.41. That way the filesystem is 
recreated with its original features, and the old kernel is able to uses this 
filesystem. Since CentOS is a RHEL clone, it means the test is also valid for 
that linux flavor. These tests also indicates that in general, the ext3 
filesystem attributes are preserved, and then it should work with all other 
linux distributions.

## How to see the extended attributes
Here is a command that show the extended attributes of your files:
```
# getfattr -P -d -R -h -m . /etc/
getfattr: Removing leading '/' from absolute path names
# file: etc
security.selinux="system_u:object_r:etc_t:s0\000"

# file: etc/ntp.conf
security.selinux="system_u:object_r:net_conf_t:s0\000"

# file: etc/rpm
security.selinux="system_u:object_r:etc_t:s0\000"

# file: etc/rpm/macros.kde4
security.selinux="system_u:object_r:etc_t:s0\000"

# file: etc/rpm/macros.specspo
security.selinux="system_u:object_r:etc_t:s0\000"
```

## Test for memory errors using Valgrind
fsarchiver is using dynamically allocated memory to carry data blocks from one 
thread to another. There is a risk to have allocated memory which is never 
freed. For that reason, fsarchiver is tested using valgrind. It tracks all the 
memory allocations (malloc and free) and prints a message when it detects 
memory leaks. Here is how to use valgrind that way:
```
#compile fsarchiver with the debugging option turned on in gcc (gcc -g)
#valgrind --tool=memcheck --leak-check=yes --show-reachable=yes --leak-resolution=med --track-origins=yes fsarchiver <fsarchiver-arguments>
```
