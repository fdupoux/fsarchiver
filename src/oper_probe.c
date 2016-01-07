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

#include "fsarchiver.h"
#include "devinfo.h"
#include "oper_probe.h"
#include "common.h"
#include "error.h"

struct s_diskinfo
{
    bool detailed;
    char format[256];
    char title[256];
};

int partlist_getlist(struct s_devinfo *blkdev, int maxblkdev, int *diskcount, int *partcount)
{
    struct s_devinfo blkdev1[FSA_MAX_BLKDEVICES];
    struct s_devinfo tmpdev;
    int best; // index of the best item found in old array
    int pos=0; // pos of latest item in new array
    char devname[1024];
    char longname[1024];
    char delims[]=" \t\n";
    char line[1024];
    char *saveptr;
    char *result;
    char major[256];
    char minor[256];
    FILE *fpart;
    int count=0;
    int i, j;
    
    // init
    *diskcount=0;
    *partcount=0;
    
    // browse list in "/proc/partitions"
    if ((fpart=fopen("/proc/partitions","rb"))==NULL)
        return -1;
    while(!feof(fpart) && (count < FSA_MAX_BLKDEVICES) && (count < maxblkdev))
    {
        if (stream_readline(fpart, line, sizeof(line))>1)
        {
            minor[0]=major[0]=0;
            devname[0]=0;
            result=strtok_r(line, delims, &saveptr);
            
            for(i=0; result != NULL && i<=4; i++)
            {
                switch(i)
                {
                    case 0: // col0 = major
                        snprintf(major, sizeof(major), "%s", result);
                        break;
                    case 1: // col1 = minor
                        snprintf(minor, sizeof(minor), "%s", result);
                        break;
                    case 3: // col3 = devname
                        snprintf(devname, sizeof(devname), "%s", result);
                        break;
                }
                result = strtok_r(NULL, delims, &saveptr);
            }
            
            // ignore invalid entries
            if ((strlen(devname)==0) || (atoi(major)==0 && atoi(minor)==0))
                continue;
            snprintf(longname, sizeof(longname), "/dev/%s", devname);
            if (get_devinfo(&tmpdev, longname, atoi(minor), atoi(major))!=0)
               continue; // to to the next part
            
            // check that this device is not already in the list
            for (i=0; i < count; i++)
                if (blkdev1[i].rdev==tmpdev.rdev)
                    continue; // to to the next part
            
            // add the device to list if it is a real device and it's not already in the list
            blkdev1[count++]=tmpdev;
        }
    }
    
    fclose(fpart);
    
    // ---- 2. sort the devices
    for (pos=0, i=0; i<count; i++)
    {
        // set best to the first available item in the old array
        for (j=0, best=-1; (j<count) && (best==-1); j++)
            if (blkdev1[j].rdev!=0)
                best=j;
        // find the index of the best item in the old array
        for (j=0; j<count; j++)
            if ((blkdev1[j].rdev > 0) && (blkdev1[j].rdev < blkdev1[best].rdev))
                best=j;
        // update counters
        switch (blkdev1[best].devtype)
        {
            case BLKDEV_FILESYSDEV:
                (*partcount)++;
                break;
            case BLKDEV_PHYSDISK:
                (*diskcount)++;
                break;                
        }
        // move item to the new array
        blkdev[pos++]=blkdev1[best];
        blkdev1[best].rdev=0;
    }

    return count;
}

struct s_diskinfo partinfo[]=
{
    {false,    "[%-16s] ",    "[=====DEVICE=====] "},
    {false,    "[%-11.11s] ", "[==FILESYS==] "},
    {false,    "[%-17.17s] ", "[======LABEL======] "},
    {false,    "[%12s] ",     "[====SIZE====] "},
    {false,    "[%3s] ",      "[MAJ] "},
    {false,    "[%3s] ",      "[MIN] "},
    {true,     "[%-36s] ",    "[==============LONGNAME==============] "},
    {true,     "[%-38s] ",    "[=================UUID=================] "},
    {false,    "",            ""}
};

char *partlist_getinfo(char *bufdat, int bufsize, struct s_devinfo *blkdev, int item)
{
    memset(bufdat, 0, bufsize);
    switch (item)
    {
        case 0:    snprintf(bufdat, bufsize, "%s", blkdev->devname); break;
        case 1:    snprintf(bufdat, bufsize, "%s", blkdev->fsname); break;
        case 2:    snprintf(bufdat, bufsize, "%s", blkdev->label); break;
        case 3:    snprintf(bufdat, bufsize, "%s", blkdev->txtsize); break;
        case 4:    snprintf(bufdat, bufsize, "%d", blkdev->major); break;
        case 5:    snprintf(bufdat, bufsize, "%d", blkdev->minor); break;
        case 6:    snprintf(bufdat, bufsize, "%s", blkdev->longname); break;
        case 7:    snprintf(bufdat, bufsize, "%s", blkdev->uuid); break;
    };
    return bufdat;
}

int oper_probe(bool details)
{
    struct s_devinfo blkdev[FSA_MAX_BLKDEVICES];
    int diskcount;
    int partcount;
    char temp[1024];
    int res;
    int i, j;
    
    // ---- 0. get info from /proc/partitions + libblkid
    if ((res=partlist_getlist(blkdev, FSA_MAX_BLKDEVICES, &diskcount, &partcount))<1)
    {   msgprintf(MSG_FORCE, "Failed to detect disks and filesystems\n");
        return -1;
    }
    
    // ---- 1. show physical disks
    if (diskcount>0)
    {
        msgprintf(MSG_FORCE, "[======DISK======] [=============NAME==============] [====SIZE====] [MAJ] [MIN]\n");
        for (i=0; i < res; i++)
            if (blkdev[i].devtype==BLKDEV_PHYSDISK)
                msgprintf(MSG_FORCE, "[%-16s] [%-31s] [%12s] [%3d] [%3d]\n", blkdev[i].devname, 
                    blkdev[i].name, blkdev[i].txtsize, blkdev[i].major, blkdev[i].minor);
        msgprintf(MSG_FORCE, "\n");
    }
    else
    {
        msgprintf(MSG_FORCE, "No physical disk found\n");
    }
    
    // ---- 2. show filesystem information
    if (partcount>0)
    {
        // show title for filesystems
        for (j=0; partinfo[j].title[0]; j++)
        {
            if (details==true || partinfo[j].detailed==false)
                msgprintf(MSG_FORCE, "%s", partinfo[j].title);
        }
        msgprintf(MSG_FORCE, "\n");
        
        // show filesystems data
        for (i=0; i < res; i++)
        {
            if (blkdev[i].devtype==BLKDEV_FILESYSDEV)
            {
                for (j=0; partinfo[j].title[0]; j++)
                {
                    if (details==true || partinfo[j].detailed==false)
                        msgprintf(MSG_FORCE, partinfo[j].format, partlist_getinfo(temp, sizeof(temp), &blkdev[i], j));
                }
                msgprintf(MSG_FORCE, "\n");
            }
        }
    }
    else
    {
        msgprintf(MSG_FORCE, "No filesystem found\n");
    }
    
    return 0;
}
