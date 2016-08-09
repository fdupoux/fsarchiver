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

#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <attr/xattr.h>
#include <zlib.h>
#include <assert.h>
#include <gcrypt.h>
#include <uuid.h>

#include "fsarchiver.h"
#include "dico.h"
#include "dichl.h"
#include "archwriter.h"
#include "options.h"
#include "common.h"
#include "oper_save.h"
#include "strlist.h"
#include "filesys.h"
#include "fs_ext2.h"
#include "fs_xfs.h"
#include "fs_reiserfs.h"
#include "fs_reiser4.h"
#include "fs_jfs.h"
#include "fs_btrfs.h"
#include "fs_ntfs.h"
#include "thread_comp.h"
#include "thread_archio.h"
#include "syncthread.h"
#include "regmulti.h"
#include "crypto.h"
#include "error.h"
#include "queue.h"

typedef struct s_savear
{   carchwriter ai;
    cregmulti   regmulti;
    cdichl      *dichardlinks;
    cstats      stats;
    int         fstype;
    int         fsid;
    u64         objectid;
    u64         cost_global;
    u64         cost_current;
} csavear;

typedef struct s_devinfo
{   char        devpath[PATH_MAX];
    char        partmount[PATH_MAX];
    bool        mountedbyfsa;
    int         fstype;
} cdevinfo;

int createar_obj_regfile_multi(csavear *save, cdico *header, char *relpath, char *fullpath, u64 filesize)
{
    char databuf[FSA_MAX_SMALLFILESIZE];
    u8 md5sum[16];
    int ret=0;
    int res;
    int fd;
    
    // The checksum will be in the obj-header not in a file footer
    if ((fd=open64(fullpath, O_RDONLY|O_LARGEFILE))<0)
    {   sysprintf("Cannot open small file %s for reading\n", relpath);
        return -1;
    }
    
    msgprintf(MSG_DEBUG1, "backup_obj_regfile_multi(file=%s, size=%lld)\n", relpath, (long long)filesize);
    
    res=read(fd, databuf, (long)filesize);
    close(fd);
    if (res!=filesize)
    {   
        if (res>=0 && res<filesize) // file has been truncated: pad with zeros
        {   ret=-1;
            errprintf("file [%s] has been truncated to %lld bytes (original size: %lld): padding with zeros\n", 
                relpath, (long long)res, (long long)filesize);
            memset(databuf+res, 0, filesize-res); // zero out remaining bytes
        }
        else // read error
        {   sysprintf("Cannot read data block size=%ld from small file %s, res=%ld\n", (long)filesize, relpath, (long)res);
            return -1;
        }
    }
    
    gcry_md_hash_buffer(GCRY_MD_MD5, md5sum, databuf, filesize);
    dico_add_data(header, 0, DISKITEMKEY_MD5SUM, md5sum, 16);
    
    // if shared-block with many small files is full, push it to queue and make a new one
    if (regmulti_save_enough_space_for_new_file(&save->regmulti, filesize)==false)
    {
        if (regmulti_save_enqueue(&save->regmulti, &g_queue, save->fsid)!=0)
        {   errprintf("Cannot queue last block of small-files\n");
            return -1;
        }
        
        regmulti_empty(&save->regmulti);
    }
    
    // copy current small file to the shared-block
    if (regmulti_save_addfile(&save->regmulti, header, databuf, filesize)!=0)
    {   errprintf("Cannot add small-file %s to regmulti structure\n", relpath);
        return -1;
    }
    
    return ret;
}

int createar_obj_regfile_unique(csavear *save, cdico *header, char *relpath, char *fullpath, u64 filesize) // large or empty files
{
    cdico *footerdico=NULL;
    struct s_blockinfo blkinfo;
    gcry_md_hd_t md5ctx;
    u32 curblocksize;
    bool eof=false;
    u64 remaining;
    char text[256];
    u8 *origblock;
    u8 *md5tmp;
    u8 md5sum[16];
    u64 filepos;
    int ret=0;
    int res;
    int fd;
    
    if (gcry_md_open(&md5ctx, GCRY_MD_MD5, 0) != GPG_ERR_NO_ERROR)
    {   errprintf("gcry_md_open() failed\n");
        return -1;
    }
    
    if ((fd=open64(fullpath, O_RDONLY|O_LARGEFILE))<0)
    {   sysprintf("Cannot open %s for reading\n", relpath);
        return -1;
    }
    
    // write header with file attributes (only if open64() works)
    queue_add_header(&g_queue, header, FSA_MAGIC_OBJT, save->fsid);
    
    msgprintf(MSG_DEBUG1, "backup_obj_regfile_unique(file=%s, size=%lld)\n", relpath, (long long)filesize);
    for (filepos=0; (filesize>0) && (filepos < filesize) && (get_interrupted()==false); filepos+=curblocksize)
    {
        remaining=filesize-filepos;
        curblocksize=min(remaining, g_options.datablocksize);
        msgprintf(MSG_DEBUG2, "----> filepos=%lld, remaining=%lld, curblocksize=%lld\n", (long long)filepos, (long long)remaining, (long long)curblocksize);
        
        origblock=malloc(curblocksize);
        if (!origblock)
        {   errprintf("malloc(%ld) failed: cannot allocate data block\n", (long)origblock);
            ret=-1;
            goto backup_obj_regfile_unique_error;
        }
        
        if (eof==false) // file has not been truncated: read the next block
        {
            if ((res=read(fd, origblock, (long)curblocksize))!=curblocksize)
            {   ret=-1;
                if (res>=0 && res<curblocksize) // file has been truncated: pad with zeros
                {   errprintf("file [%s] has been truncated to %lld bytes (original size: %lld): padding with zeros\n", 
                        relpath, (long long)(filepos+res), (long long)filesize);
                    eof=true; // set oef to true so that we don't try to read the next blocks
                    memset(origblock+res, 0, curblocksize-res); // zero out remaining bytes
                }
                else if (res<0) // read error
                {   sysprintf("Cannot read data block from %s, block=%ld and res=%ld\n", relpath, (long)curblocksize, (long)res);
                    ret=-1;
                    goto backup_obj_regfile_unique_error;
                }
            }
        }
        else // file has been truncated: write zero so that the contents and the length in the header are consistent
        {
            memset(origblock, 0, curblocksize);
        }
        
        gcry_md_write(md5ctx, origblock, curblocksize);
        
        // add block to the queue
        memset(&blkinfo, 0, sizeof(blkinfo));
        blkinfo.blkrealsize=curblocksize;
        blkinfo.blkdata=(char*)origblock;
        blkinfo.blkoffset=filepos;
        blkinfo.blkfsid=save->fsid;
        if (queue_add_block(&g_queue, &blkinfo, QITEM_STATUS_TODO)!=0)
        {   sysprintf("queue_add_block(%s) failed\n", relpath);
            ret=-1;
            goto backup_obj_regfile_unique_error;
        }
    }
    
    if (get_interrupted()==true)
    {   errprintf("operation has been interrupted\n");
        ret=-1;
        goto backup_obj_regfile_unique_error;
    }
    
    // write the footer with the global md5sum
    if ((md5tmp=gcry_md_read(md5ctx, GCRY_MD_MD5))==NULL)
    {   errprintf("gcry_md_read() failed\n");
        ret=-1;
        goto backup_obj_regfile_unique_error;
    }
    memcpy(md5sum, md5tmp, 16);
    gcry_md_close(md5ctx);
    
    msgprintf(MSG_DEBUG1, "--> finished loop for file=%s, size=%lld, md5=[%s]\n", relpath, (long long)filesize, format_md5(text, sizeof(text), md5sum));
    
    // don't write the footer for empty files (checksum does not make sense --> don't waste space in the archive)
    if (filesize>0)
    {
        if ((footerdico=dico_alloc())==NULL)
        {   errprintf("dico_alloc() failed\n");
            ret=-1;
            goto backup_obj_regfile_unique_error;
        }
        dico_add_data(footerdico, 0, BLOCKFOOTITEMKEY_MD5SUM, md5sum, 16);
        
        if (queue_add_header(&g_queue, footerdico, FSA_MAGIC_FILF, save->fsid)!=0)
        {   msgprintf(MSG_VERB2, "Cannot write footer for file %s\n", relpath);
            ret=-1;
            goto backup_obj_regfile_unique_error;
        }
    }
    
backup_obj_regfile_unique_error:
    close(fd);
    return ret;
}

int createar_item_xattr(csavear *save, char *root, char *relpath, struct stat64 *statbuf, cdico *d)
{
    char fullpath[PATH_MAX];
    char *valbuf=NULL;
    char buffer[4096];
    s64 attrsize;
    int valsize=0;
    int listlen;
    u64 attrcnt;
    int ret=0;
    int pos;
    int len;
    
    // init
    concatenate_paths(fullpath, sizeof(fullpath), root, relpath);
    attrcnt=0;
    
    memset(buffer, 0, sizeof(buffer));
    listlen=llistxattr(fullpath, buffer, sizeof(buffer)-1);
    msgprintf(MSG_DEBUG2, "xattr:llistxattr(%s)=%d\n", relpath, listlen);
    
    for (pos=0; (pos<listlen) && (pos<sizeof(buffer)); pos+=len)
    {
        len=strlen(buffer+pos)+1;
        attrsize=lgetxattr(fullpath, buffer+pos, NULL, 0);
        msgprintf(MSG_VERB2, "            xattr:file=[%s], attrid=%d, name=[%s], size=%ld\n", relpath, (int)attrcnt, buffer+pos, (long)attrsize);
        if (attrsize>65535LL)
        {   errprintf("file [%s] has an xattr [%s] with data too big (size=%ld, maxsize=64k)\n", relpath, buffer+pos, (long)attrsize);
            ret=-1;
            continue; // copy the next xattr
        }
        errno=0;
        if ((valbuf=malloc(attrsize+1))==NULL)
        {   sysprintf("malloc(%ld) failed\n", (long)(attrsize+1));
            ret=-1;
            continue; // ignore the current xattr
        }
        errno=0;
        valsize=lgetxattr(fullpath, buffer+pos, valbuf, attrsize);
        msgprintf(MSG_VERB2, "            xattr:lgetxattr(%s,%s)=%d\n", relpath, buffer+pos, valsize);
        if (valsize>=0)
        {
            msgprintf(MSG_VERB2,  "            xattr:lgetxattr(%s,%s)=%d: [%s]\n", relpath, buffer+pos, valsize, buffer+pos);
            msgprintf(MSG_DEBUG2, "            xattr:lgetxattr(%s)=%d: [%s]=[%s]\n", relpath, valsize, buffer+pos, valbuf);
            msgprintf(MSG_DEBUG2, "            xattr:dico_add_string(%s, xattr): key=%d, name=[%s]\n", relpath, (int)(2*attrcnt)+0, buffer+pos);
            dico_add_string(d, DICO_OBJ_SECTION_XATTR, (2*attrcnt)+0, buffer+pos);
            msgprintf(MSG_DEBUG2, "            xattr:dico_add_data(%s, xattr): key=%d, data (size=[%d])\n", relpath, (int)(2*attrcnt)+1, valsize);
            dico_add_data(d, DICO_OBJ_SECTION_XATTR, (2*attrcnt)+1, valbuf, valsize);
            attrcnt++;
            free(valbuf);
        }
        else if (errno!=ENOATTR) // if the attribute exists and we cannot read it
        {
            sysprintf("            xattr:lgetxattr(%s,%s)=%d\n", relpath, buffer+pos, valsize);
            ret=-1;
            free(valbuf);
            continue; // copy the next xattr
        }
        else // errno==ENOATTR hence the attribute does not exist
        {
            msgprintf(MSG_VERB2, "            xattr:lgetxattr-win(%s,%s)=-1: errno==ENOATTR\n", relpath, buffer+pos);
            free(valbuf);
        }
    }
    
    return ret;
}

int createar_item_winattr(csavear *save, char *root, char *relpath, struct stat64 *statbuf, cdico *d)
{
    char fullpath[PATH_MAX];
    char *valbuf=NULL;
    int valsize=0;
    s64 attrsize;
    u64 attrcnt;
    int ret=0;
    int i;
    
    char *winattr[]= {"system.ntfs_acl", "system.ntfs_attrib", "system.ntfs_reparse_data", "system.ntfs_times", "system.ntfs_dos_name", NULL};
    
    // init
    concatenate_paths(fullpath, sizeof(fullpath), root, relpath);
    attrcnt=0;
    
    for (i=0; winattr[i]; i++)
    {
        if ((strcmp(relpath, "/")==0) && (strcmp(winattr[i], "system.ntfs_dos_name")==0)) // the root inode does not require a short name
            continue;
        
        errno=0;
        if ((attrsize=lgetxattr(fullpath, winattr[i], NULL, 0)) < 0) // get the size of the attribute
        {
            if (errno!=ENOATTR)
            {
                sysprintf("           winattr:lgetxattr(%s,%s): returned negative attribute size\n", relpath, winattr[i]); // output if there are any other error
                ret=-1;
            }
            continue; // ignore the current xattr
        }
        msgprintf(MSG_VERB2, "            winattr:file=[%s], attrcnt=%d, name=[%s], size=%ld\n", relpath, (int)attrcnt, winattr[i], (long)attrsize);
        if (attrsize>65535LL)
        {
            errprintf("file [%s] has an xattr [%s] with data size=%ld too big (max xattr size is 65535)\n", relpath, winattr[i], (long)attrsize);
            ret=-1;
            continue; // ignore the current xattr
        }
        errno=0;
        if ((valbuf=malloc(attrsize+1))==NULL)
        {
            sysprintf("malloc(%d) failed\n", (int)(attrsize+1));
            ret=-1;
            continue; // ignore the current xattr
        }
        valsize=lgetxattr(fullpath, winattr[i], valbuf, attrsize);
        msgprintf(MSG_VERB2, "            winattr:lgetxattr-win(%s,%s)=%d\n", relpath, winattr[i], valsize);
        if (valsize>=0)
        {
            msgprintf(MSG_VERB2, "            winattr:dico_add_string(%s, winattr): key=%d, name=[%s]\n", relpath, (int)(2*attrcnt)+0, winattr[i]);
            dico_add_string(d, DICO_OBJ_SECTION_WINATTR, (2*attrcnt)+0, winattr[i]);
            msgprintf(MSG_VERB2, "            winattr:dico_add_data(%s, winattr): key=%d, data (size=[%d])\n", relpath, (int)(2*attrcnt)+1, valsize);
            dico_add_data(d, DICO_OBJ_SECTION_WINATTR, (2*attrcnt)+1, valbuf, valsize);
            free(valbuf);
            attrcnt++;
        }
        else if (errno!=ENOATTR) // if the attribute exists and we cannot read it
        {
            sysprintf("            winattr:lgetxattr(%s,%s)=%d\n", relpath, winattr[i], valsize);
            ret=-1;
            free(valbuf);
            continue; // ignore the current xattr
        }
        else // errno==ENOATTR hence the attribute does not exist
        {
            msgprintf(MSG_VERB2, "            winattr:lgetxattr-win(%s,%s)=-1: errno==ENOATTR\n", relpath, winattr[i]);
            free(valbuf);
        }
    }
    
    return ret;
}

int createar_item_stdattr(csavear *save, char *root, char *relpath, struct stat64 *statbuf, cdico *d, int *objtype, u64 *filecost)
{
    struct stat64 stattarget;
    char fullpath[PATH_MAX];
    char buffer[PATH_MAX];
    char buffer2[PATH_MAX];
    char directory[PATH_MAX];
    char *linktarget=NULL;
    u64 flags;
    int res;
    int i;
    
    // init
    flags=0;
    *objtype=OBJTYPE_NULL;
    *filecost=FSA_COST_PER_FILE; // fixed cost per file
    concatenate_paths(fullpath, sizeof(fullpath), root, relpath);
    
    msgprintf(MSG_DEBUG2, "Adding [%.5lld]=[%s]\n", (long long)save->objectid, relpath);
    if (dico_add_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_OBJECTID, (u64)(save->objectid)++)!=0)
    {   errprintf("dico_add_u64(DICO_OBJ_SECTION_STDATTR) failed\n");
        return -1;
    }
    if (dico_add_string(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_PATH, relpath)!=0)
    {   errprintf("dico_add_string(DICO_OBJ_SECTION_STDATTR) failed\n");
        return -1;
    }
    if (dico_add_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_SIZE, (u64)statbuf->st_size)!=0)
    {   errprintf("dico_add_u64(DICO_OBJ_SECTION_STDATTR) failed\n");
        return -1;
    }
    if (dico_add_u32(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_MODE, (u32)statbuf->st_mode)!=0)
    {   errprintf("dico_add_u32(DICO_OBJ_SECTION_STDATTR) failed\n");
        return -1;
    }
    if (dico_add_u32(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_UID, (u32)statbuf->st_uid)!=0)
    {   errprintf("dico_add_u32(DICO_OBJ_SECTION_STDATTR) failed\n");
        return -1;
    }
    if (dico_add_u32(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_GID, (u32)statbuf->st_gid)!=0)
    {   errprintf("dico_add_u32(gid) failed\n");
        return -1;
    }
    if (dico_add_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_ATIME, (u32)statbuf->st_atime)!=0)
    {   errprintf("dico_add_u32(DICO_OBJ_SECTION_STDATTR) failed\n");
        return -1;
    }
    if (dico_add_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_MTIME, (u32)statbuf->st_mtime)!=0)
    {   errprintf("dico_add_u32(DICO_OBJ_SECTION_STDATTR) failed\n");
        return -1;
    }
    
    // 2. copy specific properties to the dico
    switch (statbuf->st_mode & S_IFMT)
    {
        case S_IFDIR:
            *objtype=OBJTYPE_DIR;
            break;
        case S_IFLNK:
            // save path of the target of the symlink
            *objtype=OBJTYPE_SYMLINK;
            memset(buffer, 0, sizeof(buffer));
            memset(buffer2, 0, sizeof(buffer2));
            if ((readlink(fullpath, buffer, sizeof(buffer)))<0)
            {   sysprintf("readlink(%s) failed\n", fullpath);
                return -1;
            }
            // fix path as ntfs-3g>=2010.3.6 may return an absolute path that includes the mount directory
            linktarget=buffer;
            if (memcmp(linktarget, "/tmp/fsa/", 9)==0)
            {
                for (i=0; i<3; i++)
                {
                    if (linktarget[0]=='/')
                        linktarget++;
                    while (linktarget[0]!=0 && linktarget[0]!='/')
                        linktarget++;
                }
            }
            msgprintf(MSG_DEBUG1, "fixed-readlink([%s])=[%s]\n", fullpath, linktarget);
            dico_add_string(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_SYMLINK, linktarget);
            
            // save type of the target of the symlink (required to recreate ntfs symlinks)
            if (filesys[save->fstype].savesymtargettype==true)
            {
                if (linktarget[0]=='/') // absolute symbolic link
                {
                    snprintf(buffer2, sizeof(buffer2), "%s", buffer);
                    msgprintf(MSG_DEBUG1, "absolute-symlink: fullpath=[%s] --> lstat64=[%s]\n", fullpath, buffer2);
                }
                else // relative symbolic link
                {
                    extract_dirpath(fullpath, directory, sizeof(directory));
                    concatenate_paths(buffer2, sizeof(buffer2), directory, linktarget);
                    msgprintf(MSG_DEBUG1, "relative-symlink: fullpath=[%s] --> lstat64=[%s]\n", fullpath, buffer2);
                }
                if (lstat64(buffer2, &stattarget)==0)
                {
                    switch (stattarget.st_mode & S_IFMT)
                    {
                        case S_IFDIR:
                            dico_add_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_LINKTARGETTYPE, OBJTYPE_DIR);
                            msgprintf(MSG_DEBUG1, "LINK: link=[%s], target=[%s]=DIR\n", relpath, buffer2);
                            break;
                        case S_IFREG:
                            dico_add_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_LINKTARGETTYPE, OBJTYPE_REGFILEUNIQUE);
                            msgprintf(MSG_DEBUG1, "LINK: link=[%s], target=[%s]=REGFILE\n", relpath, buffer2);
                            break;
                        default:
                            msgprintf(MSG_DEBUG1, "LINK: link=[%s], target=[%s]=UNKNOWN\n", relpath, buffer2);
                            break;
                    }
                }
            }
            break;
        case S_IFREG:
            if (statbuf->st_nlink>1) // there are several links to that inode: there are hard links
            {
                res=dichl_get(save->dichardlinks, (u64)statbuf->st_rdev, (u64)statbuf->st_ino, buffer, sizeof(buffer));
                if (res==0) // inode already seen --> hard link
                {   dico_add_string(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_HARDLINK, buffer);
                    *objtype=OBJTYPE_HARDLINK;
                }
                else // next link to thar inode will be an hard link
                {
                    dichl_add(save->dichardlinks, (u64)statbuf->st_rdev, (u64)statbuf->st_ino, relpath);
                }
            }
            if (*objtype==OBJTYPE_NULL) // not an hard-link: it's a regular file or the first link when multiple links found
            {
                // don't allow reg-files with hardlinks to be copied as a small-file with other small-files
                // the file would be copied later in the archive (when the block for small files is ready)
                // and then the hardlink may come before the regfile (first link to that object)
                // case 1: file smaller than threshold: pack its data with other small files in a single compressed block
                *filecost+=statbuf->st_size;
                if ((statbuf->st_size > 0) && (statbuf->st_size < g_options.smallfilethresh) && (statbuf->st_nlink==1))
                    *objtype=OBJTYPE_REGFILEMULTI;
                else // case 2: file having hardlinks or larger than the threshold
                    *objtype=OBJTYPE_REGFILEUNIQUE;
                // empty files are considered as OBJTYPE_REGFILEUNIQUE (statbuf->st_size==0)
            }
            if (*objtype==OBJTYPE_REGFILEUNIQUE || *objtype==OBJTYPE_REGFILEMULTI)
            {
                if (((u64)statbuf->st_blocks) * ((u64)DEV_BSIZE) < ((u64)statbuf->st_size))
                    flags|=FSA_FILEFLAGS_SPARSE;
            }
            break;
        case S_IFCHR:
            dico_add_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_RDEV, statbuf->st_rdev);
            *objtype=OBJTYPE_CHARDEV;
            break;
        case S_IFBLK:
            dico_add_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_RDEV, statbuf->st_rdev);
            *objtype=OBJTYPE_BLOCKDEV;
            break;
        case S_IFIFO:
            dico_add_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_RDEV, statbuf->st_rdev);
            *objtype=OBJTYPE_FIFO;
            break;
        case S_IFSOCK:
            dico_add_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_RDEV, statbuf->st_rdev);
            *objtype=OBJTYPE_SOCKET;
            break;
        default:
            errprintf("unknown item %s\n", fullpath);
            return -1;
            break;
    }
    
    dico_add_u32(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_OBJTYPE, *objtype);
    if (flags!=0)
        dico_add_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_FLAGS, flags);
    
    return 0;
}

int createar_save_file(csavear *save, char *root, char *relpath, struct stat64 *statbuf, u64 *costeval)
{
    char fullpath[PATH_MAX];
    char strprogress[256];
    cdico *dicoattr;
    int attrerrors=0;
    u64 filecost;
    s64 progress;
    int objtype;
    int res;
    
    // init    
    concatenate_paths(fullpath, sizeof(fullpath), root, relpath);
    
    // don't backup the archive file itself
    if (archwriter_is_path_to_curvol(&save->ai, fullpath)==true)
    {   errprintf("file [%s] ignored: it's the current archive file\n", fullpath);
        save->stats.err_regfile++;
        return 0; // not a fatal error, oper must continue
    }
    
    // ---- backup standard file attributes
    if ((dicoattr=dico_alloc())==NULL)
    {   errprintf("dico_alloc() failed\n");
        return -1; // fatal error
    }
    
    if (createar_item_stdattr(save, root, relpath, statbuf, dicoattr, &objtype, &filecost)!=0)
    {   msgprintf(MSG_STACK, "backup_item_stdattr() failed: cannot read standard attributes on [%s]\n", relpath);
        attrerrors++;
    }
    
    // --- cost required for the progression info
    if (costeval!=NULL) 
    {   *costeval+=filecost;
        dico_destroy(dicoattr);
        return 0;
    }
    
    // ---- backup other file attributes (xattr + winattr)
    if (createar_item_xattr(save, root, relpath, statbuf, dicoattr)!=0)
    {   msgprintf(MSG_STACK, "backup_item_xattr() failed: cannot prepare xattr-dico for item %s\n", relpath);
        attrerrors++;
    }
    
    if (filesys[save->fstype].winattr==true)
    {
        if (createar_item_winattr(save, root, relpath, statbuf, dicoattr)!=0)
        {   msgprintf(MSG_STACK, "backup_item_winattr() failed: cannot prepare winattr-dico for item %s\n", relpath);
            attrerrors++;
        }
    }
    
    // ---- file details and progress bar
    if (get_interrupted()==false) 
    {
        memset(strprogress, 0, sizeof(strprogress));
        if (save->cost_global>0)
        {   save->cost_current+=filecost;
            progress=((save->cost_current)*100)/(save->cost_global);
            if (progress>=0 && progress<=100)
                snprintf(strprogress, sizeof(strprogress), "[%3d%%]", (int)progress);
        }
        msgprintf(MSG_VERB1, "-[%.2d]%s[%s] %s\n", save->fsid, strprogress, get_objtype_name(objtype), relpath);
    }
    
    // ---- backup file contents for regfiles
    switch (objtype)
    {
        case OBJTYPE_DIR:
            if (attrerrors>0)
            {   save->stats.err_dir++;
                dico_destroy(dicoattr);
                return 0; // error is not fatal, operation must continue
            }
            if (queue_add_header(&g_queue, dicoattr, FSA_MAGIC_OBJT, save->fsid)!=0)
            {   errprintf("queue_add_header(%s) failed\n", relpath);
                return -1; // fatal error
            }
            save->stats.cnt_dir++;
            break;
        case OBJTYPE_SYMLINK:
            if (attrerrors>0)
            {   save->stats.err_symlink++;
                dico_destroy(dicoattr);
                return 0; // error is not fatal, operation must continue
            }
            if (queue_add_header(&g_queue, dicoattr, FSA_MAGIC_OBJT, save->fsid)!=0)
            {   errprintf("queue_add_header(%s) failed\n", relpath);
                return -1; // fatal error
            }
            save->stats.cnt_symlink++;
            break;
        case OBJTYPE_HARDLINK:
            if (attrerrors>0)
            {   save->stats.err_hardlink++;
                dico_destroy(dicoattr);
                return 0; // error is not fatal, operation must continue
            }
            if (queue_add_header(&g_queue, dicoattr, FSA_MAGIC_OBJT, save->fsid)!=0)
            {   errprintf("queue_add_header(%s) failed\n", relpath);
                return -1; // fatal error
            }
            save->stats.cnt_hardlink++;
            break;
        case OBJTYPE_CHARDEV:
        case OBJTYPE_BLOCKDEV:
        case OBJTYPE_FIFO:
        case OBJTYPE_SOCKET:
            if (attrerrors>0)
            {   save->stats.err_special++;
                dico_destroy(dicoattr);
                return 0; // error is not fatal, operation must continue
            }
            if (queue_add_header(&g_queue, dicoattr, FSA_MAGIC_OBJT, save->fsid)!=0)
            {   errprintf("queue_add_header(%s) failed\n", relpath);
                return -1; // fatal error
            }
            save->stats.cnt_special++;
            break;
        case OBJTYPE_REGFILEUNIQUE:
            if (attrerrors>0)
            {   save->stats.err_regfile++;
                dico_destroy(dicoattr);
                return 0; // error is not fatal, operation must continue
            }
            if ((res=createar_obj_regfile_unique(save, dicoattr, relpath, fullpath, statbuf->st_size))!=0)
            {   msgprintf(MSG_STACK, "backup_obj_regfile_unique(%s)=%d failed\n", relpath, res);
                save->stats.err_regfile++;
                return 0; // not a fatal error, oper must continue
            }
            else
            {   save->stats.cnt_regfile++;
            }
            break;
        case OBJTYPE_REGFILEMULTI:
            if (attrerrors>0)
            {   save->stats.err_regfile++;
                dico_destroy(dicoattr);
                return 0; // error is not fatal, operation must continue
            }
            if ((res=createar_obj_regfile_multi(save, dicoattr, relpath, fullpath, statbuf->st_size))!=0)
            {   msgprintf(MSG_STACK, "backup_obj_regfile_multi(%s)=%d failed\n", relpath, res);
                save->stats.err_regfile++;
                return 0; // not a fatal error, oper must continue
            }
            else
            {   save->stats.cnt_regfile++;
            }
            break;
        default: // unknown type
            errprintf("invalid object type: %ld for file %s\n", (long)objtype, relpath);
            return -1; // fatal error
            break;
    }
    
    return 0;
}

int createar_save_directory(csavear *save, char *root, char *path, u64 *costeval)
{
    char fulldirpath[PATH_MAX];
    char fullpath[PATH_MAX];
    char relpath[PATH_MAX];
    struct stat64 statbuf;
    struct dirent *dir;
    DIR *dirdesc;
    int ret=0;
    
    // init
    concatenate_paths(fulldirpath, sizeof(fulldirpath), root, path);
    
    if (!(dirdesc=opendir(fulldirpath)))
    {   sysprintf("cannot open directory %s\n", fulldirpath);
        return 0; // not a fatal error, oper must continue
    }
    
    // backup the directory itself (important for the root of the filesystem)
    if (lstat64(fulldirpath, &statbuf)!=0)
    {   sysprintf("cannot lstat64(%s)\n", fulldirpath);
        ret=-1;
        goto backup_dir_err;
    }
    
    // save info about the directory itself
    if (createar_save_file(save, root, path, &statbuf, costeval)!=0)
    {   errprintf("createar_save_file(%s,%s) failed\n", root, path);
        ret=-1;
        goto backup_dir_err;
    }
    
    while (((dir = readdir(dirdesc)) != NULL) && (get_interrupted()==false))
    {
        // ---- ignore "." and ".." and ignore mount-points
        if (strcmp(dir->d_name,".")==0 || strcmp(dir->d_name,"..")==0)
            continue; // ignore "." and ".."
        
        // ---- calculate paths
        concatenate_paths(relpath, sizeof(relpath), path, dir->d_name);
        concatenate_paths(fullpath, sizeof(fullpath), fulldirpath, dir->d_name);
        
        // ---- get details about current file
        if (lstat64(fullpath, &statbuf)!=0)
        {   sysprintf("cannot lstat64(%s)\n", fullpath);
            ret=-1;
            goto backup_dir_err;
        }
        
        // check the list of excluded files/dirs
        if ((exclude_check(&g_options.exclude, dir->d_name)==true) // is filename excluded ?
            || (exclude_check(&g_options.exclude, relpath)==true)) // is filepath excluded ?
        {
            if (costeval==NULL) // dont log twice (eval + real)
                msgprintf(MSG_VERB2, "file/dir=[%s] excluded\n", relpath);
            continue;
        }
        
        // backup contents before the directory itself so that the dir-attributes are written after the dir contents
        if (S_ISDIR(statbuf.st_mode))
        { 
            if (createar_save_directory(save, root, relpath, costeval)!=0)
            {   msgprintf(MSG_STACK, "createar_save_directory(%s) failed\n", relpath);
                ret=-1;
                goto backup_dir_err;
            }
        }
        else // not a directory
        {
            if (createar_save_file(save, root, relpath, &statbuf, costeval)!=0)
            {   msgprintf(MSG_STACK, "createar_save_directory(%s) failed\n", relpath);
                ret=-1;
                goto backup_dir_err;
            }
        }
    }
    
backup_dir_err:
    closedir(dirdesc);
    return ret;
}

int createar_save_directory_wrapper(csavear *save, char *root, char *path, u64 *costeval)
{
    int ret;
    
    if ((save->dichardlinks=dichl_alloc())==NULL)
    {   errprintf("dichardlinks=dichl_alloc() failed\n");
        return -1;
    }
    
    if (regmulti_init(&save->regmulti, g_options.datablocksize)!=0)
    {   errprintf("regmulti_init failed\n");
        return -1;
    }
    
    ret=createar_save_directory(save, root, path, costeval);
    
    // put all small files that are in the last block to the queue
    if (regmulti_save_enqueue(&save->regmulti, &g_queue, save->fsid)!=0)
    {   errprintf("Cannot queue last block of small-files\n");
        return -1;
    }
    
    // dico for hard links not required anymore
    dichl_destroy(save->dichardlinks);
    
    return ret;
}

int createar_write_mainhead(csavear *save, int archtype, int fscount)
{
    u8 bufcheckclear[FSA_CHECKPASSBUF_SIZE+8];
    u8 bufcheckcrypt[FSA_CHECKPASSBUF_SIZE+8];
    u64 cryptsize;
    u8 md5sum[16];
    struct timeval now;
    cdico *d;
    
    if (!save)
    {   errprintf("ai is NULL\n");
        return -1;
    }
    
    // init
    gettimeofday(&now, NULL);
    if ((d=dico_alloc())==NULL)
    {   errprintf("dico_alloc() failed\n");
        return -1;
    }
    
    dico_add_string(d, 0, MAINHEADKEY_FILEFORMATVER, FSA_FILEFORMAT);
    dico_add_string(d, 0, MAINHEADKEY_PROGVERCREAT, FSA_VERSION);
    dico_add_string(d, 0, MAINHEADKEY_ARCHLABEL, g_options.archlabel);
    dico_add_u64(d, 0, MAINHEADKEY_CREATTIME, now.tv_sec);
    dico_add_u32(d, 0, MAINHEADKEY_ARCHIVEID, save->ai.archid);
    dico_add_u32(d, 0, MAINHEADKEY_ARCHTYPE, archtype);
    dico_add_u32(d, 0, MAINHEADKEY_COMPRESSALGO, g_options.compressalgo);
    dico_add_u32(d, 0, MAINHEADKEY_COMPRESSLEVEL, g_options.compresslevel);
    dico_add_u32(d, 0, MAINHEADKEY_ENCRYPTALGO, g_options.encryptalgo);
    dico_add_u32(d, 0, MAINHEADKEY_FSACOMPLEVEL, g_options.fsacomplevel);
    dico_add_u32(d, 0, MAINHEADKEY_HASDIRSINFOHEAD, true);
    
    // minimum fsarchiver version required to restore that archive
    dico_add_u64(d, 0, MAINHEADKEY_MINFSAVERSION, FSA_VERSION_BUILD(0, 6, 4, 0));
    
    if (archtype==ARCHTYPE_FILESYSTEMS)
    {   
        dico_add_u64(d, 0, MAINHEADKEY_FSCOUNT, fscount);
    }
    
    // if encryption is enabled, save the md5sum of a random buffer to check the password
    if (g_options.encryptalgo!=ENCRYPT_NONE)
    {
        memset(md5sum, 0, sizeof(md5sum));
        crypto_random(bufcheckclear, FSA_CHECKPASSBUF_SIZE);
        crypto_blowfish(FSA_CHECKPASSBUF_SIZE, &cryptsize, bufcheckclear, bufcheckcrypt, 
            g_options.encryptpass, strlen((char*)g_options.encryptpass), true);
        
        gcry_md_hash_buffer(GCRY_MD_MD5, md5sum, bufcheckclear, FSA_CHECKPASSBUF_SIZE);
        
        assert(dico_add_data(d, 0, MAINHEADKEY_BUFCHECKPASSCLEARMD5, md5sum, 16)==0);
        assert(dico_add_data(d, 0, MAINHEADKEY_BUFCHECKPASSCRYPTBUF, bufcheckcrypt, FSA_CHECKPASSBUF_SIZE)==0);
    }
    
    if (queue_add_header(&g_queue, d, FSA_MAGIC_MAIN, FSA_FILESYSID_NULL)!=0)
    {   errprintf("cannot write dico for main header\n");
        dico_destroy(d);
        return -1;
    }
    
    return 0;
}

int filesystem_mount_partition(cdevinfo *devinfo, cdico *dicofsinfo, u16 fsid)
{
    char fsbuf[FSA_MAX_FSNAMELEN];
    struct statvfs64 statfsbuf;
    cstrlist reqmntopt;
    cstrlist badmntopt;
    cstrlist curmntopt;
    int showwarningcount1=0;
    int showwarningcount2=0;
    int errorattr=false;
    char temp[PATH_MAX];
    char curmntdir[PATH_MAX];
    char optbuf[128];
    u64 fsbytestotal;
    u64 fsbytesused;
    int readwrite;
    int tmptype;
    int count;
    int res;
    int i;
    
    res=generic_get_mntinfo(devinfo->devpath, &readwrite, curmntdir, sizeof(curmntdir), optbuf, sizeof(optbuf), fsbuf, sizeof(fsbuf));
    if (res==0) // partition is already mounted
    {
        devinfo->mountedbyfsa=false;
        //snprintf(partmnt, PATH_MAX, "%s", curmntdir); // return the mount point to main savefs function
        msgprintf(MSG_DEBUG1, "generic_get_mntinfo(%s): mnt=[%s], opt=[%s], fs=[%s], rw=[%d]\n", devinfo->devpath, curmntdir, optbuf, fsbuf, readwrite);
        if (readwrite==1 && g_options.allowsaverw==0)
        {
            errprintf("partition [%s] is mounted read/write. please mount it read-only \n"
                "and then try again. you can do \"mount -o remount,ro %s\". you can \n"
                "also run fsarchiver with option '-A' if you know what you are doing.\n", 
                devinfo->devpath, devinfo->devpath);
            return -1;
        }
        if (generic_get_fstype(fsbuf, &devinfo->fstype)!=0)
        {   
            if (strcmp(fsbuf, "fuseblk")==0)
                errprintf("partition [%s] is using a fuse based filesystem (probably ntfs-3g). Unmount it and try again\n", devinfo->devpath);
            else
                errprintf("filesystem of partition [%s] is not supported by fsarchiver: filesystem=[%s]\n", devinfo->devpath, fsbuf);
            return -1;
        }

        // check the filesystem is mounted with the right mount-options (to preserve acl and xattr)
        strlist_init(&reqmntopt);
        strlist_init(&badmntopt);
        strlist_init(&curmntopt);
        if (filesys[devinfo->fstype].reqmntopt(devinfo->devpath, &reqmntopt, &badmntopt)!=0)
        {
            errprintf("cannot get the required mount options for partition=[%s]\n", devinfo->devpath);
            strlist_empty(&reqmntopt);
            strlist_empty(&badmntopt);
            return -1;
        }
        strlist_split(&curmntopt, optbuf, ',');
        msgprintf(MSG_DEBUG2, "mount options found for partition=[%s]: [%s]\n", devinfo->devpath, strlist_merge(&curmntopt, temp, sizeof(temp), ','));
        msgprintf(MSG_DEBUG2, "mount options required for partition=[%s]: [%s]\n", devinfo->devpath, strlist_merge(&reqmntopt, temp, sizeof(temp), ','));
        msgprintf(MSG_DEBUG2, "mount options to avoid for partition=[%s]: [%s]\n", devinfo->devpath, strlist_merge(&badmntopt, temp, sizeof(temp), ','));
        count=strlist_count(&reqmntopt);
        for (i=0; i < count; i++)
        {
            strlist_getitem(&reqmntopt, i, optbuf, sizeof(optbuf));
            msgprintf(MSG_DEBUG2, "checking there is reqmntopt[%d]=[%s]\n", i, optbuf);
            if (strlist_exists(&curmntopt, optbuf)!=true)
            {
                if (showwarningcount1==0)
                    errprintf("partition [%s] has to be mounted with options [%s] in order to preserve "
                        "all its attributes. you can use mount with option remount to do that.\n",
                        devinfo->devpath, strlist_merge(&reqmntopt, temp, sizeof(temp), ','));
                if (g_options.dontcheckmountopts==true)
                {   if (showwarningcount1++ == 0) // show this warning only once
                        errprintf("fsarchiver will continue anyway since the option '-a' was used\n");
                }
                else // don't ignore the mount options
                {   errprintf("fsarchiver cannot continue, you can use option '-a' to ignore "
                        "the mount options (xattr or acl may not be preserved)\n");
                    return -1;
                }
            }
        }
        count=strlist_count(&badmntopt);
        for (i=0; i < count; i++)
        {
            strlist_getitem(&badmntopt, i, optbuf, sizeof(optbuf));
            msgprintf(MSG_DEBUG2, "checking there is not badmntopt[%d]=[%s]\n", i, optbuf);
            if (strlist_exists(&curmntopt, optbuf)==true)
            {
                if (showwarningcount2==0)
                    errprintf("partition [%s] has to be mounted without options [%s] in order to preserve all its attributes\n",
                        devinfo->devpath, strlist_merge(&badmntopt, temp, sizeof(temp), ','));
                if (g_options.dontcheckmountopts==true)
                {   if (showwarningcount2++ == 0) // show this warning only once
                        errprintf("fsarchiver will continue anyway since the option '-a' was used\n");
                }
                else // don't ignore the mount options
                {   errprintf("fsarchiver cannot continue, you can use option '-a' to ignore "
                        "the mount options (xattr or acl may not be preserverd)\n");
                    return -1;
                }
            }
        }
        strlist_empty(&reqmntopt);
        strlist_empty(&badmntopt);
        strlist_empty(&curmntopt);
        // create a "mount --bind" for that mounted partition (to see behind its mount points)
        mkdir_recursive(devinfo->partmount);
        if (mount(curmntdir, devinfo->partmount, NULL, MS_BIND|MS_RDONLY, NULL)!=0)
        {
            errprintf("mount(src=[%s], target=[%s], NULL, MS_BIND|MS_RDONLY, NULL) failed\n", curmntdir, devinfo->partmount);
            return -1;
        }
        devinfo->mountedbyfsa=true;
    }
    else // partition not yet mounted
    {
        mkdir_recursive(devinfo->partmount);
        msgprintf(MSG_DEBUG1, "partition %s is not mounted\n", devinfo->devpath);
        for (tmptype=-1, i=0; (filesys[i].name) && (tmptype==-1); i++)
        {
            if ((filesys[i].test(devinfo->devpath)==true) && (filesys[i].mount(devinfo->devpath, devinfo->partmount, filesys[i].name, MS_RDONLY, NULL)==0))
            {   tmptype=i;
                msgprintf(MSG_DEBUG1, "partition %s successfully mounted on [%s] as [%s]\n", devinfo->devpath, devinfo->partmount, filesys[i].name);
            }
        }
        if (tmptype==-1)
        {   errprintf("cannot mount partition [%s]: filesystem may not be supported by either fsarchiver or the kernel.\n", devinfo->devpath);
            return -1;
        }
        devinfo->fstype=tmptype;
        devinfo->mountedbyfsa=true;
    }

    // Make sure users are aware if they save filesystems with experimental support in fsarchiver
    if ((g_options.experimental==false) && (filesys[devinfo->fstype].stable==false))
    {   errprintf("You must enable support for experimental features in order to save %s filesystems with fsarchiver.\n", filesys[devinfo->fstype].name);
        return -1;
    }

    // Make sure support for extended attributes is enabled if this filesystem supports it
    if (g_options.dontcheckmountopts==false)
    {
        errorattr=false;

        if (filesys[devinfo->fstype].support_for_xattr==true)
        {   errno=0;
            res=lgetxattr(devinfo->partmount, "user.fsa_test_xattr", temp, sizeof(temp));
            msgprintf(MSG_DEBUG1, "lgetxattr(\"%s\", \"user.fsa_test_attr\", buf, bufsize)=[%d] and errno=[%d]\n", devinfo->partmount, (int)res, (int)errno);
            // errno should be set to ENOATTR if we are able to read extended attributes
            if ((res!=0) && (errno==ENOTSUP))
            {   errprintf("fsarchiver is unable to access extended attributes on device [%s].\n", devinfo->devpath);
                errorattr=true;
            }
        }

        if (filesys[devinfo->fstype].support_for_acls==true)
        {   errno=0;
            res=lgetxattr(devinfo->partmount, "system.posix_acl_access", temp, sizeof(temp));
            msgprintf(MSG_DEBUG1, "lgetxattr(\"%s\", \"system.posix_acl_access\", buf, bufsize)=[%d] and errno=[%d]\n", devinfo->partmount, (int)res, (int)errno);
            // errno should be set to ENOATTR if we are able to read ACLs
            if ((res!=0) && (errno==ENOTSUP))
            {   errprintf("fsarchiver is unable to access ACLs on device [%s].\n", devinfo->devpath);
                errorattr=true;
            }
        }

        if (errorattr==true)
        {   errprintf("Cannot continue, you can use option '-a' to ignore "
                      "support for xattr and acl (they will not be preserved)\n");
            return -1;
        }
    }

    // get space statistics
    if (statvfs64(devinfo->partmount, &statfsbuf)!=0)
    {   errprintf("statvfs64(%s) failed\n", devinfo->partmount);
        return -1;
    }
    fsbytestotal=(u64)statfsbuf.f_frsize*(u64)statfsbuf.f_blocks;
    fsbytesused=fsbytestotal-((u64)statfsbuf.f_frsize*(u64)statfsbuf.f_bfree);
    
    dico_add_string(dicofsinfo, 0, FSYSHEADKEY_FILESYSTEM, filesys[devinfo->fstype].name);
    dico_add_string(dicofsinfo, 0, FSYSHEADKEY_MNTPATH, devinfo->partmount);
    dico_add_string(dicofsinfo, 0, FSYSHEADKEY_ORIGDEV, devinfo->devpath);
    dico_add_u64(dicofsinfo, 0, FSYSHEADKEY_BYTESTOTAL, fsbytestotal);
    dico_add_u64(dicofsinfo, 0, FSYSHEADKEY_BYTESUSED, fsbytesused);
    
    if (filesys[devinfo->fstype].getinfo(dicofsinfo, devinfo->devpath)!=0)
    {   errprintf("cannot save filesystem attributes for partition %s\n", devinfo->devpath);
        return -1;
    }
    
    return 0;
}

int createar_oper_savefs(csavear *save, cdevinfo *devinfo)
{
    cdico *dicobegin=NULL;
    cdico *dicoend=NULL;
    int ret=0;
    
    // write "begin of filesystem" header
    if ((dicobegin=dico_alloc())==NULL)
    {   errprintf("dicostart=dico_alloc() failed\n");
        return -1;
    }
    queue_add_header(&g_queue, dicobegin, FSA_MAGIC_FSYB, save->fsid);
    
    // init filesystem data struct
    save->fstype=devinfo->fstype;
    
    // main task
    ret=createar_save_directory_wrapper(save, devinfo->partmount, "/", NULL);
    
    // write "end of filesystem" header
    if ((dicoend=dico_alloc())==NULL)
    {   errprintf("dicoend=dico_alloc() failed\n");
        return -1;
    }
    
    // TODO: add stats about files count in that dico
    queue_add_header(&g_queue, dicoend, FSA_MAGIC_DATF, save->fsid);
    
    return ret;
}

int createar_oper_savedir(csavear *save, char *rootdir)
{
    char fullpath[PATH_MAX];
    char currentdir[PATH_MAX];
    
    if (rootdir[0]=='/') // absolute path
    {
        snprintf(fullpath, sizeof(fullpath), "%s", rootdir);
        createar_save_directory_wrapper(save, "/", fullpath, NULL);
    }
    else // relative path
    {
        concatenate_paths(fullpath, sizeof(fullpath), getcwd(currentdir, sizeof(currentdir)), rootdir);
        createar_save_directory_wrapper(save, ".", rootdir, NULL);
    }
    
    return 0;
}

int oper_save(char *archive, int argc, char **argv, int archtype)
{
    pthread_t thread_comp[FSA_MAX_COMPJOBS];
    cdico *dicofsinfo[FSA_MAX_FSPERARCH];
    cdevinfo devinfo[FSA_MAX_FSPERARCH];
    pthread_t thread_writer;
    u64 cost_evalfs=0;
    u64 totalerr=0;
    cdico *dicoend=NULL;
    cdico *dirsinfo=NULL;
    struct stat64 st;
    csavear save;
    int ret=0;
    int i;
    
    // init
    memset(&save, 0, sizeof(save));
    save.cost_global=0;
    
    // init archive
    archwriter_init(&save.ai);
    archwriter_generate_id(&save.ai);
    
    // pass options to archive
    path_force_extension(save.ai.basepath, PATH_MAX, archive, ".fsa");
    
    // init misc data struct to zero
    thread_writer=0;
    for (i=0; i<FSA_MAX_COMPJOBS; i++)
    {
        thread_comp[i]=0;
    }
    for (i=0; i<FSA_MAX_FSPERARCH; i++)
    {
        memset(&devinfo[i], 0, sizeof(cdevinfo));
        devinfo[i].mountedbyfsa=false;
        devinfo[i].fstype=-1;
        dicofsinfo[i]=NULL;
    }
    
    // check that arguments are all block devices when archtype==ARCHTYPE_FILESYSTEMS
    for (i=0; (archtype==ARCHTYPE_FILESYSTEMS) && (i < argc) && (argv[i]!=NULL); i++)
    {
        if ((stat64(argv[i], &st)!=0) || (!S_ISBLK(st.st_mode)))
        {   errprintf("%s is not a valid block device\n", argv[i]);
            ret=-1;
            goto do_create_error;
        }
    }
    
    // check that arguments are all directories when archtype==ARCHTYPE_DIRECTORIES
    for (i=0; (archtype==ARCHTYPE_DIRECTORIES) && (i < argc) && (argv[i]!=NULL); i++)
    {
        if ((stat64(argv[i], &st)!=0) || (!S_ISDIR(st.st_mode)))
        {   errprintf("%s is not a valid directory\n", argv[i]);
            ret=-1;
            goto do_create_error;
        }
    }
    
    // create compression threads
    for (i=0; (i<g_options.compressjobs) && (i<FSA_MAX_COMPJOBS); i++)
    {
        if (pthread_create(&thread_comp[i], NULL, thread_comp_fct, NULL) != 0)
        {   errprintf("pthread_create(thread_comp_fct) failed\n");
            ret=-1;
            goto do_create_error;
        }
    }
    
    // create archive-writer thread
    if (pthread_create(&thread_writer, NULL, thread_writer_fct, (void*)&save.ai) != 0)
    {   errprintf("pthread_create(thread_writer_fct) failed\n");
        ret=-1;
        goto do_create_error;
    }
    
    // write archive main header
    if (createar_write_mainhead(&save, archtype, argc)!=0)
    {   errprintf("archive_write_mainhead(%s) failed\n", archive);
        ret=-1;
        goto do_create_error;
    }
    
    // mount and analyse each filesystem (only if archtype==ARCHTYPE_FILESYSTEMS)
    if (archtype==ARCHTYPE_FILESYSTEMS)
    {
        // mount each partition and get filesystem information
        for (i=0; (i < argc) && (argv[i]); i++)
        {
            if ((dicofsinfo[i]=dico_alloc())==NULL)
            {   errprintf("dico_alloc() failed\n");
                goto do_create_error;
            }
            
            snprintf(devinfo[i].devpath, sizeof(devinfo[i].devpath), "%s", argv[i]);
            
            generate_random_tmpdir(devinfo[i].partmount, PATH_MAX, i);
            msgprintf(MSG_VERB2, "Mounting filesystem on %s...\n", devinfo[i].devpath);
            if (filesystem_mount_partition(&devinfo[i], dicofsinfo[i], i)!=0)
            {   msgprintf(MSG_STACK, "archive_filesystem(%s) failed\n", devinfo[i].devpath);
                goto do_create_error;
            }
        }
        
        // analyse each filesystem and write its dico
        for (i=0; (i < argc) && (argv[i]); i++)
        {
            // evaluate the cost of the operation
            cost_evalfs=0;
            msgprintf(MSG_VERB1, "Analysing filesystem on %s...\n", devinfo[i].devpath);
            if (createar_save_directory_wrapper(&save, devinfo[i].partmount, "/", &cost_evalfs)!=0)
            {   sysprintf("cannot run evaluation createar_save_directory(%s)\n", devinfo[i].partmount);
                goto do_create_error;
            }
            if (dico_add_u64(dicofsinfo[i], 0, FSYSHEADKEY_TOTALCOST, cost_evalfs)!=0)
            {   errprintf("dico_add_u64(FSYSHEADKEY_TOTALCOST) failed\n");
                goto do_create_error;
            }
            save.cost_global+=cost_evalfs;
            
            // write filesystem header
            if (queue_add_header(&g_queue, dicofsinfo[i], FSA_MAGIC_FSIN, FSA_FILESYSID_NULL)!=0)
            {   errprintf("queue_add_header(FSA_MAGIC_FSIN, %s) failed\n", devinfo[i].devpath);
                goto do_create_error;
            }
            dicofsinfo[i]=NULL;
        }
    }
    
    // analyse directories and write statistics in the dirsinfo
    if (archtype==ARCHTYPE_DIRECTORIES)
    {
        // analyse each directory to eval the cost of the operation
        for (i=0; (i < argc) && (argv[i]); i++)
        {
            cost_evalfs=0;
            msgprintf(MSG_VERB1, "Analysing directory %s...\n", argv[i]);
            if (createar_save_directory_wrapper(&save, argv[i], "/", &cost_evalfs)!=0)
            {   sysprintf("cannot run evaluation createar_save_directory(%s)\n", argv[i]);
                goto do_create_error;
            }
            save.cost_global+=cost_evalfs;
        }
        
        // write dirsinfo header
        if ((dirsinfo=dico_alloc())==NULL)
        {   errprintf("dico_alloc() failed\n");
            goto do_create_error;
        }
        if (dico_add_u64(dirsinfo, 0, DIRSINFOKEY_TOTALCOST, save.cost_global)!=0)
        {   errprintf("dico_add_u64(DIRSINFOKEY_TOTALCOST) failed\n");
            goto do_create_error;
        }
        
        if (queue_add_header(&g_queue, dirsinfo, FSA_MAGIC_DIRS, FSA_FILESYSID_NULL)!=0)
        {   errprintf("queue_add_header(FSA_MAGIC_DIRS) failed\n");
            goto do_create_error;
        }
        dirsinfo=NULL;
    }
    
    // init counters to zero before real savefs/savedir
    save.cost_current=0;
    save.objectid=0;
    
    // copy contents to archive
    switch (archtype)
    {
        case ARCHTYPE_FILESYSTEMS:// write contents of each filesystem
            for (i=0; (i < argc) && (devinfo[i].devpath!=NULL) && (get_interrupted()==false); i++)
            {
                msgprintf(MSG_VERB1, "============= archiving filesystem %s =============\n", devinfo[i].devpath);
                save.fsid=i;
                memset(&save.stats, 0, sizeof(save.stats));
                if (createar_oper_savefs(&save, &devinfo[i])!=0)
                {   errprintf("archive_filesystem(%s) failed\n", devinfo[i].devpath);
                    goto do_create_error;
                }
                if (get_interrupted()==false)
                    stats_show(save.stats, i);
                totalerr+=stats_errcount(save.stats);
            }
            break;
            
        case ARCHTYPE_DIRECTORIES: // one or several directories
            save.fsid=0; // there is no filesystem
            save.fstype=0;
            save.objectid=0;
            memset(&save.stats, 0, sizeof(save.stats));
            // write the contents of each directory passed on the command line
            for (i=0; (i < argc) && (argv[i]!=NULL) && (get_interrupted()==false); i++)
            {
                msgprintf(MSG_VERB1, "============= archiving directory %s =============\n", argv[i]);
                if (createar_oper_savedir(&save, argv[i])!=0)
                {   errprintf("archive_filesystem(%s) failed\n", argv[i]);
                    goto do_create_error;
                }
            }
            if (get_interrupted()==false)
                stats_show(save.stats, 0);
            totalerr+=stats_errcount(save.stats);
            // write "end of archive" header
            if ((dicoend=dico_alloc())==NULL)
            {   errprintf("dicoend=dico_alloc() failed\n");
                goto do_create_error;
            }
            
            // TODO: add stats about files count in that dico
            queue_add_header(&g_queue, dicoend, FSA_MAGIC_DATF, FSA_FILESYSID_NULL);
            break;
            
        default: // invalid option
            errprintf("unsupported archtype: %d\n", archtype);
            goto do_create_error;
    }
    
    if (get_interrupted()==false)
        goto do_create_success;
    if (get_abort()==true)
        msgprintf(MSG_FORCE, "operation aborted by user\n");
    
do_create_error:
    msgprintf(MSG_DEBUG1, "THREAD-MAIN1: exit error\n");
    ret=-1;
    
do_create_success:
    msgprintf(MSG_DEBUG1, "THREAD-MAIN1: exit\n");
    
    for (i=0; i < FSA_MAX_FSPERARCH; i++)
    {
        if (devinfo[i].mountedbyfsa==true)
        {
            msgprintf(MSG_VERB2, "unmounting [%s] which is mounted on [%s]\n", devinfo[i].devpath, devinfo[i].partmount);
            if (filesys[devinfo[i].fstype].umount(devinfo[i].devpath, devinfo[i].partmount)!=0)
                sysprintf("cannot umount [%s]\n", devinfo[i].partmount);
            else
                rmdir(devinfo[i].partmount); // remove temp dir created by fsarchiver
        }
    }
    
    queue_set_end_of_queue(&g_queue, true); // other threads must not wait for more data from this thread
    
    for (i=0; (i<g_options.compressjobs) && (i<FSA_MAX_COMPJOBS); i++)
        if (thread_comp[i] && pthread_join(thread_comp[i], NULL) != 0)
            errprintf("pthread_join(thread_comp[%d]) failed\n", i);
    
    if (thread_writer && pthread_join(thread_writer, NULL) != 0)
        errprintf("pthread_join(thread_writer) failed\n");
    
    if (ret!=0)
        archwriter_remove(&save.ai);
    
    // change the status if there were non-fatal errors
    if (totalerr>0)
        ret=-1;
    
    archwriter_destroy(&save.ai);
    return ret;
}
