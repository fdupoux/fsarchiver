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

#include <stdio.h>
#include <ext2fs.h>
#include <blkid.h>
#include <e2p.h>
#include <uuid.h>

#include "fsarchiver.h"
#include "dico.h"
#include "common.h"
#include "strlist.h"
#include "filesys.h"
#include "fs_ext2.h"
#include "error.h"

// e2fsprogs version required to work on ext2, ext3, ext4
u64 e2fsprogs_minver[]={PROGVER(1,39,0), PROGVER(1,39,0), PROGVER(1,41,0)};

struct mntopt
{   unsigned int   mask;
    const char     *string;
};

struct s_features
{
    char   *name; // name of that feature
    int    mask; // identifier for that feature
    int    compat; // compat type for that feature (see e2p.h)
    int    firstfs; // type of the first filesystem to support it
    u64    firste2p; // first e2fsprogs version that supports that feature
};

// TODO: check the real mke2fs version that supports the features
struct s_features mkfeatures[] = // cf e2fsprogs-1.42.3/lib/e2p/feature.c
{
    {"has_journal",  FSA_EXT3_FEATURE_COMPAT_HAS_JOURNAL,     E2P_FEATURE_COMPAT,      EXTFSTYPE_EXT3, PROGVER(1,39,0)},
    {"ext_attr",     FSA_EXT2_FEATURE_COMPAT_EXT_ATTR,        E2P_FEATURE_COMPAT,      EXTFSTYPE_EXT2, PROGVER(1,40,5)},
    {"resize_inode", FSA_EXT2_FEATURE_COMPAT_RESIZE_INODE,    E2P_FEATURE_COMPAT,      EXTFSTYPE_EXT2, PROGVER(1,39,0)},
    {"dir_index",    FSA_EXT2_FEATURE_COMPAT_DIR_INDEX,       E2P_FEATURE_COMPAT,      EXTFSTYPE_EXT2, PROGVER(1,33,0)},
    {"filetype",     FSA_EXT2_FEATURE_INCOMPAT_FILETYPE,      E2P_FEATURE_INCOMPAT,    EXTFSTYPE_EXT2, PROGVER(1,16,0)},
    {"extent",       FSA_EXT4_FEATURE_INCOMPAT_EXTENTS,       E2P_FEATURE_INCOMPAT,    EXTFSTYPE_EXT4, PROGVER(1,41,0)},
    {"journal_dev",  FSA_EXT3_FEATURE_INCOMPAT_JOURNAL_DEV,   E2P_FEATURE_INCOMPAT,    EXTFSTYPE_EXT3, PROGVER(1,39,0)},
    {"flex_bg",      FSA_EXT4_FEATURE_INCOMPAT_FLEX_BG,       E2P_FEATURE_INCOMPAT,    EXTFSTYPE_EXT4, PROGVER(1,41,0)},
    {"meta_bg",      FSA_EXT2_FEATURE_INCOMPAT_META_BG,       E2P_FEATURE_INCOMPAT,    EXTFSTYPE_EXT2, PROGVER(1,39,0)},
    {"mmp",          FSA_EXT4_FEATURE_INCOMPAT_MMP,           E2P_FEATURE_INCOMPAT,    EXTFSTYPE_EXT4, PROGVER(1,42,0)},
    {"large_file",   FSA_EXT2_FEATURE_RO_COMPAT_LARGE_FILE,   E2P_FEATURE_RO_INCOMPAT, EXTFSTYPE_EXT2, PROGVER(1,40,7)},
    {"huge_file",    FSA_EXT4_FEATURE_RO_COMPAT_HUGE_FILE,    E2P_FEATURE_RO_INCOMPAT, EXTFSTYPE_EXT4, PROGVER(1,41,0)},
    {"sparse_super", FSA_EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER, E2P_FEATURE_RO_INCOMPAT, EXTFSTYPE_EXT2, PROGVER(1,8,0)},
    {"uninit_bg",    FSA_EXT4_FEATURE_RO_COMPAT_GDT_CSUM,     E2P_FEATURE_RO_INCOMPAT, EXTFSTYPE_EXT4, PROGVER(1,41,0)},
    {"dir_nlink",    FSA_EXT4_FEATURE_RO_COMPAT_DIR_NLINK,    E2P_FEATURE_RO_INCOMPAT, EXTFSTYPE_EXT4, PROGVER(1,41,0)},
    {"extra_isize",  FSA_EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE,  E2P_FEATURE_RO_INCOMPAT, EXTFSTYPE_EXT4, PROGVER(1,41,0)},
    {"bigalloc",     FSA_EXT4_FEATURE_RO_COMPAT_BIGALLOC,     E2P_FEATURE_RO_INCOMPAT, EXTFSTYPE_EXT4, PROGVER(1,42,0)},
    {NULL,           0,                                       0,                       0,              0},
};

char *format_fstype(int fstype)
{
    switch (fstype)
    {
        case EXTFSTYPE_EXT2: return "ext2";
        case EXTFSTYPE_EXT3: return "ext3";
        case EXTFSTYPE_EXT4: return "ext4";
        default: return "invalid";
    }
}

int ext2_mkfs(cdico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid)
{
    return extfs_mkfs(d, partition, EXTFSTYPE_EXT2, fsoptions, mkfslabel, mkfsuuid);
}

int ext3_mkfs(cdico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid)
{
    return extfs_mkfs(d, partition, EXTFSTYPE_EXT3, fsoptions, mkfslabel, mkfsuuid);
}

int ext4_mkfs(cdico *d, char *partition, char *fsoptions, char *mkfslabel, char *mkfsuuid)
{
    return extfs_mkfs(d, partition, EXTFSTYPE_EXT4, fsoptions, mkfslabel, mkfsuuid);
}

int extfs_get_fstype_from_compat_flags(u32 compat, u32 incompat, u32 ro_compat)
{
    int fstype=EXTFSTYPE_EXT2;
    
    // distinguish between ext3 and ext2
    if (compat & FSA_EXT3_FEATURE_COMPAT_HAS_JOURNAL)
        fstype=EXTFSTYPE_EXT3;
    
    // any features which ext2 doesn't understand
    if ((ro_compat & FSA_EXT2_FEATURE_RO_COMPAT_UNSUPPORTED) || (incompat & FSA_EXT2_FEATURE_INCOMPAT_UNSUPPORTED))
        fstype=EXTFSTYPE_EXT3;
    
    // ext4 has at least one feature which ext3 doesn't understand
    if ((ro_compat & FSA_EXT3_FEATURE_RO_COMPAT_UNSUPPORTED) || (incompat & FSA_EXT3_FEATURE_INCOMPAT_UNSUPPORTED))
        fstype=EXTFSTYPE_EXT4;
    
    return fstype;
}

int extfs_check_compatibility(u64 compat, u64 incompat, u64 ro_compat)
{
    // to preserve the filesystem attributes, fsa must know all the features including the COMPAT ones
    if (compat & ~FSA_FEATURE_COMPAT_SUPP)
        return -1;
    
    if (incompat & ~FSA_FEATURE_INCOMPAT_SUPP)
        return -1;
    
    if (ro_compat & ~FSA_FEATURE_RO_COMPAT_SUPP)
        return -1;
    
    // TODO: check journal features
    /*if (!(flags & EXT2_FLAG_JOURNAL_DEV_OK) && (fs->super->s_feature_incompat & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV)) 
        goto check_support_for_features_error;*/
    
    return 0;
}

int extfs_mkfs(cdico *d, char *partition, int extfstype, char *fsoptions, char *mkfslabel, char *mkfsuuid)
{
    cstrlist strfeatures;
    u64 features_tab[3];
    u64 fsextrevision;
    int origextfstype;
    char buffer[2048];
    char command[2048];
    char options[2048];
    char temp[1024];
    char progname[64];
    u64 e2fstoolsver;
    int compat_type;
    u64 temp64;
    int exitst;
    int ret=0;
    int res;
    int i;
    
    // init    
    memset(options, 0, sizeof(options));
    snprintf(progname, sizeof(progname), "mke2fs");
    strlist_init(&strfeatures);
    
    // ---- check that mkfs is installed and get its version
    if (exec_command(command, sizeof(command), NULL, NULL, 0, NULL, 0, "%s -V", progname)!=0)
    {   errprintf("%s not found. please install a recent e2fsprogs on your system or check the PATH.\n", progname);
        ret=-1;
        goto extfs_mkfs_cleanup;
    }
    e2fstoolsver=check_prog_version(progname);
    
    // ---- filesystem revision (good-old-rev or dynamic)
    if (dico_get_u64(d, 0, FSYSHEADKEY_FSEXTREVISION, &fsextrevision)!=0)
        fsextrevision=EXT2_DYNAMIC_REV; // don't fail (case of fs conversion to extfs)
    
    // "mke2fs -q" prevents problems in exec_command when too many output details printed
    strlcatf(options, sizeof(options), " -q ");
    
    // "mke2fs -F" removes confirmation prompt when device is a whole disk such as /dev/sda
    strlcatf(options, sizeof(options), " -F ");

    // filesystem revision: good-old-rev or dynamic
    strlcatf(options, sizeof(options), " -r %d ", (int)fsextrevision);

    strlcatf(options, sizeof(options), " %s ", fsoptions);
    
    // ---- set the advanced filesystem settings from the dico
    if (strlen(mkfslabel) > 0)
        strlcatf(options, sizeof(options), " -L '%.16s' ", mkfslabel);
    else if (dico_get_string(d, 0, FSYSHEADKEY_FSLABEL, buffer, sizeof(buffer))==0 && strlen(buffer)>0)
        strlcatf(options, sizeof(options), " -L '%.16s' ", buffer);
    
    if (dico_get_u64(d, 0, FSYSHEADKEY_FSEXTBLOCKSIZE, &temp64)==0)
        strlcatf(options, sizeof(options), " -b %ld ", (long)temp64);
    
    if (dico_get_u64(d, 0, FSYSHEADKEY_FSINODESIZE, &temp64)==0)
        strlcatf(options, sizeof(options), " -I %ld ", (long)temp64);
    
    // ---- get original filesystem features (if the original filesystem was an ext{2,3,4})
    if (dico_get_u64(d, 0, FSYSHEADKEY_FSEXTFEATURECOMPAT, &features_tab[E2P_FEATURE_COMPAT])!=0 ||
        dico_get_u64(d, 0, FSYSHEADKEY_FSEXTFEATUREINCOMPAT, &features_tab[E2P_FEATURE_INCOMPAT])!=0 ||
        dico_get_u64(d, 0, FSYSHEADKEY_FSEXTFEATUREROCOMPAT, &features_tab[E2P_FEATURE_RO_INCOMPAT])!=0)
    {   // dont fail the original filesystem may not be ext{2,3,4}. in that case set defaults features
        features_tab[E2P_FEATURE_COMPAT]=EXT2_FEATURE_COMPAT_RESIZE_INODE|EXT2_FEATURE_COMPAT_DIR_INDEX;
        features_tab[E2P_FEATURE_INCOMPAT]=EXT2_FEATURE_INCOMPAT_FILETYPE;
        features_tab[E2P_FEATURE_RO_INCOMPAT]=EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;
    }
    
    // ---- check that fsarchiver is aware of all the filesystem features used on that filesystem
    if (extfs_check_compatibility(features_tab[E2P_FEATURE_COMPAT], features_tab[E2P_FEATURE_INCOMPAT], features_tab[E2P_FEATURE_RO_INCOMPAT])!=0)
    {   errprintf("this filesystem has ext{2,3,4} features which are not supported by this fsarchiver version.\n");
        return -1;
    }
    
    // ---- get original filesystem type
    origextfstype=extfs_get_fstype_from_compat_flags(features_tab[E2P_FEATURE_COMPAT], 
            features_tab[E2P_FEATURE_INCOMPAT], features_tab[E2P_FEATURE_RO_INCOMPAT]);
    msgprintf(MSG_VERB2, "the filesystem type determined by the original filesystem features is [%s]\n", format_fstype(origextfstype));
    
    // remove all the features not supported by the filesystem to create (conversion = downgrade fs)
    for (i=0; mkfeatures[i].name; i++)
    {
        compat_type=mkfeatures[i].compat;
        if (mkfeatures[i].firstfs > extfstype)
            features_tab[compat_type] &= ~mkfeatures[i].mask;
    }
    
    // add new features if the filesystem to create is newer than the filesystem type that was backed up
    // eg: user did a "savefs" of an ext3 and does a "restfs mkfs=ext4" --> add features to force ext4
    // it's a bit more difficult because we only want to add such a feature if no feature of the new
    // filesystem is currently enabled.
    msgprintf(MSG_VERB2, "the filesystem type to create considering the command options is [%s]\n", format_fstype(extfstype));
    if (origextfstype==EXTFSTYPE_EXT2 && extfstype>EXTFSTYPE_EXT2) // upgrade ext2 to ext{3,4}
    {   fsextrevision=EXT2_DYNAMIC_REV;
        features_tab[E2P_FEATURE_COMPAT]|=EXT3_FEATURE_COMPAT_HAS_JOURNAL;
    }
    if (origextfstype<EXTFSTYPE_EXT4 && extfstype>=EXTFSTYPE_EXT4) // upgrade ext{2,3} to ext4
    {   fsextrevision=EXT2_DYNAMIC_REV;
        features_tab[E2P_FEATURE_INCOMPAT]|=EXT3_FEATURE_INCOMPAT_EXTENTS;
    }
    
    // convert int features to string to be passed to mkfs
    for (i=0; mkfeatures[i].name; i++)
    {
        if (mkfeatures[i].firste2p<=e2fstoolsver) // don't pass an option to a program that does not support it
        {
            compat_type=mkfeatures[i].compat;
            if (features_tab[compat_type] & mkfeatures[i].mask)
            {   msgprintf(MSG_VERB2, "--> feature [%s]=YES\n", mkfeatures[i].name);
                strlist_add(&strfeatures, mkfeatures[i].name);
            }
            else
            {   msgprintf(MSG_VERB2, "--> feature [%s]=NO\n", mkfeatures[i].name);
                snprintf(temp, sizeof(temp), "^%s", mkfeatures[i].name); // exclude feature
                strlist_add(&strfeatures, temp);
            }
        }
    }
    
    // if extfs revision is dynamic and there are features in the list
    if (fsextrevision!=EXT2_GOOD_OLD_REV && strlist_count(&strfeatures)>0)
    {   strlist_merge(&strfeatures, temp, sizeof(temp), ',');
        strlcatf(options, sizeof(options), " -O %s ", temp);
        msgprintf(MSG_VERB2, "features: mkfs_options+=[-O %s]\n", temp);
    }
    
    // ---- check mke2fs version requirement
    msgprintf(MSG_VERB2, "mke2fs version detected: %s\n", format_prog_version(e2fstoolsver, temp, sizeof(temp)));
    msgprintf(MSG_VERB2, "mke2fs version required: %s\n", format_prog_version(e2fsprogs_minver[extfstype], temp, sizeof(temp)));
    if (e2fstoolsver < e2fsprogs_minver[extfstype])
    {   errprintf("mke2fs was found but is too old, please upgrade to a version %s or more recent.\n", 
            format_prog_version(e2fsprogs_minver[extfstype], temp, sizeof(temp)));
        ret=-1;
        goto extfs_mkfs_cleanup;
    }
    
    // ---- extended options
    if (dico_get_u64(d, 0, FSYSHEADKEY_FSEXTEOPTRAIDSTRIDE, &temp64)==0)
        strlcatf(options, sizeof(options), " -E stride=%ld ", (long)temp64);
    if ((dico_get_u64(d, 0, FSYSHEADKEY_FSEXTEOPTRAIDSTRIPEWIDTH, &temp64)==0) && e2fstoolsver>=PROGVER(1,40,7))
        strlcatf(options, sizeof(options), " -E stripe-width=%ld ", (long)temp64);
    
    // ---- execute mke2fs
    msgprintf(MSG_VERB2, "exec: %s\n", command);
    if (exec_command(command, sizeof(command), &exitst, NULL, 0, NULL, 0, "%s %s %s", progname, partition, options)!=0 || exitst!=0)
    {   errprintf("command [%s] failed with return status=%d\n", command, exitst);
        ret=-1;
        goto extfs_mkfs_cleanup;
    }
    
    // ---- use tune2fs to set the other advanced options
    memset(options, 0, sizeof(options));
    if (strlen(mkfsuuid) > 0)
        strlcatf(options, sizeof(options), " -U %s ", mkfsuuid);
    else if (dico_get_string(d, 0, FSYSHEADKEY_FSUUID, buffer, sizeof(buffer))==0 && strlen(buffer)==36)
        strlcatf(options, sizeof(options), " -U %s ", buffer);
    
    if (dico_get_string(d, 0, FSYSHEADKEY_FSEXTDEFMNTOPT, buffer, sizeof(buffer))==0 && strlen(buffer)>0)
        strlcatf(options, sizeof(options), " -o %s ", buffer);
    
    if (dico_get_u64(d, 0, FSYSHEADKEY_FSEXTFSCKMAXMNTCOUNT, &temp64)==0)
        strlcatf(options, sizeof(options), " -c %ld ", (long)temp64);
    
    if (dico_get_u64(d, 0, FSYSHEADKEY_FSEXTFSCKCHECKINTERVAL, &temp64)==0)
        strlcatf(options, sizeof(options), " -i %ldd ", (long)(temp64/86400L));
    
    if (options[0])
    {
        if (exec_command(command, sizeof(command), &exitst, NULL, 0, NULL, 0, "tune2fs %s %s", partition, options)!=0 || exitst!=0)
        {   errprintf("command [%s] failed with return status=%d\n", command, exitst);
            ret=-1;
            goto extfs_mkfs_cleanup;
        }
        
        // run e2fsck to workaround an tune2fs bug in e2fsprogs < 1.41.4 on ext4
        // http://article.gmane.org/gmane.comp.file-systems.ext4/11181
        if (extfstype==EXTFSTYPE_EXT4 && e2fstoolsver<PROGVER(1,41,4))
        {
            if ( ((res=exec_command(command, sizeof(command), &exitst, NULL, 0, NULL, 0, "e2fsck -fy %s", partition))!=0) || ((exitst!=0) && (exitst!=1)) )
            {   errprintf("command [%s] failed with return status=%d\n", command, exitst);
                ret=-1;
                goto extfs_mkfs_cleanup;
            }
        }
    }
    
extfs_mkfs_cleanup:
    strlist_destroy(&strfeatures);
    return ret;
}

int extfs_getinfo(cdico *d, char *devname)
{
    struct fsa_ext2_sb *super;
    blk_t use_superblock=0;
    int use_blocksize=0;
    char uuid[512];
    ext2_filsys fs;
    int origextfstype;
    char mntopt[1024];
    char label[80];
    u32 mask, m;
    int count;
    int i;
    
    // ---- open partition
    if (ext2fs_open(devname, EXT2_FLAG_JOURNAL_DEV_OK | EXT2_FLAG_SOFTSUPP_FEATURES, use_superblock,  use_blocksize, unix_io_manager, &fs)!=0)
    {   errprintf("ext2fs_open(%s) failed\n", devname);
        return -1;
    }
    super=(struct fsa_ext2_sb *)fs->super;
    
    // --- label
    memset(label, 0, sizeof(label));
    if (super->s_volume_name[0])
    {   memset(label, 0, sizeof(label));
        strncpy(label, super->s_volume_name, sizeof(super->s_volume_name));
    }
    dico_add_string(d, 0, FSYSHEADKEY_FSLABEL, label);
    
    // ---- uuid
    /*if ((str=e2p_uuid2str(super->s_uuid))!=NULL)
        dico_add_string(d, 0, FSYSHEADKEY_FSUUID, str);*/
    memset(uuid, 0, sizeof(uuid));
    uuid_unparse_lower((u8*)super->s_uuid, uuid);
    dico_add_string(d, 0, FSYSHEADKEY_FSUUID, uuid);
    msgprintf(MSG_DEBUG1, "extfs_uuid=[%s]\n", uuid); 
    
    // ---- block size
    dico_add_u64(d, 0, FSYSHEADKEY_FSEXTBLOCKSIZE, EXT2_BLOCK_SIZE(super));
    
    // ---- filesystem revision (good-old-rev or dynamic)
    dico_add_u64(d, 0, FSYSHEADKEY_FSEXTREVISION, super->s_rev_level);
    
    // ---- inode size
    if (super->s_rev_level >= EXT2_DYNAMIC_REV)
        dico_add_u64(d, 0, FSYSHEADKEY_FSINODESIZE, super->s_inode_size);
    else
        dico_add_u64(d, 0, FSYSHEADKEY_FSINODESIZE, EXT2_GOOD_OLD_INODE_SIZE); // Good old rev
    
    // ---- extended options
    if (super->s_raid_stride > 0)
    {   dico_add_u64(d, 0, FSYSHEADKEY_FSEXTEOPTRAIDSTRIDE, super->s_raid_stride);
        msgprintf(MSG_DEBUG1, "extfs_raid_stride: %u\n", super->s_raid_stride);
    }
    if (super->s_raid_stripe_width > 0)
    {   dico_add_u64(d, 0, FSYSHEADKEY_FSEXTEOPTRAIDSTRIPEWIDTH, super->s_raid_stripe_width);
        msgprintf(MSG_DEBUG1, "extfs_raid_stripe_width: %u\n", super->s_raid_stripe_width);
    }
    
    // ---- fsck details: max_mount_count and check_interval
    dico_add_u64(d, 0, FSYSHEADKEY_FSEXTFSCKMAXMNTCOUNT, max(super->s_max_mnt_count,0));
    dico_add_u64(d, 0, FSYSHEADKEY_FSEXTFSCKCHECKINTERVAL, super->s_checkinterval);
    msgprintf(MSG_DEBUG1, "extfs_max_mount_count: %ld\n", (long)max(super->s_max_mnt_count,0));
    msgprintf(MSG_DEBUG1, "extfs_check_interval: %ld\n", (long)super->s_checkinterval);
    
    // ---- default mount options
    memset(mntopt, 0, sizeof(mntopt));
    count=0;
    mask=super->s_default_mount_opts;
    if (mask & EXT3_DEFM_JMODE)
    {   strlcatf(mntopt, sizeof(mntopt), "%s", e2p_mntopt2string(mask & EXT3_DEFM_JMODE));
        count++;
    }
    for (i=0, m=1; i < 32; i++, m<<=1)
    {
        if (m & EXT3_DEFM_JMODE)
            continue;
        if (mask & m)
        {
            if (count++) strlcatf(mntopt, sizeof(mntopt), ",");
            strlcatf(mntopt, sizeof(mntopt), "%s", e2p_mntopt2string(m));
        }
    }
    dico_add_string(d, 0, FSYSHEADKEY_FSEXTDEFMNTOPT, mntopt);
    msgprintf(MSG_DEBUG1, "default mount options: [%s]\n", mntopt);
    
    // ---- filesystem features
    dico_add_u64(d, 0, FSYSHEADKEY_FSEXTFEATURECOMPAT, (u64)super->s_feature_compat);
    dico_add_u64(d, 0, FSYSHEADKEY_FSEXTFEATUREINCOMPAT, (u64)super->s_feature_incompat);
    dico_add_u64(d, 0, FSYSHEADKEY_FSEXTFEATUREROCOMPAT, (u64)super->s_feature_ro_compat);
    
    origextfstype=extfs_get_fstype_from_compat_flags((u64)super->s_feature_compat, (u64)super->s_feature_incompat, (u64)super->s_feature_ro_compat);
    msgprintf(MSG_DEBUG1, "the filesystem type determined by the features is [%s]\n", format_fstype(origextfstype));
    
    // ---- check that fsarchiver is aware of all the filesystem features used on that filesystem
    if (extfs_check_compatibility((u64)super->s_feature_compat, (u64)super->s_feature_incompat, (u64)super->s_feature_ro_compat)!=0)
    {   errprintf("this filesystem has ext{2,3,4} features which are not supported by this fsarchiver version.\n");
        return -1;
    }
    
    // ---- minimum fsarchiver version required to restore
    dico_add_u64(d, 0, FSYSHEADKEY_MINFSAVERSION, FSA_VERSION_BUILD(0, 6, 4, 0));
            
    ext2fs_close(fs);
    
    return 0;
}

int extfs_mount(char *partition, char *mntbuf, char *fsbuf, int flags, char *mntinfo)
{
    blk_t use_superblock=0;
    int use_blocksize=0;
    ext2_filsys fs;
    int origextfstype;
    char fsname[32];
    
    msgprintf(MSG_DEBUG1, "extfs_mount(partition=[%s], mnt=[%s], fsbuf=[%s])\n", partition, mntbuf, fsbuf);
    
    if (ext2fs_open(partition, EXT2_FLAG_JOURNAL_DEV_OK | EXT2_FLAG_SOFTSUPP_FEATURES, use_superblock,  use_blocksize, unix_io_manager, &fs)!=0)
    {   msgprintf(MSG_DEBUG1, "ext2fs_open(%s) failed\n", partition);
        return -1;
    }
    
    origextfstype=extfs_get_fstype_from_compat_flags((u64)fs->super->s_feature_compat, 
            (u64)fs->super->s_feature_incompat, (u64)fs->super->s_feature_ro_compat);
    snprintf(fsname, sizeof(fsname), "%s", format_fstype(origextfstype));
    msgprintf(MSG_VERB2, "the filesystem of [%s] type determined by the features is [%s]\n", partition, fsname);
    
    ext2fs_close(fs);
    
    if (strcmp(fsname, fsbuf)!=0)
    {   msgprintf(MSG_DEBUG1, "extfs_mount: the filesystem requested [%s] does not match the filesystem detected [%s]\n", fsbuf, fsname);
        return -1;
    }
    
    return generic_mount(partition, mntbuf, fsbuf, "user_xattr,acl", flags);
}

int extfs_umount(char *partition, char *mntbuf)
{
    return generic_umount(mntbuf);
}

int extfs_test(char *partition, int extfstype) // returns true if it's that sort of filesystem
{
    blk_t use_superblock=0;
    int use_blocksize=0;
    int extfstypedetected;
    ext2_filsys fs;
    
    if (ext2fs_open(partition, EXT2_FLAG_JOURNAL_DEV_OK | EXT2_FLAG_SOFTSUPP_FEATURES, use_superblock,  use_blocksize, unix_io_manager, &fs)!=0)
        return false;
    
    extfstypedetected=extfs_get_fstype_from_compat_flags((u64)fs->super->s_feature_compat, 
            (u64)fs->super->s_feature_incompat, (u64)fs->super->s_feature_ro_compat);
    msgprintf(MSG_DEBUG1, "the filesystem type determined by the extfs features is [%s]\n", format_fstype(extfstypedetected));
    
    ext2fs_close(fs);
    
    // if detected is what is tested, say yes
    return (extfstypedetected==extfstype);
}

int ext2_test(char *partition)
{
    return extfs_test(partition, EXTFSTYPE_EXT2);
}

int ext3_test(char *partition)
{
    return extfs_test(partition, EXTFSTYPE_EXT3);
}

int ext4_test(char *partition)
{
    return extfs_test(partition, EXTFSTYPE_EXT4);
}

int extfs_get_reqmntopt(char *partition, cstrlist *reqopt, cstrlist *badopt)
{
    if (!reqopt || !badopt)
        return -1;
    
    strlist_add(badopt, "nouser_xattr");
    strlist_add(badopt, "noacl");
    
    return 0;
}

u64 check_prog_version(char *prog)
{
    char stderrbuf[2048];
    char command[2048];
    char options[1024];
    char temp1[1024];
    char delims[]="\n\r";
    char *saveptr;
    char *result;
    int foundversion;
    int x, y, z;
    
    // init
    memset(options, 0, sizeof(options));
    memset(stderrbuf, 0, sizeof(stderrbuf));
    
    if (exec_command(command, sizeof(command), NULL, NULL, 0, stderrbuf, sizeof(stderrbuf), "%s -V", prog)!=0)
    {   errprintf("program %s was not found or has bad permissions.\n", prog);
        return -1;
    }
    
    foundversion=false;
    result=strtok_r(stderrbuf, delims, &saveptr);
    while (result != NULL && foundversion==false)
    {   if ((memcmp(result, prog, strlen(prog))==0))
            foundversion=true;
        else
            result = strtok_r(NULL, delims, &saveptr);
    }
    
    if (foundversion==false)
    {   errprintf("can't parse %s version number: no match\n", prog);
        return 0;
    }
    
    x=y=z=0;
    sscanf(result, "%1023s %d.%d.%d", temp1, &x, &y, &z);
    
    if (x==0 && y==0)
    {   errprintf("can't parse %s version number: x=y=0\n", prog);
        return 0;
    }
    
    return PROGVER(x,y,z);
}
