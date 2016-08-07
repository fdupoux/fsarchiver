/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2016 Francois Dupoux.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Homepage: http://www.fsarchiver.org
 */

#ifndef __FSARCHIVER_H__
#define __FSARCHIVER_H__

#include "types.h"

// ----------------------------- min and max -----------------------------
#if !defined(min)
#    define min(a, b)            ((a) < (b) ? (a) : (b))
#endif

#if !defined(max)
#    define max(a, b)            ((a) > (b) ? (a) : (b))
#endif

// -------------------------------- fsarchiver commands ---------------------------------------------
enum {OPER_NULL=0, OPER_SAVEFS, OPER_RESTFS, OPER_SAVEDIR, OPER_RESTDIR, OPER_ARCHINFO, OPER_PROBE};

// ----------------------------------- dico sections ------------------------------------------------
enum {DICO_OBJ_SECTION_STDATTR=0, DICO_OBJ_SECTION_XATTR=1, DICO_OBJ_SECTION_WINATTR=2};

// ----------------------------------- archive types ------------------------------------------------
enum {ARCHTYPE_NULL=0, ARCHTYPE_FILESYSTEMS, ARCHTYPE_DIRECTORIES};

// ----------------------------------- volume header and footer -------------------------------------
enum {VOLUMEHEADKEY_VOLNUM, VOLUMEHEADKEY_ARCHID, VOLUMEHEADKEY_FILEFORMATVER, VOLUMEHEADKEY_PROGVERCREAT};
enum {VOLUMEFOOTKEY_VOLNUM, VOLUMEFOOTKEY_ARCHID, VOLUMEFOOTKEY_LASTVOL};

// ----------------------------------- algorithms used to process data-------------------------------
enum {COMPRESS_NULL=0, COMPRESS_NONE, COMPRESS_LZO, COMPRESS_GZIP, COMPRESS_BZIP2, COMPRESS_LZMA};
enum {ENCRYPT_NULL=0, ENCRYPT_NONE, ENCRYPT_BLOWFISH};

// ----------------------------------- dico keys ----------------------------------------------------
enum {OBJTYPE_NULL=0, OBJTYPE_DIR, OBJTYPE_SYMLINK, OBJTYPE_HARDLINK, OBJTYPE_CHARDEV, 
      OBJTYPE_BLOCKDEV, OBJTYPE_FIFO, OBJTYPE_SOCKET, OBJTYPE_REGFILEUNIQUE, OBJTYPE_REGFILEMULTI};

enum {DISKITEMKEY_NULL=0, DISKITEMKEY_OBJECTID, DISKITEMKEY_PATH, DISKITEMKEY_OBJTYPE, 
      DISKITEMKEY_SYMLINK, DISKITEMKEY_HARDLINK, DISKITEMKEY_RDEV, DISKITEMKEY_MODE, 
      DISKITEMKEY_SIZE, DISKITEMKEY_UID, DISKITEMKEY_GID, DISKITEMKEY_ATIME, DISKITEMKEY_MTIME,
      DISKITEMKEY_MD5SUM, DISKITEMKEY_MULTIFILESCOUNT, DISKITEMKEY_MULTIFILESOFFSET,
      DISKITEMKEY_LINKTARGETTYPE, DISKITEMKEY_FLAGS};

enum {BLOCKHEADITEMKEY_NULL=0, BLOCKHEADITEMKEY_REALSIZE, BLOCKHEADITEMKEY_BLOCKOFFSET, 
      BLOCKHEADITEMKEY_COMPRESSALGO, BLOCKHEADITEMKEY_ENCRYPTALGO, BLOCKHEADITEMKEY_ARSIZE, 
      BLOCKHEADITEMKEY_COMPSIZE, BLOCKHEADITEMKEY_ARCSUM};

enum {BLOCKFOOTITEMKEY_NULL=0, BLOCKFOOTITEMKEY_MD5SUM};

enum {MAINHEADKEY_NULL=0, MAINHEADKEY_FILEFORMATVER, MAINHEADKEY_PROGVERCREAT, MAINHEADKEY_ARCHIVEID, 
      MAINHEADKEY_CREATTIME, MAINHEADKEY_ARCHLABEL, MAINHEADKEY_ARCHTYPE, MAINHEADKEY_FSCOUNT, 
      MAINHEADKEY_COMPRESSALGO, MAINHEADKEY_COMPRESSLEVEL, MAINHEADKEY_ENCRYPTALGO, 
      MAINHEADKEY_BUFCHECKPASSCLEARMD5, MAINHEADKEY_BUFCHECKPASSCRYPTBUF, MAINHEADKEY_FSACOMPLEVEL,
      MAINHEADKEY_MINFSAVERSION, MAINHEADKEY_HASDIRSINFOHEAD};

enum {FSYSHEADKEY_NULL=0, FSYSHEADKEY_FILESYSTEM, FSYSHEADKEY_MNTPATH, FSYSHEADKEY_BYTESTOTAL, 
      FSYSHEADKEY_BYTESUSED, FSYSHEADKEY_FSLABEL, FSYSHEADKEY_FSUUID, FSYSHEADKEY_FSINODESIZE, 
      FSYSHEADKEY_FSVERSION, FSYSHEADKEY_FSEXTDEFMNTOPT, FSYSHEADKEY_FSEXTREVISION, 
      FSYSHEADKEY_FSEXTBLOCKSIZE, FSYSHEADKEY_FSEXTFEATURECOMPAT, FSYSHEADKEY_FSEXTFEATUREINCOMPAT,
      FSYSHEADKEY_FSEXTFEATUREROCOMPAT, FSYSHEADKEY_FSREISERBLOCKSIZE, FSYSHEADKEY_FSREISER4BLOCKSIZE, 
      FSYSHEADKEY_FSXFSBLOCKSIZE, FSYSHEADKEY_FSBTRFSSECTORSIZE, FSYSHEADKEY_NTFSSECTORSIZE,
      FSYSHEADKEY_NTFSCLUSTERSIZE, FSYSHEADKEY_BTRFSFEATURECOMPAT, FSYSHEADKEY_BTRFSFEATUREINCOMPAT,
      FSYSHEADKEY_BTRFSFEATUREROCOMPAT, FSYSHEADKEY_NTFSUUID, FSYSHEADKEY_MINFSAVERSION,
      FSYSHEADKEY_MOUNTINFO, FSYSHEADKEY_ORIGDEV, FSYSHEADKEY_TOTALCOST,
      FSYSHEADKEY_FSEXTFSCKMAXMNTCOUNT, FSYSHEADKEY_FSEXTFSCKCHECKINTERVAL, 
      FSYSHEADKEY_FSEXTEOPTRAIDSTRIPEWIDTH, FSYSHEADKEY_FSEXTEOPTRAIDSTRIDE,
      FSYSHEADKEY_FSINODEBLOCKSPERGROUP, FSYSHEADKEY_FSXFSVERSION,
      FSYSHEADKEY_FSXFSFEATURECOMPAT, FSYSHEADKEY_FSXFSFEATUREROCOMPAT,
      FSYSHEADKEY_FSXFSFEATUREINCOMPAT, FSYSHEADKEY_FSXFSFEATURELOGINCOMPAT,
      FSYSHEADKEY_FSVFATTYPE, FSYSHEADKEY_FSVFATSERIAL};

enum {DIRSINFOKEY_NULL=0, DIRSINFOKEY_TOTALCOST};

// -------------------------------- fsarchiver errors ---------------------------------------------
enum {FSAERR_SUCCESS=0,           // success
      FSAERR_UNKNOWN=-1,          // uknown error (default code that means error)
      FSAERR_ENOMEM=-2,           // out of memory error
      FSAERR_EINVAL=-3,           // invalid parameter
      FSAERR_ENOENT=-4,           // entry not found
      FSAERR_ENDOFFILE=-5,        // end of file/queue
      FSAERR_WRONGTYPE=-6,        // wrong type of data
      FSAERR_NOTOPEN=-7,          // resource has been closed
      FSAERR_ENOSPC=-8,           // no space left on device
      FSAERR_SEEK=-9,             // lseek64 error
      FSAERR_READ=-10,            // read error
      FSAERR_WRITE=-11            // write error
};

// -------------------------------- old errors codes ---------------------------------------------
enum {OLDERR_FATAL=1,
      OLDERR_MINOR=2};

// ----------------------------- fsarchiver const ------------------------
#define FSA_VERSION              PACKAGE_VERSION
#define FSA_RELDATE              PACKAGE_RELDATE
#define FSA_FILEFORMAT           PACKAGE_FILEFMT

#define FSA_GCRYPT_VERSION       "1.2.3"

#define FSA_MAX_FILEFMTLEN       32
#define FSA_MAX_PROGVERLEN       32
#define FSA_MAX_FSNAMELEN        128
#define FSA_MAX_DEVLEN           256
#define FSA_MAX_UUIDLEN          128
#define FSA_MAX_BLKDEVICES       256

#define FSA_MAX_FSPERARCH        128
#define FSA_MAX_COMPJOBS         32
#define FSA_MAX_QUEUESIZE        32
#define FSA_MAX_BLKSIZE          921600
#define FSA_DEF_BLKSIZE          262144
#define FSA_DEF_COMPRESS_ALGO    COMPRESS_GZIP  // compress using gzip by default
#define FSA_DEF_COMPRESS_LEVEL   6              // compress with "gzip -6" by default
#define FSA_MAX_SMALLFILECOUNT   512            // there can be up to FSA_MAX_SMALLFILECOUNT files copied in a single data block 
#define FSA_MAX_SMALLFILESIZE    131072         // files smaller than that will be grouped with other small files in a single data block
#define FSA_COST_PER_FILE        16384          // how much it cost to copy an empty file/dir/link: used to eval the progress bar

#define FSA_MAX_LABELLEN         512
#define FSA_MIN_PASSLEN          6
#define FSA_MAX_PASSLEN          64

#define FSA_FILESYSID_NULL       0xFFFF
#define FSA_CHECKPASSBUF_SIZE    4096

#define FSA_FILEFLAGS_SPARSE     1<<0           // set when a regfile is a sparse file

// ----------------------------- fsarchiver magics --------------------------------------------------
#define FSA_SIZEOF_MAGIC         4
#define FSA_MAGIC_VOLH           "FsA0" // volume header (one per volume at the very beginning)
#define FSA_MAGIC_VOLF           "FsAE" // volume footer (one per volume at the very end)
#define FSA_MAGIC_MAIN           "ArCh" // archive header (one per archive at the beginning of the first volume)
#define FSA_MAGIC_FSIN           "FsIn" // filesys info (one per filesystem at the beginning of the archive)
#define FSA_MAGIC_FSYB           "FsYs" // filesys begin (one per filesystem when the filesys contents start)
#define FSA_MAGIC_DIRS           "DiRs" // dirs info (one per archive after mainhead before flat dirs/files)
#define FSA_MAGIC_OBJT           "ObJt" // object header (one per object: regfiles, dirs, symlinks, ...)
#define FSA_MAGIC_BLKH           "BlKh" // datablk header (one per data block, each regfile may have [0-n])
#define FSA_MAGIC_FILF           "FiLf" // filedat footer (one per regfile, after the list of data blocks)
#define FSA_MAGIC_DATF           "DaEn" // data footer (one per file system, at the end of its contents, or after the contents of the flatfiles)

// ------------ global variables ---------------------------
extern char *valid_magic[];

// -------------------------------- version_number to u64 -------------------------------------------
#define FSA_VERSION_BUILD(a, b, c, d)     ((u64)((((u64)a&0xFFFF)<<48)+(((u64)b&0xFFFF)<<32)+(((u64)c&0xFFFF)<<16)+(((u64)d&0xFFFF)<<0)))
#define FSA_VERSION_GET_A(ver)            ((((u64)ver)>>48)&0xFFFF)
#define FSA_VERSION_GET_B(ver)            ((((u64)ver)>>32)&0xFFFF)
#define FSA_VERSION_GET_C(ver)            ((((u64)ver)>>16)&0xFFFF)
#define FSA_VERSION_GET_D(ver)            ((((u64)ver)>>0)&0xFFFF)

#endif // __FSARCHIVER_H__
