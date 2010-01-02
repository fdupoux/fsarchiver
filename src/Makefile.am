sbin_PROGRAMS		= fsarchiver

fsarchiver_SOURCES	= fsarchiver.c create.c extract.c showpart.c filesys.c devinfo.c \
	thread_archio.c archreader.c archwriter.c writebuf.c archinfo.c \
	thread_comp.c comp_gzip.c comp_bzip2.c comp_lzma.c comp_lzo.c crypto.c \
	fs_ntfs.c fs_ext2.c fs_reiserfs.c fs_reiser4.c fs_btrfs.c fs_xfs.c fs_jfs.c \
	common.c dico.c dichl.c queue.c error.c syncthread.c md5.c \
	strlist.c regmulti.c options.c logfile.c

noinst_HEADERS		= fsarchiver.h create.h extract.h showpart.h filesys.h devinfo.h \
	thread_archio.h archreader.h archwriter.h writebuf.h archinfo.h \
	thread_comp.h comp_gzip.h comp_bzip2.h comp_lzma.h comp_lzo.h crypto.h \
	fs_ntfs.h fs_ext2.h fs_reiserfs.h fs_reiser4.h fs_btrfs.h fs_xfs.h fs_jfs.h \
	common.h dico.h dichl.h queue.h error.h syncthread.h md5.h \
	strlist.h regmulti.h options.h logfile.h types.h

fsarchiver_LDADD	= -lpthread -lrt
fsarchiver_CFLAGS	= @CFLAGS@ -Wall -std=gnu99 -rdynamic -ggdb
fsarchiver_LDFLAGS	= @FSARCHIVER_LDFLAGS@

DEFS=@DEFS@ -D_REENTRANT -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_GNU_SOURCE

MAINTAINERCLEANFILES	= Makefile.in
