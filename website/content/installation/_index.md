+++
weight = 700
title = "Installation"
nameInMenu = "Installation"
draft = false
+++

## Installation
You can either download a livecd with FSArchiver on it (eg: any recent SystemRescueCd),
or you can install it on your existing Linux system. If you want to install it, you
have three solutions:

* use the official package that comes with your Linux distribution (recommended method)
* use the official static binary (very easy: you just have one standalone binary file to copy)
* compile fsarchiver from sources

### Compilation from sources
If you want to compile it from sources, you will need to have the following
libraries installed on your system, including their header files. On RPM based
distributions, you often have to install packages such as libXXX-devel to
have the header files, since the base package (libXXX) does not provide
these files. Here are the required libraries:

* zlib (and zlib-devel)
* liblzo (and lzo-devel)
* lz4 (and lz4-devel)
* bzip2-libs (and bzip2-devel)
* libgcrypt (and libgcrypt-devel)
* libblkid (and libblkid-devel)
* e2fsprogs-libs (e2fsprogs-devel)
* [xz-utils](http://tukaani.org/xz/) (and xz-devel)
* [zstd](https://facebook.github.io/zstd/) (and libzstd-devel)

You can selectively disable support for missing libraries or header files. For example,
on a system that lacks lzo and zstd:
```
./configure --prefix=/usr --disable-lzo --disable-zstd && make && make install
```

You can also use the static binary that provides support for all compression levels
and does not require the libraries to be installed on your system.

To compile the sources, you have to run follow these instructions:

#### Download the latest sources
First, you have to download [fsarchiver-0.8.5.tar.gz]
(https://github.com/fdupoux/fsarchiver/releases/download/0.8.5/fsarchiver-0.8.5.tar.gz).
Just save it to a temporary directory.

#### Extract the sources in a temporary directory
```
mkdir -p /var/tmp/fsarchiver
cd /var/tmp/fsarchiver
tar xfz /path/to/fsarchiver-x.y.z.tar.gz
```

#### Compile and install from sources
```
cd /var/tmp/fsarchiver/fsarchiver-x.y.z
./configure --prefix=/usr
make && make install
```

### Installation of the precompiled binary
If the compilation does not work, you can just download the
[static binary](https://github.com/fdupoux/fsarchiver/releases), extract it, and
copy the binary somewhere on your system (/usr/sbin for instance). You can use
either the 32 bit version (fsarchiver-static-x.y.z.i386.tar.gz) or the 64 bit one
(fsarchiver-static-x.y.z.x86_64.tar.gz).

Uninstalling fsarchiver is easy since it only installs one file on your system,
which is the compiled binary. To uninstall fsarchiver, just remove
/usr/sbin/fsarchiver.

## Dependencies
FSArchiver has two kind of dependencies: libraries and file-system tools.

### Libraries
Several **libraries with their header files** are necessary to compile the sources
(cf previous section about installation) and to execute the program if it has
not been compiled in a static way. You can avoid the problem with libraries
dependencies by using a static binary that you can download from the
[github release page](https://github.com/fdupoux/fsarchiver/releases).

### File-system tools
The **file-system tools** for the file-system you are using are required to
restore a file-system.
You should not have any problem with the file-system tools (such as a program
which is missing because it is not installed) when you save the file-system
(using **fsarchiver savefs**). You only need the file-system tools
(such as e2fsprogs, reiserfsprogs, xfsprogs, ...) when you want to restore a
file-system. And it should not really be a problem since you often want to
restore a file-system by booting from a livecd such as
[SystemRescueCd](http://www.system-rescue-cd.org), since you cannot restore your
root file-system when you are using it, so booting from a livecd / usb-stick is
mandatory in that case.

FSArchiver only requires the tools that match the file-system of the restoration.
For instance, if you are trying to restore an archive to a reiserfs partition,
you will need the reiserfsprogs to be available, even if the original filesystem
was ext3 when you saved the file-system to an archive.

## Distribution specific information
Using your distribution package manager ensures all necessary dependencies will
be automatically installed.

### Installation on Fedora (using dnf)
Package [information](https://apps.fedoraproject.org/packages/fsarchiver).
```
dnf install fsarchiver
```

### Installation on Ubuntu (using apt-get)
Package [information](https://packages.ubuntu.com/bionic/fsarchiver).
```
sudo apt-get update
sudo apt-get install fsarchiver
```

### Installation on Debian (using apt-get)
Package [information](https://packages.debian.org/stable/fsarchiver).
```
sudo apt-get update
sudo apt-get install fsarchiver
```

### Installation on openSUSE (using zypper)
Package [information](https://software.opensuse.org/package/fsarchiver).
```
zypper in fsarchiver
```

### Installation on Arch Linux (using pacman)
Package [information](https://www.archlinux.org/packages/extra/x86_64/fsarchiver/).
```
pacman -S fsarchiver
```

### Installation on RHEL / CentOS / Scientific Linux

You should use these RPM packages that have been built for RHEL7 based
distributions, that is the recommended way for most users:

* [fsarchiver-0.8.5-1.el7.x86_64.rpm](https://github.com/fdupoux/fsarchiver/releases/download/0.8.5/fsarchiver-0.8.5-1.el7.x86_64.rpm)

NOTE: Installing fsarchiver's rpm requires [EPEL](https://fedoraproject.org/wiki/EPEL) enabled.

Use the following commands to install:
```
yum install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
yum install https://github.com/fdupoux/fsarchiver/releases/download/0.8.5/fsarchiver-0.8.5-1.el7.x86_64.rpm
```

If you want to compile fsarchiver yourself, use the following instructions:
```
yum install zlib-devel bzip2-devel lzo-devel lz4-devel xz-devel e2fsprogs-devel libgcrypt-devel libattr-devel libblkid-devel
tar xfz fsarchiver-0.8.5.tar.gz
cd fsarchiver-0.8.5
./configure --prefix=/usr && make && make install
```

### Installation on Gentoo
Gentoo implements support for packages using ebuild files. There is an
[official ebuild for fsarchiver]
(http://packages.gentoo.org/packages/app-backup/fsarchiver), so you can directly
install it using the **emerge** command as long as your portage tree is recent.
You may also have to change the keywords to make the installation possible:

Add app-backup/fsarchiver to your /etc/portage/package.keywords if required:
```
echo "app-backup/fsarchiver ~*" >> /etc/portage/package.keywords
```

Change USE flags if you want non-default compilation options:
```
echo "app-backup/fsarchiver lzma lzo" >> /etc/portage/package.use
```

Run the installation command:
```
emerge app-backup/fsarchiver
```
