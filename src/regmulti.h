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

#ifndef __REGMULTI_H__
#define __REGMULTI_H__

#include "queue.h"
#include "dico.h"

typedef struct s_regmulti
{
	// common
	u32	count; // how many small files are in this struct
	u32	maxitems; // how many small files that struct can contains
	u32	maxblksize; // maximum size of a data block
	
	// linked list of headers
	cdico	*objhead[FSA_MAX_SMALLFILECOUNT]; // worst case: each file is just one byte: this is how many files we can store in the block
	
	// common block to be compressed
	char	data[FSA_MAX_BLKSIZE];
	u32	usedsize; // how many bytes are used in data
} cregmulti;

int	regmulti_empty(struct s_regmulti *m);
int	regmulti_init(struct s_regmulti *m, u32 maxblksize);
int	regmulti_count(struct s_regmulti *m, cdico *header, char *data, u32 datsize);
bool	regmulti_save_enough_space_for_new_file(struct s_regmulti *m, u32 filesize);
int	regmulti_save_addfile(struct s_regmulti *m, cdico *header, char *data, u32 datsize);
int	regmulti_save_enqueue(struct s_regmulti *m, cqueue *q, int fsid);
int	regmulti_rest_addheader(struct s_regmulti *m, cdico *header);
int	regmulti_rest_setdatablock(struct s_regmulti *m, char *data, u32 datsize);
int	regmulti_rest_getfile(struct s_regmulti *m, int index, cdico **filehead, char *data, u64 *datsize, u32 bufsize);

#endif // __REGMULTI_H__
