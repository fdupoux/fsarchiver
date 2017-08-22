# Copyright (C) 2008-2010 Francois Dupoux
# Distributed under the GPL-2 license (GNU General Public License version 2)

AC_PREREQ(2.59)

AC_INIT([fsarchiver], 0.8.3-git)
AC_DEFINE([PACKAGE_RELDATE], "YYYY-MM-DD", [Define the date of the release])
AC_DEFINE([PACKAGE_FILEFMT], "FsArCh_002", [Define the version of the file format])
AC_DEFINE([PACKAGE_VERSION_A], 0, [Major version number])
AC_DEFINE([PACKAGE_VERSION_B], 8, [Medium version number])
AC_DEFINE([PACKAGE_VERSION_C], 3, [Minor version number])
AC_DEFINE([PACKAGE_VERSION_D], 0, [Patch version number])

AC_CANONICAL_HOST([])
AC_CANONICAL_TARGET([])
AM_INIT_AUTOMAKE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_CONFIG_HEADER([config.h])

AC_ISC_POSIX
AC_PROG_CC
AC_HEADER_STDC
AC_PROG_INSTALL
AM_PROG_CC_C_O
AC_SYS_LARGEFILE
PKG_PROG_PKG_CONFIG

dnl =======================================================================================
dnl checks for os/architecture specificities.
case "$target_os" in
  linux*)
    AC_DEFINE([OS_LINUX], 1, [Define to 1 if OS is Linux])
    AC_CHECK_LIB([pthread], [pthread_create], [],
       AC_MSG_ERROR([*** pthread library (libpthread) not found]))
    ;;
  *)
    AC_MSG_ERROR([Unsupported system type $target_os])
    ;;
esac

dnl =======================================================================================
AC_CHECKING([for libz (library and header files)])
AC_CHECK_LIB([z], [compress], [LIBS="$LIBS -lz"],
    AC_MSG_ERROR([*** gzip library (libz) not found (you may also have to install zlib-devel)]))
AC_CHECK_HEADERS(zlib.h)

AC_CHECKING([for libbz2 (library and header files) ... ])
AC_CHECK_LIB([bz2], [BZ2_bzBuffToBuffCompress], [LIBS="$LIBS -lbz2"],
    AC_MSG_ERROR([*** bzip2 library (libbz2) not found or too old (you may also have to install bzip2-devel)]))
AC_CHECK_HEADERS(bzlib.h)

dnl option to disable lzma support (for people who don't have xz installed)
AC_ARG_ENABLE([lzma],
    [AS_HELP_STRING([--disable-lzma], [don't compile the support for lzma compression (which requires liblzma)])],
    [enable_lzma=$enableval],
    [enable_lzma=yes])
if test "x$enable_lzma" = "xyes"
then
    AC_DEFINE([OPTION_LZMA_SUPPORT], 1, [Define to 1 to enable the support for lzma compression])
    PKG_CHECK_MODULES([LZMA], [liblzma])
fi

dnl option to disable lzo support (for people who don't have liblzo2 installed)
AC_ARG_ENABLE([lzo],
    [AS_HELP_STRING([--disable-lzo], [don't compile the support for lzo compression (which requires liblzo2)])],
    [enable_lzo=$enableval],
    [enable_lzo=yes])
if test "x$enable_lzo" = "xyes"
then
    AC_DEFINE([OPTION_LZO_SUPPORT], 1, [Define to 1 to enable the support for lzo compression])
    AC_CHECKING([for liblzo2 (library and header files)])
    AC_CHECK_LIB([lzo2], [lzo1x_1_compress], [LIBS="$LIBS -llzo2"],
        AC_MSG_ERROR([*** lzo library (liblzo2) not found: please install liblzo (you may also have to install lzo-devel) or disable lzo support using --disable-lzo]))
    AC_CHECK_HEADERS(lzo/lzo1x.h)
fi

dnl check libgcrypt (required for crypto and md5)
AC_CHECKING([for libgcrypt (library and header files)])
AC_CHECK_LIB([gcrypt], [gcry_cipher_encrypt], [LIBS="$LIBS -lgcrypt -lgpg-error"], AC_MSG_ERROR([*** libgcrypt not found]))
AC_CHECK_HEADERS(gcrypt.h)

dnl check e2fsprogs and its libs
PKG_CHECK_EXISTS([ext2fs < 1.41.2], [CFLAGS="$CFLAGS -I /usr/include/blkid -I /usr/include/ext2fs -I /usr/include/uuid -I /usr/include/e2p"])
PKG_CHECK_MODULES([EXT2FS], [ext2fs])
PKG_CHECK_MODULES([COM_ERR], [com_err])
PKG_CHECK_MODULES([E2P], [e2p])
PKG_CHECK_MODULES([BLKID], [blkid])
PKG_CHECK_MODULES([UUID], [uuid])

dnl Check for header files installed with a library
AC_CHECK_HEADER([attr/xattr.h],, [AC_MSG_ERROR([attr/xattr.h not found. you may have to install a package called attr, libattr, libattr-devel])])

dnl Check for standard header files.
AC_CHECK_HEADERS([malloc.h unistd.h pthread.h])
AC_CHECK_HEADERS([sys/param.h sys/statfs.h sys/mount.h])
AC_CHECK_HEADERS([stdint.h endian.h stdbool.h stdlib.h stdio.h getopt.h fcntl.h time.h wordexp.h execinfo.h fnmatch.h])

dnl Check for library functions.
AC_CHECK_FUNCS(strerror open64 lstat64 stat64 fstatfs64 fstatvfs64 mempcpy lutimes)

# checks for header files.
AC_HEADER_STDC

dnl =======================================================================================
dnl with-log-dir option.
AC_ARG_WITH(log-dir,
    [  --with-log-dir=PATH     logfiles [/var/log]],
    LOGDIR=$with_log_dir, LOGDIR=/var/log)
    AC_SUBST(LOGDIR)

dnl options for developers
AC_ARG_ENABLE(devel,
    [  --enable-devel          enable options for developers (debug, ...)],
    AC_DEFINE([OPTION_DEVEL_SUPPORT], 1, [Define to 1 to enable options for development]),)

dnl static executables?
AC_ARG_ENABLE(static,
    [  --enable-static         build static binaries], allstatic=$enableval, allstatic=no)

if test "$allstatic" = "yes"
then
  FSARCHIVER_LDFLAGS="${FSARCHIVER_LDFLAGS} -static"
  AC_SUBST(FSARCHIVER_LDFLAGS)
fi

dnl with-debug-level option.
AC_ARG_WITH(debug-level,
  [  --with-debug-level=X    default debut level used to write the debug logfile [1]],
   DEFAULT_DEBUG_LEVEL=$with_debug_level, DEFAULT_DEBUG_LEVEL=1
)
AC_SUBST(DEFAULT_DEBUG_LEVEL)

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile])

AC_OUTPUT
