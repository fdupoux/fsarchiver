+++
weight = 200
title = "ChangeLog"
nameInMenu = "ChangeLog"
draft = false
+++

* **0.8.2 (2017-08-22):**
  * Add support for the latest ext4 filesystem features
  * Improved support of large block device when restoring extfs filesystems
* **0.8.1 (2017-01-10):**
  * Improved support for XFS filesystem (contributions from Marcos Mello)
  * Updated documentation and comments in the sources (Marcos Mello)
* **0.8.0 (2016-08-09):**
  * Implemented FAT filesystem support for EFI system partitions
  * Allow user to specify new filesystem label or UUID during restfs
  * Fixed more errors and warnings reported by cppcheck
* **0.6.24 (2016-08-07):**
  * Updated man page and description of the commands and options
  * Support for sparse inode chunks on XFS v5
  * Avoid internationalization when running commands so mkfs output can be parsed properly
* **0.6.23 (2016-06-16):**
  * Added micro-seconds to timestamp used in the name of the temporary directory
  * Fixed memory leaks on failure scenarios and protect against buffer overflows in scanf
  * Fixed possible failure to restore ext4 filesystems when mkfs running in interactive mode
  * Fixed compilation errors with the musl libc library
* **0.6.22 (2016-02-13):**
  * Test support for extended attributes and ACLs instead of checking mount options
* **0.6.21 (2016-01-07):**
  * Removed duplicate types definitions in order to fix compilation errors
  * Attempt to unmount a device four times before failing
* **0.6.20 (2016-01-03):**
  * Detect version of XFS filesystems in order to preserve it when they are restored
  * Add support for XFS filesystems features introduced in XFS version 5
  * Make sure the UUIDs of XFS filesystems are always preserved
* **0.6.19 (2014-03-01):**
  * Reverted "number of inode blocks per group" patch which caused a regression in release 0.6.18
* **0.6.18 (2014-02-13):**
  * Prepared release sources using autoconf-2.69 to add support for new architectures (RHBZ#925370)
  * Applied patch from Berix to preserve the number of inode blocks per group on ext filesystems
  * Added support for recent btrfs features (up to linux-3.14)
  * Run mkfs.btrfs with option "-f" so that it does not fail on devices with pre-existing filesystems
* **0.6.17 (2013-02-25):**
  * Implemented "mkfsopt" restfs option to pass extra options to mkfs (Michael Moninski)
  * Fixed parsing of "/proc/self/mountinfo" (mount options were not parsed on new systems)
  * Removed the ebuild for gentoo as fsarchiver is now in the official portage tree
* **0.6.16 (2013-02-07):**
  * Fixed mke2fs requirement for the "ext_attr" feature (affects RHLE5/CentOS5)
  * Fixed parsing of "/proc/self/mountinfo" for systemd based systems
* **0.6.15 (2012-06-02):**
  * Added support for recent ext4 features (up to linux-3.4 / e2fsprogs-1.42)
* **0.6.14 (2012-06-02):**
  * Added support for recent btrfs features (up to linux-3.4)
  * Fixed compilation warnings (variable ‘res’ set but not used)
* **0.6.13 (2012-03-04):**
  * Fixed detection of the root filesystem using "/proc/self/mountinfo" instead of "/proc/mounts"
* **0.6.12 (2010-12-25):**
  * Fix: get correct mount info for root device when not listed in /proc/mounts (eg: missing "/dev/root")
* **0.6.11 (2010-12-01):**
  * Updated supported btrfs compat flags to make it work with btrfs-2.6.35
  * The -c/--cryptpass option now supports interactive passwords: use "-c -"
* **0.6.10 (2010-05-09):**
  * Fixed support of symbolic links on ntfs filesystems with ntfs3g >= 2010.3.6
*  *0.6.9 (2010-05-04):**
  * Fix in probe: show devmapper/lvm volumes even when /dev/dm-xx does not exist
  * Fixed restoration of very small archives (archive < 4K)
  * Fix error handling in restoration: consider ENOSPC as a fatal error and other fixes
  * Fix: remove all volumes of the archive instead of just the first one if save fails
* **0.6.8 (2010-02-20):**
  * Fixed compilation error on systems with recent kernel headers by including `<sys/stat.h>`
  * Fixed critical bug: there was a risk of corruption when the archive was written on a smbfs/cifs filesystem
* **0.6.7 (2010-01-31):**
  * Added support for sparse files (sparse file on the original disk will be recreated as sparse file)
  * Added per-archive minimum fsarchiver version requirement (MAINHEADKEY_MINFSAVERSION)
  * Added dirsinfo in archives with simple files and directories to store stats required for progression
  * The logfile created when option -d is used now has a specific name so that it's not overwritten
  * The md5 checksums are now calculated using the implementation from libgcrypt instead of the internal one
  * The libgcrypt library is now a mandatory dependency and crypto cannot be disabled any more
  * Allow non root users to use "fsarchiver archinfo" as long as they have read permissions on the archive
* **0.6.6 (2010-01-24):**
  * Fix: don't remove the archive file when savefs/savedir fails because the archive already exists
  * Partitions already mounted are remounted with MS_BIND to have access to files hidden by mounted filesystems
  * Analyse filesystems only when they are all accessible to prevent having to wait and then get an error
  * Moved management of data files (open/write/md5sum) from extract.c to a separate object (datafile.c)
  * Important internal changes, renaming of functions/files, and simplifications for better consistency
  * Rephrased and simplified messages and other improvements in fsarchiver.c (contribution from dgerman)
  * Fixed potential memory error in savefs/savedir with extended attributes (bug reported by mbiebl)
* **0.6.5 (2010-01-07):**
  * Fixed compilation issues (pkg-config problems especially on systems with e2fsprogs < 1.41.2)
  * Retry with the default level (gzip -6) when compression of a data block lacks memory with bzip2/lzma
* **0.6.4 (2010-01-03):**
  * Improved the manpage: documented the long options, added examples, links, ...
  * Reorganized code of the archive-io thread (split archive into archreader and archwriter)
  * Fixed critical bug: integer overflow for "u16 headerlen" when sum of attributes size > 65535
  * Introduced new fileformat: "FsArCh_002", but old format "FsArCh_001" is also supported
  * Using code from libblkid instead of complex implementation to read ntfs labels
  * Switch to pkg-config in configure.ac and Makefile.am (contribution from Michael Biebl)
* **0.6.3 (2009-12-28):**
  * Don't fail when e2fsck returns 1 in extfs_mkfs() since it means the filesystem has been fixed
  * Extended options (stride and stripe_width), max_mount_count, check_interval are now preserved for ext{2,3,4}
  * Display the percentage of the operation which has been completed when verbose >= 1 (sort of progress bar)
  * Display information about physical disks as well as partitions in `fsarchiver probe <mode>`
  * Fixed bug with archive splitting: the split size was sometimes incorrect due to an integer overflow
  * Added option `--exclude/-e <pattern>` to exclude files/dirs. It works for both archiving and extracting.
  * Added support for long options (--option) using getopt_long
  * Removed the dependency on "which" to find the path to a program
  * Accept all libgcrypt versions >= 1.2.4 at runtime in gcry_check_version
  * Using functions from libuuid instead of duplicating code in uuid.[ch]
* **0.6.2 (2009-12-08):**
  * Dynamic memory allocation for ntfs specific extended-attributes in create.c
  * Fix related the the ntfs attributes when lgetxattr returns a negative size
  * Saves the name of the original device where the filesystem is stored (FSYSHEADKEY_ORIGDEV)
  * Fixed enable options in configure: "--enable-xxx" had the opposite effect (Thanks to horhe)
  * Exit with an error if the user wants to use a compression level which is not supported (Thanks to mbiebl)
  * Fixed crash when mount fails (mntbyfsa was set to true and not clear if mount fails)
  * Fixed code for ntfs symlinks (they have to be recreated as normal files and dirs + special attributes)
  * Changed the requirement from ntfs3g-AR (advanced release) to ntfs3g >= 20091114 (standard release) for ntfs
  * Improvements and fixes in the autotools build chain files (contribution from Michael Biebl)
  * Added option "-L" to specify the label of the archive: it's just a comment about the contents
  * Detabified the sources: find . -iname "*.[ch]" -exec sed -i -e "s/[\t]/    /g" -e 's!{    !{   !g' {} \;
* **0.6.1 (2009-10-04):**
  * New encryption implementation was not thread-safe (broken when option -j was used)
  * Dropped openssl support (this code was disabled in fsarchiver-0.6.0 anyway)
* **0.6.0 (2009-09-27):**
  * Debianized fsarchiver (added the debian directory necessary to build the debian package)
  * Rewrote the encryption support using libgcrypt instead of openssl (fix licensing issues)
  * Added the manpage written by Ilya Barygin (it will be installed by "make install")
