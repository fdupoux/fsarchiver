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

#ifndef __WRITEBUF_H__
#define __WRITEBUF_H__

struct s_dico;
struct s_blockinfo;

struct s_writebuf;
typedef struct s_writebuf cwritebuf;

struct s_writebuf
{   char *data;
    u64  size;
};

cwritebuf *writebuf_alloc();
int writebuf_destroy(cwritebuf *wb);
int writebuf_add_data(cwritebuf *wb, void *data, u64 size);
int writebuf_add_dico(cwritebuf *wb, struct s_dico *d, char *magic);
int writebuf_add_header(cwritebuf *wb, struct s_dico *d, char *magic, u32 archid, u16 fsid);
int writebuf_add_block(cwritebuf *wb, struct s_blockinfo *blkinfo, u32 archid, u16 fsid);

#endif // __WRITEBUF_H__
