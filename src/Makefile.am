sbin_PROGRAMS		= fsarchiver

fsarchiver_SOURCES	= fsarchiver.c dico.c common.c create.c extract.c md5.c \
	filesys.c archive.c archinfo.c uuid.c fs_ext2.c fs_reiserfs.c fs_reiser4.c \
	fs_btrfs.c fs_xfs.c fs_jfs.c comp_gzip.c comp_bzip2.c comp_lzma.c comp_lzo.c \
	thread_comp.c thread_archio.c queue.c writebuf.c error.c syncthread.c \
	crypto.c dichl.c fs_ntfs.c showpart.c strlist.c regmulti.c

noinst_HEADERS		= queue.h common.h comp_gzip.h dico.h fsarchiver.h \
	fs_ext2.h fs_reiser4.h fs_xfs.h filesys.h thread_archio.h uuid.h archive.h \
	comp_bzip2.h create.h extract.h  fs_btrfs.h fs_jfs.h fs_reiserfs.h md5.h \
	thread_comp.h types.h archinfo.h writebuf.h error.h syncthread.h \
	crypto.h  dichl.h  fs_ntfs.h showpart.h strlist.h regmulti.h comp_lzma.h \
	comp_lzo.h

fsarchiver_LDADD	= -lpthread -lrt
fsarchiver_CFLAGS	= @CFLAGS@ -Wall -std=gnu99 -rdynamic -ggdb
fsarchiver_LDFLAGS	= @FSARCHIVER_LDFLAGS@

DEFS=@DEFS@ -D_REENTRANT -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -D_GNU_SOURCE

MAINTAINERCLEANFILES	= Makefile.in
