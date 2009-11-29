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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "writebuf.h"
#include "common.h"

struct s_writebuf *writebuf_alloc()
{
	struct s_writebuf *wb;
	if ((wb=malloc(sizeof(struct s_writebuf)))==NULL)
	{	errprintf("malloc(%d) failed: cannot allocate memory for writebuf\n", (int)sizeof(struct s_writebuf));
		return NULL;
	}
	wb->size=0;
	wb->data=NULL;
	return wb;
}

int writebuf_destroy(struct s_writebuf *wb)
{
	if (wb==NULL)
	{	errprintf("wb is NULL\n");
		return -1;
	}
	
	if (wb->data)
	{
		free(wb->data);
		wb->data=NULL;
	}
	wb->size=0;
	free(wb);
	return 0;
}

int writebuf_add_data(struct s_writebuf *wb, void *data, u64 size)
{
	u64 newsize;
	
	if (wb==NULL)
	{	errprintf("wb is NULL\n");
		return -1;
	}
	
	if (size==0)
	{	errprintf("size=0\n");
		return -1;
	}
	
	newsize=wb->size+size;
	wb->data=realloc(wb->data, newsize+4); // "+4" required else the last byte of the buffer may be alterred (see release-0.3.3)
	if (!wb->data)
	{	errprintf("realloc(oldsize=%ld, newsize=%ld) failed\n", (long)wb->size, (long)newsize+4);
		return -1;
	}
	memcpy(wb->data+wb->size, data, size);
	
	wb->size+=size;
	return 0;
}
