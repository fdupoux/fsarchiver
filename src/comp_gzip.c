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

#include <zlib.h>

#include "fsarchiver.h"
#include "common.h"
#include "comp_gzip.h"
#include "error.h"

int compress_block_gzip(u64 origsize, u64 *compsize, u8 *origbuf, u8 *compbuf, u64 compbufsize, int level)
{
    int res;
    uLong gzsize;
    Bytef *gzbuffer;
    
    gzsize=(uLong)compbufsize;
    gzbuffer=(Bytef *)compbuf;
    
    res=compress2(gzbuffer, &gzsize, (const Bytef*)origbuf, (uLong)origsize, level);
    if (res!=Z_OK)
        return -1;
    
    *compsize=(u64)gzsize;
    return 0;
}

int uncompress_block_gzip(u64 compsize, u64 *origsize, u8 *origbuf, u64 origbufsize, u8 *compbuf)
{
    uLong gzsize=(uLong)origbufsize;
    Bytef *gzbuffer=(Bytef *)origbuf;
    int res;
    
    res=uncompress(gzbuffer, &gzsize, (const Bytef*)compbuf, (uLong)compsize);
    if (res!=Z_OK)
    {   errprintf("uncompress failed, res=%d\n", res);
        return -1;
    }
    *origsize=(u64)gzsize;
    return 0;
}
