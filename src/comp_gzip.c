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

#include <zlib.h>

#include "fsarchiver.h"
#include "common.h"
#include "comp_gzip.h"
#include "error.h"

int compress_block_gzip(u64 origsize, u64 *compsize, u8 *origbuf, u8 *compbuf, u64 compbufsize, int level)
{
    uLong gzsize;
    Bytef *gzbuffer;
    
    gzsize=(uLong)compbufsize;
    gzbuffer=(Bytef *)compbuf;
    
    switch (compress2(gzbuffer, &gzsize, (const Bytef*)origbuf, (uLong)origsize, level))
    {
        case Z_OK:
            *compsize=(u64)gzsize;
            return FSAERR_SUCCESS;
        case Z_MEM_ERROR:
            return FSAERR_ENOMEM;
        default:
            return FSAERR_UNKNOWN;
    }
    
    return FSAERR_UNKNOWN;
}

int uncompress_block_gzip(u64 compsize, u64 *origsize, u8 *origbuf, u64 origbufsize, u8 *compbuf)
{
    uLong gzsize=(uLong)origbufsize;
    Bytef *gzbuffer=(Bytef *)origbuf;
    int res;
    
    switch ((res=uncompress(gzbuffer, &gzsize, (const Bytef*)compbuf, (uLong)compsize)))
    {
        case Z_OK:
            *origsize=(u64)gzsize;
            return FSAERR_SUCCESS;
        case Z_MEM_ERROR:
            return FSAERR_ENOMEM;
        default:
            errprintf("uncompress() failed, res=%d\n", res);
            return FSAERR_UNKNOWN;
    }
}
