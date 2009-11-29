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

#ifndef __FILESYS_H__
#define __FILESYS_H__

#include <stdio.h>

#define PROGVER(x, y, z)	(((u64)x)<<16)+(((u64)y)<<8)+(((u64)z)<<0)

#include "dico.h"
#include "strlist.h"

typedef struct s_filesys
{
	char *name;
	int (*mount)(char *partition, char *mntbuf, char *fsname, int flags, char *mntinfo);
	int (*umount)(char *partition, char *mntbuf);
	int (*getinfo)(cdico *d, char *devname);
	int (*mkfs)(cdico *d, char *partition);
	int (*test)(char *partition);
	int (*reqmntopt)(char *partition, cstrlist *reqopt, cstrlist *badopt);
	bool winattr;
} cfilesys;

extern cfilesys filesys[];

int devcmp(char *dev1, char *dev2);
int generic_get_spacestats(char *dev, char *mnt, char *text, int textlen);
int generic_get_fsrwstatus(char *options);
int generic_get_fstype(char *fsname, int *fstype);
int generic_get_mntinfo(char *devname, int *readwrite, char *mntbuf, int maxmntbuf, char *optbuf, int maxoptbuf, char *fsbuf, int maxfsbuf);
int generic_mount(char *partition, char *mntbuf, char *fsbuf, char *mntopt, int flags);
char *format_prog_version(u64 version, char *bufdat, int buflen);
int generic_umount(char *mntbuf);
u64 check_prog_version(char *prog);

#endif // __FILESYS_H__
