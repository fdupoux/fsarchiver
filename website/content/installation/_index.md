+++
weight = 700
title = "Installation"
nameInMenu = "Installation"
draft = false
+++

## Installation
There are two ways to use FSArchiver. You can either download a livecd with this
program on it (eg: any recent SystemRescueCd), or you can install it on a linux
system on your computer. If you want to install FSArchiver, you have three
solutions:

* you can use the official fsarchiver package that comes with your Linux
distribution (recommended method)
* you can just copy the official static binary (very easy: you just have one
standalone binary file to copy)
* you can compile fsarchiver from sources

### Compilation from sources
If you want to compile it from sources, you will need to have the following
libraries installed on your system, including their header files. On RPM based
distributions, you often have to install packages such as libXXX-devel.rpm to
have the header files, since the base package (libXXX.rpm) does not provide
these files. Here are the required libraries:

* zlib (and zlib-devel)
* liblzo (and lzo-devel)
* bzip2-libs (and bzip2-devel)
* libgcrypt (and libgcrypt-devel)
* libblkid (and libblkid-devel)
* e2fsprogs-libs (e2fsprogs-devel)
* [xz-utils](http://tukaani.org/xz/) (and xz-devel)

If libraries or header files are missing: If you do not have the xz/liblzma or
liblzo installed, you have the ability to compile fsarchiver with no support for
lzma or lzo:
```
./configure --prefix=/usr --disable-lzma --disable-lzo && make && make install
```

You can also use the static that provides support for all the compression levels
and which does not require the libraries to be installed on your system.

To compile the sources, you have to run follow these instructions:

#### download the latest sources
First, you have to download [fsarchiver-0.8.2.tar.gz]
(https://github.com/fdupoux/fsarchiver/releases/download/0.8.2/fsarchiver-0.8.2.tar.gz).
Just save it to a temporary directory.

#### extract the sources in a temporary directory
```
mkdir -p /var/tmp/fsarchiver
cd /var/tmp/fsarchiver
tar xfz /path/to/fsarchiver-x.y.z.tar.gz
```

#### compile and install from sources
``` 
cd /var/tmp/fsarchiver/fsarchiver-x.y.z
./configure --prefix=/usr
make && make install
```
### Installation of the precompiled binary

If the compilation does not work, you can just download the
[static binary](https://github.com/fdupoux/fsarchiver/releases), extract it, and
copy the binary somewhere on your system (/usr/sbin for instance). You can use
either the 32bit version (fsarchiver-static-x.y.z.i386.tar.gz) or the 64bit one
(fsarchiver-static-x.y.z.x86_64.tar.gz).

Uninstalling fsarchiver is easy since it only installs one file on your system,
which is the compiled binary. To uninstall fsarchiver, just remove
/usr/sbin/fsarchiver.

## Dependencies
FSArchiver has to kind of dependencies: libraries and file-system tools.

### libraries dependencies
Several **libraries with their header files** are necessary to compile the sources
(cf previous section about installation) and to execute the program if it has
not been compiled in a static way. You can avoid the problem with libraries
dependencies by using a static binary that you can download from the
[github release page](https://github.com/fdupoux/fsarchiver/releases).

### file-system tools required
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
### installation on fedora (using yum)
Fedora provide an [official package for fsarchiver](http://koji.fedoraproject.org/koji/packageinfo?packageID=7733)
where all the optional features are enabled (lzma compression, encryption, ...).
You can just use the following command to download and install fsarchiver and
all its dependencies.
```
yum install fsarchiver
```
### installation on ubuntu (using apt-get)
Ubuntu has an [official fsarchiver package](http://packages.ubuntu.com/xenial/fsarchiver)
in [the universe repository](https://help.ubuntu.com/community/Repositories/Ubuntu).
It can be installed with the standard apt-get command:
```
sudo apt-get update
sudo apt-get install fsarchiver
```

### installation on debian (using apt-get)
Thanks to Michael Biebl there is now a [fsarchiver deb package for debian](https://packages.debian.org/stable/fsarchiver).
You can just install it with the normal command:
```
sudo apt-get update
sudo apt-get install fsarchiver
```

### installation on RHEL / CentOS / Scientific-Linux
You should use these RPM packages that have been built for RHEL7 based
distributions, that is the recommended way for most users:

* [fsarchiver-0.8.2-1.el7.x86_64.rpm](https://github.com/fdupoux/fsarchiver/releases/download/0.8.2/fsarchiver-0.8.2-1.el7.x86_64.rpm)

If you want to recompile fsarchiver yourself, you can use the following instructions:

Here are the instructions to install the required libraries and to compile the
sources on RHEL / CentOS / Scientific-Linux:
```
yum install zlib-devel bzip2-devel lzo-devel xz-devel e2fsprogs-devel libgcrypt-devel libattr-devel libblkid-devel
tar xfz fsarchiver-0.8.2.tar.gz
cd fsarchiver-0.8.2
./configure --prefix=/usr && make && make install
```

### installation on arch-linux (using pacman)
There is now an official package for fsarchiver in ArchLinux: for both
[i686](http://www.archlinux.org/packages/extra/i686/fsarchiver/) and
[x86_64](http://www.archlinux.org/packages/extra/x86_64/fsarchiver/).
You can install it with the following command:
```
pacman -S fsarchiver
```

### installation on SUSE (using zypper)
* There is an
[official fsarchiver package](https://software.opensuse.org/package/fsarchiver?search_term=fsarchiver)
for Suse.

* There are three ways to install software in SUSE:
  * First, following the link above with **one click install** -- simplest way.
  * Second, add additional repository and install with GUI-tool YaST.
  * Third, add additional repository and install with CLI-tool zypper.
* To add a repository with the CLI: **zypper ar repository-url**
* To install fsarchiver with the CLI: **zypper in fsarchiver**

### installation on gentoo
Gentoo implements the support for packages using ebuild files. There is now an
[official ebuild for fsarchiver]
(http://packages.gentoo.org/packages/app-backup/fsarchiver) in gentoo, so you
can directly install it using the **emerge** command as long as your portage
tree is recent. You may also have to change the keywords to make the
installation possible:

Add app-backup/fsarchiver to your /etc/portage/package.keywords if required:

```
echo "app-backup/fsarchiver ~*" >> /etc/portage/package.keywords
```
Change the USE if you want non-default compilation options:

```
echo "app-backup/fsarchiver lzma lzo" >> /etc/portage/package.use
```
Run the installation command:

```
emerge app-backup/fsarchiver
```
