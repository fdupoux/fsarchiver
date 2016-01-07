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

#ifndef __DATAFILE_H__
#define __DATAFILE_H__

#include "types.h"

struct s_datafile;
typedef struct s_datafile cdatafile;

cdatafile *datafile_alloc();
int       datafile_destroy(cdatafile *f);
int       datafile_open_write(cdatafile *f, char *path, bool simul, bool sparse);
int       datafile_write(cdatafile *f, char *data, u64 len);
int       datafile_close(cdatafile *f, u8 *md5bufdat, int md5bufsize);

#endif // __DATAFILE_H__
