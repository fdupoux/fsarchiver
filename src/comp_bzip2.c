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

#include <bzlib.h>

#include "fsarchiver.h"
#include "common.h"
#include "comp_bzip2.h"
#include "error.h"

int compress_block_bzip2(u64 origsize, u64 *compsize, u8 *origbuf, u8 *compbuf, u64 compbufsize, int level)
{
    unsigned int destsize=compbufsize;
    
    if (BZ2_bzBuffToBuffCompress((char*)compbuf, &destsize, (char*)origbuf, origsize, 9, 0, 30)!=BZ_OK)
        return -1;
    
    *compsize=(u64)destsize;
    return 0;
}

int uncompress_block_bzip2(u64 compsize, u64 *origsize, u8 *origbuf, u64 origbufsize, u8 *compbuf)
{
    unsigned int destsize=origbufsize;
    int res;
    
    res=BZ2_bzBuffToBuffDecompress((char*)origbuf, &destsize, (char*)compbuf, compsize, 0, 0);
    if (res!=BZ_OK)
    {   errprintf("uncompress failed, res=%d\n", res);
        return -1;
    }

    *origsize=(u64)destsize;
    return 0;
}
