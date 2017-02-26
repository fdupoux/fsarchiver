+++
weight = 1100
title = "Attributes support"
nameInMenu = "Attributes"
draft = false
+++

## Preservation of attributes
FSArchiver has been written with the preservation of everything in mind. We want
to be sure that a filesystem saved with fsarchiver will keep both the filesystem
attributes (filesystem features, label, uuid) and all the file attributes
(contents, timestamps, owner, permissions, extended attributes, acl, ...). There
is no need to specify any option in fsarchiver to preserve these sort of things.
This is the default behaviour. The down side is you may have error about the
mount options when the extended-attributes or the ACLs are not visible.

## Mount options required
The standard attributes (permissions, owner, ...) are always visible.
Unfortunately, the extended-attributes and the ACL (which are stored as
extended-attributes) may not be visible by the programs when the file-system is
mounted with the wrong options. For example the ext3/ext4 filesystem may have
to be mounted with options "user_xattr" and "acl". It depends on the "default
mount options". If these required options are set as "default mount options"
in the superblock of the filesystem, then it's not necessary to specify these
options then you mount it, or when it's mounted via fstab. Here is an example
of a partition which has "acl" as a default mount option, but "user_xattr" is
not:
```
# dumpe2fs -h /dev/sda1 | grep -i "default mount options"
dumpe2fs 1.41.4 (27-Jan-2009)
Default mount options:    acl
```

If these mount options are not used, the risk is that extended attributes or
ACLs may have been written in the filesystem in the past, and these attributes
are now invisible because of the mount options. Then fsarchiver will warn about
it because it cannot save these attributes and they would be lost when you
restore the filesystem. Fortunately, this is an extreme case. In general, if the
mount option does not allow extended-attributes or ACL to be seen, it just means
you don't have that in the filesystem. The bad scenario may happen if you mount
a filesystem from different operating systems, or with different mount options
during the time.

## Errors about the mount-option and solutions
When you try to save a filesystem with fsarchiver, it will check that all the
required mount options that allow to see the extended-attributes and the ACLs
are ok. If they are not, it will complain with the following error message:
```
# fsarchiver savefs /mnt/archives/gentoo-backup-20090328-01.fsa /dev/sda1 -A
create.c#0674,filesystem_mount_partition(): partition [/dev/sda1] has to be mounted with 
       options [user_xattr] in order to preserve all its attributes. you can use mount 
       with option remount to do that.
create.c#0681,filesystem_mount_partition(): fsarchiver cannot continue, you can use 
       option '-a' to ignore the mount options (xattr or acl may not be preserved)
removing /mnt/archives/gentoo-backup-20090328-01.fsa
```

The first solution is to remount the partition using mount with the remount option:
```
mount -o remount,acl,user_xattr /dev/xxx
```
You can also decide to ignore this error if you have no such attributes on your
filesystem, or if you don't want it to be preserved. In that case you can just
run fsarchiver with the option **-a** and the operation will be able
to continue.
```
# fsarchiver savefs /mnt/archives/gentoo-backup-20090328-01.fsa /dev/sda1 -A -a
```

## SELinux (Security Enhanced Linux)
Several important Linux distribution support SELinux, which is a Security
Enhancement option that can be either enabled or disabled on your system. If
SELinux is enabled, it may have two impacts on fsarchiver:

### it can impact the programs that are running (including fsarchiver)
If SELinux is enabled when you save filesystems to an archive, you must check
that there is no restriction that may impact fsarchiver. So you can save a
filesystem on a system having SELinux enabled as long as it does not block
fsarchiver. But it's recommended to restore filesystems using fsarchiver from
an environment where SELinux is disabled or not supported. You can use
http://www.sysresccd.org[SystemRescueCd] which does not have SELinux enabled.
The problem with SELinux during a restoration is that it can create labels on
each file that is being restored, even if the original filesystem had no such
labels.

### its attributes have to be preserved (else you could have problems to boot)
If the operating system that you want to backup has SELinux enabled, it's
important to make sure that fsarchiver will preserve the SElinux labels, else
your operating system may not boot properly after a restoration. Fortunately,
this is the default behaviour: fsarchiver preserves all the file attributes by
default, including the extended attributes, and SELinux labels are implemented
as normal extended attributes. So fsarchiver is able to preserve the SELinux
labels on any system that supports the extended attributes, as long as it can
read and write the extended attributes on the filesystems where it is working.
