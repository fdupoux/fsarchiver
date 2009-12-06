/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2009 Francois Dupoux.  All rights reserved.
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

#include "fsarchiver.h"

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <endian.h>
#include <unistd.h>
#include <byteswap.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <attr/xattr.h>
#include <linux/limits.h>
#include <zlib.h>

#include "dico.h"
#include "dichl.h"
#include "archive.h"
#include "common.h"
#include "create.h"
#include "md5.h"
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

typedef struct
{   carchive    ai;
    cregmulti    regmulti;
    cdichl        *dichardlinks;
    cstats        stats;
    int        fstype;
    int        fsid;
} csavear;

int createar_obj_regfile_multi(csavear *save, cdico *header, char *relpath, char *fullpath, u64 filesize)
{
    char databuf[FSA_MAX_SMALLFILESIZE];
    struct md5_ctx md5ctx;
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
    
    md5_init_ctx(&md5ctx);
    md5_process_bytes(databuf, filesize, &md5ctx);
    md5_finish_ctx(&md5ctx, md5sum);
    
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
    struct md5_ctx md5ctx;
    u32 curblocksize;
    bool eof=false;
    u64 remaining;
    char text[256];
    u8 *origblock;
    u8 md5sum[16];
    u64 filepos;
    int ret=0;
    int res;
    int fd;
    
    md5_init_ctx(&md5ctx);
    
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
        
        md5_process_bytes(origblock, curblocksize, &md5ctx);
        
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
    md5_finish_ctx(&md5ctx, md5sum);
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
    char value[65535];
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
        if (attrsize>0 && attrsize>(s64)sizeof(value))
        {   errprintf("file [%s] has an xattr [%s] with data too big (size=%ld, maxsize=64k)\n", relpath, buffer+pos, (long)attrsize);
            ret=-1;
            continue; // copy the next xattr
        }
        memset(value, 0, sizeof(value));
        errno=0;
        valsize=lgetxattr(fullpath, buffer+pos, value, sizeof(value));
        if (valsize>=0)
        {
            msgprintf(MSG_VERB2, "            xattr:lgetxattr(%s,%s)=%d: [%s]\n", relpath, buffer+pos, valsize, buffer+pos);
            msgprintf(MSG_DEBUG2, "            xattr:lgetxattr(%s)=%d: [%s]=[%s]\n", relpath, valsize, buffer+pos, value);
            msgprintf(MSG_DEBUG2, "            xattr:dico_add_string(%s, xattr): key=%d, name=[%s]\n", relpath, (int)(2*attrcnt)+0, buffer+pos);
            dico_add_string(d, DICO_OBJ_SECTION_XATTR, (2*attrcnt)+0, buffer+pos);
            msgprintf(MSG_DEBUG2, "            xattr:dico_add_string(%s, xattr): key=%d, data (size=[%d])\n", relpath, (int)(2*attrcnt)+1, valsize);
            dico_add_data(d, DICO_OBJ_SECTION_XATTR, (2*attrcnt)+1, value, valsize);
            attrcnt++;
        }
        else if (errno!=ENOATTR) // if the attribute exists and we cannot read it
        {
            sysprintf("            xattr:lgetxattr(%s,%s)=%d\n", relpath, buffer+pos, valsize);
            ret=-1;
            continue; // copy the next xattr
        }
        else if (errno==ENOATTR) // if the attribute does not exist
        {
            msgprintf(MSG_VERB2, "            xattr:lgetxattr-win(%s,%s)=-1: errno==ENOATTR\n", relpath, buffer+pos);
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
        if ((attrsize>0) && (attrsize>65535LL))
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
            msgprintf(MSG_VERB2, "            winattr:dico_add_string(%s, winattr): key=%d, data (size=[%d])\n", relpath, (int)(2*attrcnt)+1, valsize);
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
        else if (errno==ENOATTR) // if the attribute does not exist
        {
            msgprintf(MSG_VERB2, "            winattr:lgetxattr-win(%s,%s)=-1: errno==ENOATTR\n", relpath, winattr[i]);
            free(valbuf);
        }
        else
        {
            free(valbuf);
        }
    }
    
    return ret;
}

int createar_item_stdattr(csavear *save, char *root, char *relpath, struct stat64 *statbuf, cdico *d, int *objtype, u64 objectid)
{
    struct stat64 stattarget;
    char fullpath[PATH_MAX];
    char buffer[PATH_MAX];
    char buffer2[PATH_MAX];
    char directory[PATH_MAX];
    int res;
    
    // init
    *objtype=OBJTYPE_NULL;
    concatenate_paths(fullpath, sizeof(fullpath), root, relpath);
    
    msgprintf(MSG_DEBUG2, "Adding [%.5ld]=[%s]\n", (long)objectid, relpath);
    if (dico_add_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_OBJECTID, (u64)objectid)!=0)
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
            if ((readlink(fullpath, buffer, sizeof(buffer)))<0)
            {   sysprintf("readlink(%s) failed\n", fullpath);
                return -1;
            }
            dico_add_string(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_SYMLINK, buffer);
            // save type of the target of the symlink (required to recreate ntfs symlinks)
            if (filesys[save->fstype].savesymtargettype==true)
            {
                extract_dirpath(fullpath, directory, sizeof(directory));
                concatenate_paths(buffer2, sizeof(buffer2), directory, buffer);
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
                if ((statbuf->st_size > 0) && (statbuf->st_size < g_options.smallfilethresh) && (statbuf->st_nlink==1))
                    *objtype=OBJTYPE_REGFILEMULTI;
                else // case 2: file having hardlinks or larger than the threshold
                    *objtype=OBJTYPE_REGFILEUNIQUE;
                // empty files are considered as OBJTYPE_REGFILEUNIQUE (statbuf->st_size==0)
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
    
    return 0;
}

int createar_save_file(csavear *save, char *root, char *relpath, struct stat64 *statbuf, u64 objectid)
{
    char fullpath[PATH_MAX];
    cdico *dicoattr;
    int attrerrors=0;
    int objtype;
    int res;
    
    // init    
    concatenate_paths(fullpath, sizeof(fullpath), root, relpath);
    
    if (archive_is_path_to_curvol(&save->ai, fullpath)==true)
    {   errprintf("file [%s] ignored: it's the current archive file\n", fullpath);
        save->stats.err_regfile++;
        return 0; // not a fatal error, oper must continue
    }

    // ---- backup file attributes (standard + xattr)
    if ((dicoattr=dico_alloc())==NULL)
    {   errprintf("dico_alloc() failed\n");
        return -1; // fatal error
    }
    
    if (createar_item_stdattr(save, root, relpath, statbuf, dicoattr, &objtype, objectid)!=0)
    {   msgprintf(MSG_STACK, "backup_item_stdattr() failed: cannot read standard attributes on [%s]\n", relpath);
        attrerrors++;
    }
    
    if (get_interrupted()==false)
        msgprintf(MSG_VERB1, "-[%.2d][%s] %s\n", save->fsid, get_objtype_name(objtype), relpath);
    
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

int createar_save_directory(csavear *save, char *root, char *path, u64 dev, u64 *objectid)
{
    char fulldirpath[PATH_MAX];
    char fullpath[PATH_MAX];
    char relpath[PATH_MAX];
    struct stat64 statbuf;
    struct dirent *dir;
    DIR *dirdesc;
    int ret=0;
    int res;
    
    // init
    concatenate_paths(fulldirpath, sizeof(fulldirpath), root, path);
    
    if (!(dirdesc=opendir(fulldirpath)))
    {      sysprintf("cannot open directory %s\n", fulldirpath);
        return 0; // not a fatal error, oper must continue
    }
    
    // backup the directory itself (important for the root of the filesystem)
    if (lstat64(fulldirpath, &statbuf)!=0)
    {   sysprintf("cannot lstat64(%s)\n", fulldirpath);
        ret=-1;
        goto backup_dir_err;
    }
    
    // save info about the directory itself
    if (createar_save_file(save, root, path, &statbuf, (*objectid)++)!=0)
    {   errprintf("createar_save_file(%s,%s) failed\n", root, path);
        ret=-1;
        goto backup_dir_err;
    }
    
    while (((dir = readdir(dirdesc)) != NULL) && (get_interrupted()==false))
    {
        // ---- calculate paths
        concatenate_paths(relpath, sizeof(relpath), path, dir->d_name);
        concatenate_paths(fullpath, sizeof(fullpath), fulldirpath, dir->d_name);
        if (lstat64(fullpath, &statbuf)!=0)
        {   sysprintf("cannot lstat64(%s)\n", fullpath);
            ret=-1;
            goto backup_dir_err;
        }
        
        // ---- ignore "." and ".." and ignore mount-points
        if (strcmp(dir->d_name,".")==0 || strcmp(dir->d_name,"..")==0)
            continue; // ignore "." and ".."
        
        // ---- if dev!=0 (when we backup a filesystem not a dir), ignore all other devices
        if ((dev!=0) && (u64)(statbuf.st_dev)!=dev)
        {
            // copy directories which are used as mount-points (important especially for /dev, /proc, /sys)
            if (S_ISDIR(statbuf.st_mode))
            {
                if (createar_save_file(save, root, relpath, &statbuf, (*objectid)++)!=0)
                {   ret=-1;
                    errprintf("createar_save_file(%s,%s) failed\n", root, path);
                    goto backup_dir_err;
                }
                
                // copy the files in /dev/ during a live-backup (option -A) else /dev files such as /dev/console would be missing
                if (g_options.allowsaverw==true && strcmp(relpath, "/dev")==0)
                {   
                    if (createar_save_directory(save, root, relpath, statbuf.st_dev, objectid)!=0)
                    {   ret=-1;
                        msgprintf(MSG_STACK, "createar_save_directory(%s) failed\n", relpath);
                        goto backup_dir_err;
                    }
                }
            }
            
            continue; // don't copy the contents of mount-points directories: it's another filesystem
        }
        
        // backup contents before the directory itself so that the dir-attributes are written after the dir contents
        if (S_ISDIR(statbuf.st_mode))
        {
            // if "dev==0" then accept to go on other filesystems (archtype=ARCHTYPE_DIRECTORIES)
            // we keep the same dichardlinks even if the filesystem is different (with different inodes numbers)
            // since the cdichl is able to manage several filesystems (its key1)
            if ((dev==0) && (u64)(statbuf.st_dev)!=dev) // cross filesystems when archtype=ARCHTYPE_DIRECTORIES
            {
                res=createar_save_directory(save, root, relpath, statbuf.st_dev, objectid);
            }
            else // we are still on the same filesystem
            {
                res=createar_save_directory(save, root, relpath, dev, objectid);
            }
            if (res!=0)
            {   msgprintf(MSG_STACK, "createar_save_directory(%s) failed\n", relpath);
                ret=-1;
                goto backup_dir_err;
            }
        }
        else // not a directory
        {
            if (createar_save_file(save, root, relpath, &statbuf, (*objectid)++)!=0)
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

int createar_write_mainhead(csavear *save, char *label)
{
    struct timeval now;
    cdico *d;
    
    if (!save)
    {   errprintf("ai is NULL\n");
        return -1;
    }
    
    // init
    gettimeofday(&now, NULL);
    save->ai.creattime=now.tv_sec;
    if ((d=dico_alloc())==NULL)
    {   errprintf("dico_alloc() failed\n");
        return -1;
    }
    
    dico_add_string(d, 0, MAINHEADKEY_FILEFORMATVER, FSA_FILEFORMAT);
    dico_add_string(d, 0, MAINHEADKEY_PROGVERCREAT, FSA_VERSION);
    dico_add_string(d, 0, MAINHEADKEY_ARCHLABEL, label);
    dico_add_u64(d, 0, MAINHEADKEY_CREATTIME, save->ai.creattime);
    dico_add_u32(d, 0, MAINHEADKEY_ARCHIVEID, save->ai.archid);
    dico_add_u32(d, 0, MAINHEADKEY_ARCHTYPE, save->ai.archtype);
    dico_add_u32(d, 0, MAINHEADKEY_COMPRESSALGO, save->ai.compalgo);
    dico_add_u32(d, 0, MAINHEADKEY_COMPRESSLEVEL, save->ai.complevel);
    dico_add_u32(d, 0, MAINHEADKEY_ENCRYPTALGO, save->ai.cryptalgo);
    dico_add_u32(d, 0, MAINHEADKEY_FSACOMPLEVEL, save->ai.fsacomp);
    
    if (save->ai.archtype==ARCHTYPE_FILESYSTEMS)
    {   
        dico_add_u64(d, 0, MAINHEADKEY_FSCOUNT, save->ai.fscount);
    }
    
    // if encryption is enabled, save the md5sum of a random buffer to check the password
#ifdef OPTION_CRYPTO_SUPPORT
    u8 bufcheckclear[FSA_CHECKPASSBUF_SIZE+8];
    u8 bufcheckcrypt[FSA_CHECKPASSBUF_SIZE+8];
    u64 cryptsize;
    struct md5_ctx md5ctx;
    u8 md5sum[16];
    if (save->ai.cryptalgo!=ENCRYPT_NONE)
    {
        memset(md5sum, 0, sizeof(md5sum));
        crypto_random(bufcheckclear, FSA_CHECKPASSBUF_SIZE);
        crypto_blowfish(FSA_CHECKPASSBUF_SIZE, &cryptsize, bufcheckclear, bufcheckcrypt, 
            g_options.encryptpass, strlen((char*)g_options.encryptpass), true);
        
        md5_init_ctx(&md5ctx);
        md5_process_bytes(bufcheckclear, FSA_CHECKPASSBUF_SIZE, &md5ctx);
        md5_finish_ctx(&md5ctx, md5sum);
        
        assert(dico_add_data(d, 0, MAINHEADKEY_BUFCHECKPASSCLEARMD5, md5sum, 16)==0);
        assert(dico_add_data(d, 0, MAINHEADKEY_BUFCHECKPASSCRYPTBUF, bufcheckcrypt, FSA_CHECKPASSBUF_SIZE)==0);
    }
#endif // OPTION_CRYPTO_SUPPORT
    
    if (queue_add_header(&g_queue, d, FSA_MAGIC_MAIN, FSA_FILESYSID_NULL)!=0)
    {   errprintf("cannot write dico for main header\n");
        dico_destroy(d);
        return -1;
    }
    
    return 0;
}

int filesystem_mount_partition(cdico *dicofs, char *partition, char *partmnt, int *fstype, u16 fsid, bool *mntbyfsa)
{
    char fsbuf[FSA_MAX_FSNAMELEN];
    struct statvfs64 statfsbuf;
    cstrlist reqmntopt;
    cstrlist badmntopt;
    cstrlist curmntopt;
    int showwarningcount1=0;
    int showwarningcount2=0;
    char temp[PATH_MAX];
    char optbuf[128];
    u64 fsbytestotal;
    u64 fsbytesused;
    int readwrite;
    int count;
    int res;
    int i;
    
    res=generic_get_mntinfo(partition, &readwrite, temp, sizeof(temp), optbuf, sizeof(optbuf), fsbuf, sizeof(fsbuf));
    if (res==0) // partition is already mounted
    {
        *mntbyfsa=false;
        snprintf(partmnt, PATH_MAX, "%s", temp); // return the mount point to main savefs function
        msgprintf(MSG_DEBUG1, "generic_get_mntinfo(%s): mnt=[%s], opt=[%s], fs=[%s], rw=[%d]\n", partition, partmnt, optbuf, fsbuf, readwrite);
        if (readwrite==1 && g_options.allowsaverw==0)
        {
            errprintf("partition [%s] is mounted read/write. please mount it read-only \n"
                "and then try again. you can do \"mount -o remount,ro %s\". you can \n"
                "also run fsarchiver with option '-A' if you know what you are doing.\n", partition, partition);
            return -1;
        }
        if (generic_get_fstype(fsbuf, fstype)!=0)
        {   
            if (strcmp(fsbuf, "fuseblk")==0)
                errprintf("partition [%s] is using a fuse based filesystem (probably ntfs-3g). Unmount it and try again\n", partition);
            else
                errprintf("filesystem of partition [%s] is not supported by fsarchiver: filesystem=[%s]\n", partition, fsbuf);
            return -1;
        }
        // check the filesystem is mounted with the right mount-options (to preserve acl and xattr)
        strlist_init(&reqmntopt);
        strlist_init(&badmntopt);
        strlist_init(&curmntopt);
        if (filesys[*fstype].reqmntopt(partition, &reqmntopt, &badmntopt)!=0)
        {
            errprintf("cannot get the required mount options for partition=[%s]\n", partition);
            strlist_empty(&reqmntopt);
            strlist_empty(&badmntopt);
            return -1;
        }
        strlist_split(&curmntopt, optbuf, ',');
        msgprintf(MSG_DEBUG2, "mount options found for partition=[%s]: [%s]\n", partition, strlist_merge(&curmntopt, temp, sizeof(temp), ','));
        msgprintf(MSG_DEBUG2, "mount options required for partition=[%s]: [%s]\n", partition, strlist_merge(&reqmntopt, temp, sizeof(temp), ','));
        msgprintf(MSG_DEBUG2, "mount options to avoid for partition=[%s]: [%s]\n", partition, strlist_merge(&badmntopt, temp, sizeof(temp), ','));
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
                        partition, strlist_merge(&reqmntopt, temp, sizeof(temp), ','));
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
                        partition, strlist_merge(&badmntopt, temp, sizeof(temp), ','));
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
    }
    else // partition not yet mounted
    {
        mkdir_recursive(partmnt);
        msgprintf(MSG_DEBUG1, "partition %s is not mounted\n", partition);
        for (*fstype=-1, i=0; (filesys[i].name) && (*fstype==-1); i++)
        {
            if ((filesys[i].test(partition)==true) && (filesys[i].mount(partition, partmnt, filesys[i].name, MS_RDONLY, NULL)==0))
            {   *fstype=i;
                msgprintf(MSG_DEBUG1, "partition %s successfully mounted on [%s] as [%s]\n", partition, partmnt, filesys[i].name);
            }
        }
        if (*fstype==-1)
        {   errprintf("can't detect and mount filesystem of partition [%s], cannot continue.\n", partition);
            return -1;
        }
        *mntbyfsa=true;
    }
    
    // get space statistics
    if (statvfs64(partmnt, &statfsbuf)!=0)
    {   errprintf("statvfs64(%s) failed\n", partmnt);
        return -1;
    }
    fsbytestotal=(u64)statfsbuf.f_frsize*(u64)statfsbuf.f_blocks;
    fsbytesused=fsbytestotal-((u64)statfsbuf.f_frsize*(u64)statfsbuf.f_bfree);
    
    dico_add_string(dicofs, 0, FSYSHEADKEY_FILESYSTEM, filesys[*fstype].name);
    dico_add_string(dicofs, 0, FSYSHEADKEY_MNTPATH, partmnt);
    dico_add_string(dicofs, 0, FSYSHEADKEY_ORIGDEV, partition);
    dico_add_u64(dicofs, 0, FSYSHEADKEY_BYTESTOTAL, fsbytestotal);
    dico_add_u64(dicofs, 0, FSYSHEADKEY_BYTESUSED, fsbytesused);
    
    if (filesys[*fstype].getinfo(dicofs, partition)!=0)
    {   errprintf("cannot save filesystem attributes for partition %s\n", partition);
        return -1;
    }
    
    return 0;
}

int createar_oper_savefs(csavear *save, char *partition, char *partmount, int fstype, bool mountedbyfsa)
{
    cdico *dicobegin=NULL;
    cdico *dicoend=NULL;
    struct stat64 statbuf;
    //struct s_filesysdat fsdat;
    u64 objectid=0;
    int ret=0;
    
    // write "begin of filesystem" header
    if ((dicobegin=dico_alloc())==NULL)
    {   errprintf("dicostart=dico_alloc() failed\n");
        return -1;
    }
    // TODO: add stats about files count in that dico
    queue_add_header(&g_queue, dicobegin, FSA_MAGIC_FSYB, save->fsid);
    
    // get dev ids of the filesystem to ignore mount points
    if (lstat64(partmount, &statbuf)!=0)
    {   sysprintf("cannot lstat64(%s)\n", partmount);
        return -1;
    }
    
    // init filesystemdata struct
    save->fstype=fstype;
    regmulti_init(&save->regmulti, g_options.datablocksize);
    if ((save->dichardlinks=dichl_alloc())==NULL)
    {   errprintf("dichardlinks=dichl_alloc() failed\n");
        return -1;
    }
    
    ret=createar_save_directory(save, partmount, "/", (u64)statbuf.st_dev, &objectid);
    
    // put all small files that are in the last block to the queue
    if (regmulti_save_enqueue(&save->regmulti, &g_queue, save->fsid)!=0)
    {   errprintf("Cannot queue last block of small-files\n");
        return -1;
    }
    
    // dico for hard links not required anymore
    dichl_destroy(save->dichardlinks);
    
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
    u64 objectid=0;
    
    // init filesystemdata struct
    regmulti_init(&save->regmulti, g_options.datablocksize);
    
    if ((save->dichardlinks=dichl_alloc())==NULL)
    {   errprintf("dichardlinks=dichl_alloc() failed\n");
        return -1;
    }
    
    if (rootdir[0]=='/') // absolute path
    {
        snprintf(fullpath, sizeof(fullpath), "%s", rootdir);
        createar_save_directory(save, "/", fullpath, (u64)0, &objectid);
    }
    else // relative path
    {
        concatenate_paths(fullpath, sizeof(fullpath), getcwd(currentdir, sizeof(currentdir)), rootdir);
        createar_save_directory(save, ".", rootdir, (u64)0, &objectid);
        // put all small files that are in the last block to the queue
    }
    
    // put all small files that are in the last block to the queue
    if (regmulti_save_enqueue(&save->regmulti, &g_queue, 0)!=0)
    {   errprintf("Cannot queue last block of small-files\n");
        return -1;
    }
    
    // dico for hard links not required anymore
    dichl_destroy(save->dichardlinks);
    
    return 0;
}

int do_create(char *archive, char **partition, int fscount, int archtype)
{
    cdico *dicofsinfo[FSA_MAX_FSPERARCH];
    char partmounts[PATH_MAX][FSA_MAX_FSPERARCH];
    pthread_t thread_comp[FSA_MAX_COMPJOBS];
    bool mountedbyfsa[FSA_MAX_FSPERARCH];
    int fstype[FSA_MAX_FSPERARCH];
    pthread_t thread_writer;
    u64 totalerr=0;
    cdico *dicoend;
    struct stat64 st;
    csavear save;
    int ret=0;
    int i;
    
    // init archive
    archive_init(&save.ai);
    archive_generate_id(&save.ai);
    
    // pass options to archive
    path_force_extension(save.ai.basepath, PATH_MAX, archive, ".fsa");
    save.ai.cryptalgo=g_options.encryptalgo;
    save.ai.compalgo=g_options.compressalgo;
    save.ai.complevel=g_options.compresslevel;
    save.ai.fsacomp=g_options.fsacomplevel;
    save.ai.fscount=fscount;
    save.ai.archtype=archtype;
    
    // init misc data struct to zero
    thread_writer=0;
    for (i=0; i<FSA_MAX_COMPJOBS; i++)
        thread_comp[i]=0;
    for (i=0; i<FSA_MAX_FSPERARCH; i++)
        fstype[i]=-1;
    for (i=0; i<FSA_MAX_FSPERARCH; i++)
        memset(partmounts[i], 0, PATH_MAX);
    for (i=0; i<FSA_MAX_FSPERARCH; i++)
        mountedbyfsa[i]=false;
    
    // check that arguments are all block devices when archtype==ARCHTYPE_FILESYSTEMS
    for (i=0; (archtype==ARCHTYPE_FILESYSTEMS) && (i < fscount) && (partition[i]!=NULL); i++)
    {
        if ((stat64(partition[i], &st)!=0) || (!S_ISBLK(st.st_mode)))
        {   errprintf("%s is not a valid block device\n", partition[i]);
            ret=-1;
            goto do_create_error;
        }
    }
    
    // check that arguments are all directories when archtype==ARCHTYPE_DIRECTORIES
    for (i=0; (archtype==ARCHTYPE_DIRECTORIES) && (i < fscount) && (partition[i]!=NULL); i++)
    {
        if ((stat64(partition[i], &st)!=0) || (!S_ISDIR(st.st_mode)))
        {   errprintf("%s is not a valid directory\n", partition[i]);
            ret=-1;
            goto do_create_error;
        }
    }
    
    // create compression threads
    for (i=0; (i<g_options.compressjobs) && (i<FSA_MAX_COMPJOBS); i++)
    {
        if (pthread_create(&thread_comp[i], NULL, thread_comp_fct, (void*)&save.ai) != 0)
        {      errprintf("pthread_create(thread_comp_fct) failed\n");
            ret=-1;
            goto do_create_error;
        }
    }
    
    // create archive-writer thread
    if (pthread_create(&thread_writer, NULL, thread_writer_fct, (void*)&save.ai) != 0)
    {      errprintf("pthread_create(thread_writer_fct) failed\n");
        ret=-1;
        goto do_create_error;
    }
    
    // write archive main header
    if (createar_write_mainhead(&save, "archive-label")!=0)
    {      errprintf("archive_write_mainhead(%s) failed\n", archive);
        ret=-1;
        goto do_create_error;
    }
    
    // write one fsinfo header for each filesystem (only if archtype==ARCHTYPE_FILESYSTEMS)
    for (i=0; (archtype==ARCHTYPE_FILESYSTEMS) && (i < fscount) && (partition[i]); i++)
    {
        if ((dicofsinfo[i]=dico_alloc())==NULL)
        {   errprintf("dico_alloc() failed\n");
            return -1;
        }
        generate_random_tmpdir(partmounts[i], PATH_MAX, i);
        if (filesystem_mount_partition(dicofsinfo[i], partition[i], partmounts[i], &fstype[i], i, &mountedbyfsa[i])!=0)
        {   msgprintf(MSG_STACK, "archive_filesystem(%s) failed\n", partition[i]);
            goto do_create_error;
        }
        if (queue_add_header(&g_queue, dicofsinfo[i], FSA_MAGIC_FSIN, FSA_FILESYSID_NULL)!=0)
        {   errprintf("queue_add_header(FSA_MAGIC_FSIN, %s) failed\n", partition[i]);
            goto do_create_error;
        }
    }
    
    // copy contents to archive
    switch (archtype)
    {
        case ARCHTYPE_FILESYSTEMS:// write contents of each filesystem
            for (i=0; (i < fscount) && (partition[i]!=NULL) && (get_interrupted()==false); i++)
            {
                msgprintf(MSG_VERB1, "============= archiving filesystem %s =============\n", partition[i]);
                save.fsid=i;
                memset(&save.stats, 0, sizeof(save.stats));
                if (createar_oper_savefs(&save, partition[i], partmounts[i], fstype[i], mountedbyfsa[i])!=0)
                {   errprintf("archive_filesystem(%s) failed\n", partition[i]);
                    goto do_create_error;
                }
                if (get_interrupted()==false)
                    stats_show(save.stats, i);
                totalerr+=stats_errcount(save.stats);
            }
            break;
            
        case ARCHTYPE_DIRECTORIES: // one or several directories
            // there is no filesystem
            save.fsid=0;
            save.fstype=0;
            memset(&save.stats, 0, sizeof(save.stats));
            // write the contents of each directory passed on the command line
            for (i=0; (i < fscount) && (partition[i]!=NULL) && (get_interrupted()==false); i++)
            {
                msgprintf(MSG_VERB1, "============= archiving directory %s =============\n", partition[i]);
                if (createar_oper_savedir(&save, partition[i])!=0)
                {   errprintf("archive_filesystem(%s) failed\n", partition[i]);
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
    
    for (i=0; (i < FSA_MAX_FSPERARCH) && (partmounts[i]); i++)
    {
        if (mountedbyfsa[i]==true)
        {
            msgprintf(MSG_VERB2, "unmounting [%s] which is mounted on [%s]\n", partition[i], partmounts[i]);
            if (filesys[fstype[i]].umount(partition[i], partmounts[i])!=0)
                sysprintf("cannot umount [%s]\n", partmounts[i]);
            else
                rmdir(partmounts[i]); // remove temp dir created by fsarchiver
        }
    }
    
    queue_set_end_of_queue(&g_queue, true); // other threads must not wait for more data from this thread
    
    for (i=0; (i<g_options.compressjobs) && (i<FSA_MAX_COMPJOBS); i++)
        if (thread_comp[i] && pthread_join(thread_comp[i], NULL) != 0)
            errprintf("pthread_join(thread_comp[%d]) failed\n", i);
    
    if (thread_writer && pthread_join(thread_writer, NULL) != 0)
        errprintf("pthread_join(thread_writer) failed\n");
    
    if (ret!=0)
        archive_remove(&save.ai);
    
    // change the status if there were non-fatal errors
    if (totalerr>0)
        ret=-1;
    
    archive_destroy(&save.ai);
    return ret;
}
