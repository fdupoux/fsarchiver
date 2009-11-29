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

#ifdef OPTION_LZO_SUPPORT

#include <stdio.h>
#include <unistd.h>

#include "common.h"
#include "comp_lzo.h"

int compress_block_lzo(u64 origsize, u64 *compsize, u8 *origbuf, u8 *compbuf, u64 compbufsize, int level)
{
	lzo_uint destsize=(lzo_uint)compbufsize;
	char workmem[LZO1X_1_MEM_COMPRESS];
	
	if (lzo1x_1_compress((lzo_bytep)origbuf, (lzo_uint)origsize, (lzo_bytep)compbuf, (lzo_uintp)&destsize, (lzo_voidp)workmem)!=LZO_E_OK)
	{	errprintf("lzo1x_1_compress() failed\n");
		return -1;
	}
	
	*compsize=(u64)destsize;
	return 0;
}

int uncompress_block_lzo(u64 compsize, u64 *origsize, u8 *origbuf, u64 origbufsize, u8 *compbuf)
{
	lzo_uint new_len=origbufsize;
	
	if (lzo1x_decompress_safe(compbuf, compsize, origbuf, &new_len, NULL)!=LZO_E_OK)
	{	errprintf("lzo1x_decompress_safe() failed\n");
		return -1;
	}
	
	*origsize=(u64)new_len;
	return 0;
}

#endif // OPTION_LZO_SUPPORT
