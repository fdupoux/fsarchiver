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
#include "showpart.h"
#include "common.h"
#include "fs_ext2.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <dirent.h>
#include <blkid/blkid.h>
#include <sys/ioctl.h>

int partlist_getlongname(char *longnamebuf, int longbufsize, char *shortname)
{
	char fullpath[PATH_MAX];
	struct stat64 statbuf;
	struct dirent *dir;
	DIR *dirdesc;
	u64 rdev;
	
	// init
	snprintf(longnamebuf, longbufsize, "%s", shortname);
	if (stat64(shortname, &statbuf)!=0)
		return -1;
	rdev=statbuf.st_rdev;
	
	// init
	if (!(dirdesc=opendir("/dev/mapper")))
		return -1;
	
	while ((dir = readdir(dirdesc)) != NULL)
	{
		snprintf(fullpath, sizeof(fullpath), "/dev/mapper/%s", dir->d_name);
		lstat64(fullpath, &statbuf);
		
		if ((stat64(fullpath, &statbuf)==0) && S_ISBLK(statbuf.st_mode) && (statbuf.st_rdev==rdev))
		{	
			snprintf(longnamebuf, longbufsize, "%s", fullpath);
			closedir(dirdesc);
			return 0;
		}	
	}
	
	closedir(dirdesc);
	return -1;
}

int partlist_probe(struct s_blkdev *sdev)
{
	blkid_tag_iterate iter;
	const char *type, *value;
	struct stat64 statbuf;
	blkid_dev dev;
	int fd;
	
	// ---- write defaults values
	snprintf(sdev->label, sizeof(sdev->label), "<unknown>");
	snprintf(sdev->uuid, sizeof(sdev->uuid), "<unknown>");
	snprintf(sdev->fsname, sizeof(sdev->fsname), "<unknown>");
	
	// ---- get blkid infos about the device (label, uuid)
	blkid_cache cache = NULL;
	if (blkid_get_cache(&cache, NULL) < 0)
		return -1;
	if ((dev=blkid_get_dev(cache, sdev->devname, BLKID_DEV_NORMAL))!=NULL)
	{
		iter = blkid_tag_iterate_begin(dev);
		while (blkid_tag_next(iter, &type, &value)==0)
		{
			if (strcmp(type, "LABEL")==0)
				snprintf(sdev->label, sizeof(sdev->label), "%s", value);
			else if (strcmp(type, "UUID")==0)
				snprintf(sdev->uuid, sizeof(sdev->uuid), "%s", value);
			else if (strcmp(type, "TYPE")==0)
				snprintf(sdev->fsname, sizeof(sdev->fsname), "%s", value);
		}
		blkid_tag_iterate_end(iter);
		
		// workaround: blkid < 1.41 don't know ext4 and say it is ext3 instead
		if (strcmp(sdev->fsname, "ext3")==0)
		{
			if (ext3_test(sdev->devname)==true)
				snprintf(sdev->fsname, sizeof(sdev->fsname), "ext3");
			else // cannot run ext4_test(): it would fail on an ext4 when e2fsprogs < 1.41
				snprintf(sdev->fsname, sizeof(sdev->fsname), "ext4");
		}
	}
	blkid_put_cache(cache); // free memory allocated by blkid_get_cache

	// ---- get misc infos about the device (size, minor, major)
	if ((stat64(sdev->devname, &statbuf)!=0) || !S_ISBLK(statbuf.st_mode))
		return -1;
	
	sdev->rdev=statbuf.st_rdev;
	snprintf(sdev->major, sizeof(sdev->major), "%d", major(statbuf.st_rdev));
	snprintf(sdev->minor, sizeof(sdev->minor), "%d", minor(statbuf.st_rdev));
	
	if (partlist_getlongname(sdev->longname, sizeof(sdev->longname), sdev->devname)!=0)
		snprintf(sdev->longname, sizeof(sdev->longname), "%s", sdev->devname);
	
	if ((fd=open64(sdev->devname, O_RDONLY|O_LARGEFILE))<0)
		return -1;
	
	sdev->devsize=lseek64(fd, 0, SEEK_END);
	format_size(sdev->devsize, sdev->txtsize, sizeof(sdev->txtsize), 'h');
	close(fd);

	if (sdev->devsize==1024) // ignore extended partitions
		return -1;	
	
	return 0;
}

int is_partition(const char *devname)
{
	int chr;
	
	// when HDIO_GETGEO works use it
/*
	#define HDIO_GETGEO 0x0301 // get device geometry
	
	struct hd_geometry {
	unsigned char heads;
	unsigned char sectors;
	unsigned short cylinders;
	unsigned long start;
	};
	
#ifdef HDIO_GETGEO
        struct hd_geometry geometry;
        int fd=-1;
	int res;
	
        if ((fd=open(devname, O_RDONLY))>=0)
	{
		res=ioctl(fd, HDIO_GETGEO, &geometry);
		//printf("ioctl(%s)=%d and (geometry.start!=0)=%d: geometry.start=%d, geometry.sectors=%d\n", devname, res, geometry.start!=0, geometry.start, geometry.sectors);
		close(fd);
		if (res==0)
			return geometry.start!=0;
	}
#endif
	return -1;*/
	
	// case-1: HP CCISS devices ('/dev/cciss/c0d0' and '/dev/cciss/c1d1' are disks, '/dev/cciss/c1d1p1' is partition)
	if (strstr(devname, "/dev/cciss"))
		return (strstr(devname, "p")!=NULL);
	// case-2: device mapper devices
	if (strstr(devname, "/dev/dm-") || strstr(devname, "/dev/mapper"))
		return true;
	// case-3: standard devices
	for (chr=0; devname[chr] && devname[chr+1]; chr++);
	while ((chr>0) && (isdigit(devname[chr-1]))) chr--;
	if (atol(devname+chr)==0) // partitions names terminates with a number > 0
		return false; // not a partition device name
	else
        	return true; // this is a partition device name
}

int partlist_getlist(struct s_blkdev *blkdev, int maxblkdev)
{
	int best; // index of the best item found in old array
	int pos=0; // pos of latest item in new array
	struct s_blkdev blkdev1[FSA_MAX_BLKDEVICES];
	struct s_blkdev tmpdev;
	char delims[]=" \t\n";
	char line[1024];
	char *saveptr;
	char *result;
	int count=0;
	int i, j;
	FILE *fpart;
	
	// ---- 1. add partitions listed in /proc/partitions
	if ((fpart=fopen("/proc/partitions","rb"))!=NULL)
	{
		while(!feof(fpart) && (count < FSA_MAX_BLKDEVICES) && (count < maxblkdev))
		{
			if (stream_readline(fpart, line, sizeof(line))>1)
			{
				memset(&tmpdev, 0, sizeof(tmpdev));
				result=strtok_r(line, delims, &saveptr);
				
				for(i=0; result != NULL && i<=4; i++)
				{
					switch(i)
					{
						case 0: // col0 = major
							snprintf(tmpdev.major, sizeof(tmpdev.major), "%s", result);
							break;
						case 1: // col1 = minor
							snprintf(tmpdev.minor, sizeof(tmpdev.minor), "%s", result);
							break;
						case 3: // col3 = devname
							snprintf(tmpdev.devname, sizeof(tmpdev.devname), "/dev/%s", result);
							break;
					}
					result = strtok_r(NULL, delims, &saveptr);
				}
				
				// remove empty names
				if (strlen(tmpdev.devname)==0)
					continue; // to to the next part
				
				// remove invalid entries from /proc/partitions
				if (atoi(tmpdev.major)==0 && atoi(tmpdev.minor)==0)
					continue; // to to the next part
				
				// exclude whole disks (eg: "/dev/sda", "/dev/sdb", "/dev/cciss/c0d0", ...)
				if (is_partition(tmpdev.devname)!=true)
					continue; // to to the next part
				
				if (partlist_probe(&tmpdev)==-1)
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
	}
	
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
		// move item to the new array
		blkdev[pos++]=blkdev1[best];
		blkdev1[best].rdev=0;
	}
	
	return count;
}

struct s_diskinfo diskinfo[]=
{
	{false,	"[%-16s] ",	"[=====DEVICE=====] "},
	{false,	"[%-11s] ",	"[==FILESYS==] "},
	{false,	"[%-15s] ",	"[=====LABEL=====] "},
	{false,	"[%12s] ",	"[====SIZE====] "},
	{false,	"[%3s] ",	"[MAJ] "},
	{false,	"[%3s] ",	"[MIN] "},
	{true,	"[%-36s] ",	"[==============LONGNAME==============] "},
	{true,	"[%-40s] ",	"[==================UUID==================] "},
	{false,	"",		""}
};

char *partlist_getinfo(char *bufdat, int bufsize, struct s_blkdev *blkdev, int item)
{
	memset(bufdat, 0, bufsize);
	switch (item)
	{
		case 0:	snprintf(bufdat, bufsize, "%s", blkdev->devname); break;
		case 1:	snprintf(bufdat, bufsize, "%s", blkdev->fsname); break;
		case 2:	snprintf(bufdat, bufsize, "%s", blkdev->label); break;
		case 3:	snprintf(bufdat, bufsize, "%s", blkdev->txtsize); break;
		case 4:	snprintf(bufdat, bufsize, "%s", blkdev->major); break;
		case 5:	snprintf(bufdat, bufsize, "%s", blkdev->minor); break;
		case 6:	snprintf(bufdat, bufsize, "%s", blkdev->longname); break;
		case 7:	snprintf(bufdat, bufsize, "%s", blkdev->uuid); break;
	};
	return bufdat;
}

int partlist_showlist(bool details)
{
	struct s_blkdev blkdev[FSA_MAX_BLKDEVICES];
	char temp[1024];
	int res;
	int i, j;
	
	if ((res=partlist_getlist(blkdev, FSA_MAX_BLKDEVICES))<1)
	{	msgprintf(MSG_FORCE, "No filesystem detected\n");
		return -1;
	}
	
	for (j=0; diskinfo[j].title[0]; j++)
	{
		if (details==true || diskinfo[j].detailed==false)
			msgprintf(MSG_FORCE, "%s", diskinfo[j].title);
	}
	msgprintf(MSG_FORCE, "\n");
	
	for (i=0; i < res; i++)
	{
		for (j=0; diskinfo[j].title[0]; j++)
		{
			if (details==true || diskinfo[j].detailed==false)
				msgprintf(MSG_FORCE, diskinfo[j].format, partlist_getinfo(temp, sizeof(temp), &blkdev[i], j));
		}
		msgprintf(MSG_FORCE, "\n");
	}
	
	return 0;
}
