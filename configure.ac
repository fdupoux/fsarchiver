# Copyright (C) 2008-2009 Francois Dupoux
# Distributed under the GPL-2 license (GNU General Public License version 2)

AC_PREREQ(2.59)

AC_INIT([fsarchiver], 0.6.3-svn)
AC_DEFINE([PACKAGE_RELDATE], "YYYY-MM-DD", [Define the date of the release])
AC_DEFINE([PACKAGE_FILEFMT], "FsArCh_001", [Define the version of the file format])
AC_DEFINE([PACKAGE_VERSION_A], 0, [Major version number])
AC_DEFINE([PACKAGE_VERSION_B], 6, [Medium version number])
AC_DEFINE([PACKAGE_VERSION_C], 3, [Minor version number])
AC_DEFINE([PACKAGE_VERSION_D], 0, [Patch version number])

AC_CANONICAL_HOST([])
AC_CANONICAL_TARGET([])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([config.h])

AC_ISC_POSIX
AC_PROG_CC
AC_HEADER_STDC
AC_PROG_INSTALL
AM_PROG_CC_C_O
AC_SYS_LARGEFILE

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
    AC_CHECKING([for liblzma (library and header files)])
    AC_CHECK_LIB([lzma], [lzma_easy_encoder], [LIBS="$LIBS -llzma"],
        AC_MSG_ERROR([*** lzma library (liblzma) not found: you can either install xz-4.999.9beta.tar.gz (or newer) or disable lzma support using --disable-lzma]))
    AC_CHECK_HEADERS(lzma.h)
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

dnl option to disable crypto support (for people who don't have libgcrypt installed)
AC_ARG_ENABLE([crypto],
    [AS_HELP_STRING([--disable-crypto], [don't compile the support for encryption (which requires libgcrypt)])],
    [enable_crypto=$enableval],
    [enable_crypto=yes])
if test "x$enable_crypto" = "xyes"
then
    AC_DEFINE([OPTION_CRYPTO_SUPPORT], 1, [Define to 1 to enable the support for archive encryption using libgcrypt])
    AC_CHECKING([for libgcrypt (library and header files)])
    AC_CHECK_LIB([gcrypt], [gcry_cipher_encrypt], [LIBS="$LIBS -lgcrypt -lgpg-error"],
        AC_MSG_ERROR([*** libgcrypt not found: you can either install libgcrypt or disable the support for encryption using --disable-crypto]))
    AC_CHECK_HEADERS(gcrypt.h)
fi

dnl check e2fsprogs and its libs
AC_CHECKING([for e2fsprogs-1.39 or newer])
AC_CHECK_LIB([ext2fs], [ext2fs_open], [LIBS="$LIBS -lext2fs -lcom_err -lpthread"], AC_MSG_ERROR([*** ext2/ext3/ext4 library (libext2fs) not found]))
AC_CHECK_LIB([blkid], [blkid_dev_iterate_begin], [LIBS="$LIBS -lblkid"], AC_MSG_ERROR([*** libblkid not found (it comes with e2fsprogs sources but the package name may be different)]))
AC_CHECK_LIB([uuid], [uuid_generate], [LIBS="$LIBS -luuid"], AC_MSG_ERROR([*** libuuid not found (it comes with e2fsprogs sources but the package name may be different)]))
AC_CHECK_LIB([e2p], [e2p_mntopt2string], [LIBS="$LIBS -le2p"], AC_MSG_ERROR([*** libe2p not found (it comes with e2fsprogs sources but the package name may be different)]))
AC_CHECK_HEADERS(ext2fs/ext2fs.h blkid/blkid.h e2p/e2p.h)

dnl PKG_CHECK_MODULES([e2fsprogs], [ext2fs >= 1.39])

dnl Check for header files installed with a library
AC_CHECK_HEADER([attr/xattr.h],, [AC_MSG_ERROR([attr/xattr.h not found. you may have to install a package called attr, libattr, libattr-devel])])

dnl Check for standard header files.
AC_CHECK_HEADERS(malloc.h unistd.h pthread.h)
AC_CHECK_HEADERS(sys/param.h sys/statfs.h sys/mount.h)
AC_CHECK_HEADERS([stdint.h endian.h stdbool.h stdlib.h stdio.h getopt.h fcntl.h time.h wordexp.h execinfo.h])

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
