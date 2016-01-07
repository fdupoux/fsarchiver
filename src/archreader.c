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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <assert.h>

#include "fsarchiver.h"
#include "dico.h"
#include "common.h"
#include "options.h"
#include "archreader.h"
#include "queue.h"
#include "comp_gzip.h"
#include "comp_bzip2.h"
#include "error.h"

int archreader_init(carchreader *ai)
{
    assert(ai);
    memset(ai, 0, sizeof(struct s_archreader));
    ai->cryptalgo=ENCRYPT_NULL;
    ai->compalgo=COMPRESS_NULL;
    ai->fsacomp=-1;
    ai->complevel=-1;
    ai->archfd=-1;
    ai->archid=0;
    ai->curvol=0;
    ai->filefmtver=0;
    ai->hasdirsinfohead=false;
    return 0;
}

int archreader_destroy(carchreader *ai)
{
    assert(ai);
    return 0;
}

int archreader_open(carchreader *ai)
{   
    struct stat64 st;
    char volhead[64];
    int magiclen;
    
    assert(ai);
    
    // on the archive volume
    ai->archfd=open64(ai->volpath, O_RDONLY|O_LARGEFILE);
    if (ai->archfd<0)
    {   sysprintf ("cannot open archive %s\n", ai->volpath);
        return -1;
    }
    
    // check the archive volume is a regular file
    if (fstat64(ai->archfd, &st)!=0)
    {   sysprintf("fstat64(%s) failed\n", ai->volpath);
        return -1;
    }
    if (!S_ISREG(st.st_mode))
    {   errprintf("%s is not a regular file, cannot continue\n", ai->volpath);
        close(ai->archfd);
        return -1;
    }
    
    // read file format version and rewind to beginning of the volume
    if (read(ai->archfd, volhead, sizeof(volhead))!=sizeof(volhead))
    {   sysprintf("cannot read magic from %s\n", ai->volpath);
        close(ai->archfd);
        return -1;
    }
    if (lseek64(ai->archfd, 0, SEEK_SET)!=0)
    {   sysprintf("cannot rewind volume %s\n", ai->volpath);
        close(ai->archfd);
        return -1;
    }
    
    // interpret magic an get file format version
    magiclen=strlen(FSA_FILEFORMAT);
    if ((memcmp(volhead+40, "FsArCh_001", magiclen)==0) || (memcmp(volhead+40, "FsArCh_00Y", magiclen)==0))
    {
        ai->filefmtver=1;
    }
    else if (memcmp(volhead+42, "FsArCh_002", magiclen)==0)
    {
        ai->filefmtver=2;
    }
    else
    {
        errprintf("%s is not a supported fsarchiver file format\n", ai->volpath);
        close(ai->archfd);
        return -1;
    }
    
    msgprintf(MSG_VERB2, "Detected fileformat=%d in archive %s\n", (int)ai->filefmtver, ai->volpath);
    
    return 0;
}

int archreader_close(carchreader *ai)
{
    assert(ai);
    
    if (ai->archfd<0)
        return -1;
    
    lockf(ai->archfd, F_ULOCK, 0);
    close(ai->archfd);
    ai->archfd=-1;

    return 0;
}

int archreader_volpath(carchreader *ai)
{
    int res;
    res=get_path_to_volume(ai->volpath, PATH_MAX, ai->basepath, ai->curvol);
    return res;
}

int archreader_incvolume(carchreader *ai, bool waitkeypress)
{
    assert(ai);
    ai->curvol++;
    return archreader_volpath(ai);
}

int archreader_read_data(carchreader *ai, void *data, u64 size)
{
    long lres;
    
    assert(ai);

    if ((lres=read(ai->archfd, (char*)data, (long)size))!=(long)size)
    {   sysprintf("read failed: read(size=%ld)=%ld\n", (long)size, lres);
        return -1;
    }
    
    return 0;
}

int archreader_read_dico(carchreader *ai, cdico *d)
{
    u16 size;
    u32 headerlen;
    u32 origsum;
    u32 newsum;
    u8 *buffer;
    u8 *bufpos;
    u16 temp16;
    u32 temp32;
    u8 section;
    u16 count;
    u8 type;
    u16 key;
    int i;
    
    assert(ai);
    assert(d);
    
    // header-len, header-data, header-checksum
    switch (ai->filefmtver)
    {
        case 1:
            if (archreader_read_data(ai, &temp16, sizeof(temp16))!=0)
            {   errprintf("imgdisk_read_data() failed\n");
                return OLDERR_FATAL;
            }
            headerlen=le16_to_cpu(temp16);
            break;
        case 2:
            if (archreader_read_data(ai, &temp32, sizeof(temp32))!=0)
            {   errprintf("imgdisk_read_data() failed\n");
                return OLDERR_FATAL;
            }
            headerlen=le32_to_cpu(temp32);
            break;
        default:
            errprintf("Fatal error: invalid file format version: ai->filefmtver=%d\n", ai->filefmtver);
            return OLDERR_FATAL;
    }
    
    bufpos=buffer=malloc(headerlen);
    if (!buffer)
    {   errprintf("cannot allocate memory for header\n");
        return FSAERR_ENOMEM;
    }
    
    if (archreader_read_data(ai, buffer, headerlen)!=0)
    {   errprintf("cannot read header data\n");
        free(buffer);
        return OLDERR_FATAL;
    }
    
    if (archreader_read_data(ai, &temp32, sizeof(temp32))!=0)
    {   errprintf("cannot read header checksum\n");
        free(buffer);
        return OLDERR_FATAL;
    }
    origsum=le32_to_cpu(temp32);
    
    // check header-data integrity using checksum    
    newsum=fletcher32(buffer, headerlen);
    
    if (newsum!=origsum)
    {   errprintf("bad checksum for header\n");
        free(buffer);
        return OLDERR_MINOR; // header corrupt --> skip file
    }
    
    // read count from buffer
    memcpy(&temp16, bufpos, sizeof(temp16));
    bufpos+=sizeof(temp16);
    count=le16_to_cpu(temp16);
    
    // read items
    for (i=0; i < count; i++)
    {
        // a. read type from buffer
        memcpy(&type, bufpos, sizeof(type));
        bufpos+=sizeof(section);
        
        // b. read section from buffer
        memcpy(&section, bufpos, sizeof(section));
        bufpos+=sizeof(section);
        
        // c. read key from buffer
        memcpy(&temp16, bufpos, sizeof(temp16));
        bufpos+=sizeof(temp16);
        key=le16_to_cpu(temp16);
        
        // d. read sizeof(data)
        memcpy(&temp16, bufpos, sizeof(temp16));
        bufpos+=sizeof(temp16);
        size=le16_to_cpu(temp16);
        
        // e. add item to dico
        if (dico_add_generic(d, section, key, bufpos, size, type)!=0)
            return OLDERR_FATAL;
        bufpos+=size;
    }
    
    free(buffer);
    return FSAERR_SUCCESS;
}

int archreader_read_header(carchreader *ai, char *magic, cdico **d, bool allowseek, u16 *fsid)
{
    s64 curpos;
    u16 temp16;
    u32 temp32;
    u32 archid;
    int res;
    
    assert(ai);
    assert(d);
    assert(fsid);
    
    // init
    memset(magic, 0, FSA_SIZEOF_MAGIC);
    *fsid=FSA_FILESYSID_NULL;
    *d=NULL;
    
    if ((*d=dico_alloc())==NULL)
    {   errprintf("dico_alloc() failed\n");
        return OLDERR_FATAL;
    }
    
    // search for next read header marker and magic (it may be further if corruption in archive)
    if ((curpos=lseek64(ai->archfd, 0, SEEK_CUR))<0)
    {   sysprintf("lseek64() failed to get the current position in archive\n");
        return OLDERR_FATAL;
    }
    
    if ((res=archreader_read_data(ai, magic, FSA_SIZEOF_MAGIC))!=FSAERR_SUCCESS)
    {   msgprintf(MSG_STACK, "cannot read header magic: res=%d\n", res);
        return OLDERR_FATAL;
    }
    
    // we don't want to search for the magic if it's a volume header
    if (is_magic_valid(magic)!=true && allowseek!=true)
    {   errprintf("cannot read header magic: this is not a valid fsarchiver file, or it has been created with a different version.\n");
        return OLDERR_FATAL;
    }
    
    while (is_magic_valid(magic)!=true)
    {
        if (lseek64(ai->archfd, curpos++, SEEK_SET)<0)
        {   sysprintf("lseek64(pos=%lld, SEEK_SET) failed\n", (long long)curpos);
            return OLDERR_FATAL;
        }
        if ((res=archreader_read_data(ai, magic, FSA_SIZEOF_MAGIC))!=FSAERR_SUCCESS)
        {   msgprintf(MSG_STACK, "cannot read header magic: res=%d\n", res);
            return OLDERR_FATAL;
        }
    }
    
    // read the archive id
    if ((res=archreader_read_data(ai, &temp32, sizeof(temp32)))!=FSAERR_SUCCESS)
    {   msgprintf(MSG_STACK, "cannot read archive-id in header: res=%d\n", res);
        return OLDERR_FATAL;
    }
    archid=le32_to_cpu(temp32);
    if (ai->archid) // only check archive-id if it's known (when main header has been read)
    {
        if (archid!=ai->archid)
        {   errprintf("archive-id in header does not match: archid=[%.8x], expected=[%.8x]\n", archid, ai->archid);
            return OLDERR_MINOR;
        }
    }
    
    // read the filesystem id
    if ((res=archreader_read_data(ai, &temp16, sizeof(temp16)))!=FSAERR_SUCCESS)
    {   msgprintf(MSG_STACK, "cannot read filesystem-id in header: res=%d\n", res);
        return OLDERR_FATAL;
    }
    *fsid=le16_to_cpu(temp16);
    
    // read the dico of the header
    if ((res=archreader_read_dico(ai, *d))!=FSAERR_SUCCESS)
    {   msgprintf(MSG_STACK, "imgdisk_read_dico() failed\n");
        return res;
    }
    
    return FSAERR_SUCCESS;
}

int archreader_read_volheader(carchreader *ai)
{
    char creatver[FSA_MAX_PROGVERLEN];
    char filefmt[FSA_MAX_FILEFMTLEN];
    char magic[FSA_SIZEOF_MAGIC];
    cdico *d;
    u32 volnum;
    u32 readid;
    u16 fsid;
    int res;
    int ret=0;
    
    // init
    assert(ai);
    memset(magic, 0, sizeof(magic));

    // ---- a. read header from archive file
    if ((res=archreader_read_header(ai, magic, &d, false, &fsid))!=FSAERR_SUCCESS)
    {   errprintf("archreader_read_header() failed to read the archive header\n");
        return -1;
    }
    
    // ---- b. check the magic is what we expected
    if (strncmp(magic, FSA_MAGIC_VOLH, FSA_SIZEOF_MAGIC)!=0)
    {   errprintf("magic is not what we expected: found=[%s] and expected=[%s]\n", magic, FSA_MAGIC_VOLH);
        ret=-1; goto archio_read_volheader_error;
    }
    
    if (dico_get_u32(d, 0, VOLUMEHEADKEY_ARCHID, &readid)!=0)
    {   errprintf("cannot get VOLUMEHEADKEY_ARCHID from the volume header\n");
        ret=-1; goto archio_read_volheader_error;
    }
    
    // ---- c. check the archive id
    if (ai->archid==0) // archid not know: this is the first volume
    {
        ai->archid=readid;
    }
    else if (readid!=ai->archid) // archid known: not the first volume
    {   errprintf("wrong header id: found=%.8x and expected=%.8x\n", readid, ai->archid);
        ret=-1; goto archio_read_volheader_error;
    }
    
    // ---- d. check the volnum
    if (dico_get_u32(d, 0, VOLUMEHEADKEY_VOLNUM, &volnum)!=0)
    {   errprintf("cannot get VOLUMEHEADKEY_VOLNUM from the volume header\n");
        ret=-1; goto archio_read_volheader_error;
    }
    if (volnum!=ai->curvol) // not the right volume number
    {   errprintf("wrong volume number in [%s]: volnum is %d and we need volnum %d\n", ai->volpath, (int)volnum, (int)ai->curvol);
        ret=-1; goto archio_read_volheader_error;
    }
    
    // ---- d. check the the file format
    if (dico_get_data(d, 0, VOLUMEHEADKEY_FILEFORMATVER, filefmt, FSA_MAX_FILEFMTLEN, NULL)!=0)
    {   errprintf("cannot find VOLUMEHEADKEY_FILEFORMATVER in main-header\n");
        ret=-1; goto archio_read_volheader_error;
    }
    
    if (ai->filefmt[0]==0) // filefmt not know: this is the first volume
    {
        memcpy(ai->filefmt, filefmt, FSA_MAX_FILEFMTLEN);
    }
    else if (strncmp(filefmt, ai->filefmt, FSA_MAX_FILEFMTLEN)!=0)
    {
        errprintf("This archive is based on a different file format: [%s]. Cannot continue.\n", ai->filefmt);
        errprintf("It has been created with fsarchiver [%s], you should extrat the archive using that version.\n", ai->creatver);
        errprintf("The current version of the program is [%s], and it's based on format [%s]\n", FSA_VERSION, FSA_FILEFORMAT);
        ret=-1; goto archio_read_volheader_error;
    }
    
    if (dico_get_data(d, 0, VOLUMEHEADKEY_PROGVERCREAT, creatver, FSA_MAX_PROGVERLEN, NULL)!=0)
    {   errprintf("cannot find VOLUMEHEADKEY_PROGVERCREAT in main-header\n");
        ret=-1; goto archio_read_volheader_error;
    }
    
    if (ai->creatver[0]==0)
        memcpy(ai->creatver, creatver, FSA_MAX_PROGVERLEN);
    
archio_read_volheader_error:
    dico_destroy(d);
    
    return ret;
}

int archreader_read_block(carchreader *ai, cdico *in_blkdico, int in_skipblock, int *out_sumok, struct s_blockinfo *out_blkinfo)
{
    u32 arblockcsumorig;
    u32 arblockcsumcalc;
    u32 curblocksize; // data size
    u64 blockoffset; // offset of the block in the file
    u16 compalgo; // compression algo used
    u16 cryptalgo; // encryption algo used
    u32 finalsize; // compressed  block size
    u32 compsize;
    u8 *buffer;
    
    assert(ai);
    assert(out_sumok);
    assert(in_blkdico);
    assert(out_blkinfo);
    
    // init
    memset(out_blkinfo, 0, sizeof(struct s_blockinfo));
    *out_sumok=-1;
    
    if (dico_get_u64(in_blkdico, 0, BLOCKHEADITEMKEY_BLOCKOFFSET, &blockoffset)!=0)
    {   msgprintf(3, "cannot get blockoffset from block-header\n");
        return -1;
    }
    
    if (dico_get_u32(in_blkdico, 0, BLOCKHEADITEMKEY_REALSIZE, &curblocksize)!=0 || curblocksize>FSA_MAX_BLKSIZE)
    {   msgprintf(3, "cannot get blocksize from block-header\n");
        return -1;
    }
    
    if (dico_get_u16(in_blkdico, 0, BLOCKHEADITEMKEY_COMPRESSALGO, &compalgo)!=0)
    {   msgprintf(3, "cannot get BLOCKHEADITEMKEY_COMPRESSALGO from block-header\n");
        return -1;
    }
    
    if (dico_get_u16(in_blkdico, 0, BLOCKHEADITEMKEY_ENCRYPTALGO, &cryptalgo)!=0)
    {   msgprintf(3, "cannot get BLOCKHEADITEMKEY_ENCRYPTALGO from block-header\n");
        return -1;
    }
    
    if (dico_get_u32(in_blkdico, 0, BLOCKHEADITEMKEY_ARSIZE, &finalsize)!=0)
    {   msgprintf(3, "cannot get BLOCKHEADITEMKEY_ARSIZE from block-header\n");
        return -1;
    }
    
    if (dico_get_u32(in_blkdico, 0, BLOCKHEADITEMKEY_COMPSIZE, &compsize)!=0)
    {   msgprintf(3, "cannot get BLOCKHEADITEMKEY_COMPSIZE from block-header\n");
        return -1;
    }
    
    if (dico_get_u32(in_blkdico, 0, BLOCKHEADITEMKEY_ARCSUM, &arblockcsumorig)!=0)
    {   msgprintf(3, "cannot get BLOCKHEADITEMKEY_ARCSUM from block-header\n");
        return -1;
    }
    
    if (in_skipblock==true) // the main thread does not need that block (block belongs to a filesys we want to skip)
    {
        if (lseek64(ai->archfd, (long)finalsize, SEEK_CUR)<0)
        {   sysprintf("cannot skip block (finalsize=%ld) failed\n", (long)finalsize);
            return -1;
        }
        return 0;
    }
    
    // ---- allocate memory
    if ((buffer=malloc(finalsize))==NULL)
    {   errprintf("cannot allocate block: malloc(%d) failed\n", finalsize);
        return FSAERR_ENOMEM;
    }
    
    if (read(ai->archfd, buffer, (long)finalsize)!=(long)finalsize)
    {   sysprintf("cannot read block (finalsize=%ld) failed\n", (long)finalsize);
        free(buffer);
        return -1;
    }
    
    // prepare blkinfo
    out_blkinfo->blkdata=(char*)buffer;
    out_blkinfo->blkrealsize=curblocksize;
    out_blkinfo->blkoffset=blockoffset;
    out_blkinfo->blkarcsum=arblockcsumorig;
    out_blkinfo->blkcompalgo=compalgo;
    out_blkinfo->blkcryptalgo=cryptalgo;
    out_blkinfo->blkarsize=finalsize;
    out_blkinfo->blkcompsize=compsize;
    
    // ---- checksum
    arblockcsumcalc=fletcher32(buffer, finalsize);
    if (arblockcsumcalc!=arblockcsumorig) // bad checksum
    {
        errprintf("block is corrupt at offset=%ld, blksize=%ld\n", (long)blockoffset, (long)curblocksize);
        free(out_blkinfo->blkdata);
        if ((out_blkinfo->blkdata=malloc(curblocksize))==NULL)
        {   errprintf("cannot allocate block: malloc(%d) failed\n", curblocksize);
            return FSAERR_ENOMEM;
        }
        memset(out_blkinfo->blkdata, 0, curblocksize);
        *out_sumok=false;
        // go to the beginning of the corrupted contents so that the next header is searched here
        if (lseek64(ai->archfd, -(long long)finalsize, SEEK_CUR)<0)
        {   errprintf("lseek64() failed\n");
        }
    }
    else // no corruption detected
    {
        *out_sumok=true;
    }
    
    return 0;
}
