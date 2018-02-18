
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

#include "fsarchiver.h"
#include "common.h"
#include "comp_zstd.h"
#include "error.h"


#ifdef OPTION_ZSTD_SUPPORT
int compress_block_zstd(u64 origsize, u64 *compsize, u8 *origbuf, u8 *compbuf, u64 compbufsize, int level)
{
    int destsize=compbufsize;
    int res;

    res=ZSTD_compress((char*)compbuf, destsize, (const char*)origbuf, (int)origsize, level);
    if (res < 0) {
        errprintf("ZSTD_compress(): failed.\n");
        return FSAERR_UNKNOWN;
    } else {
        *compsize=(u64)res;
        return FSAERR_SUCCESS;
    }

    return FSAERR_UNKNOWN;
}

int uncompress_block_zstd(u64 compsize, u64 *origsize, u8 *origbuf, u64 origbufsize, u8 *compbuf)
{
    int destsize=origbufsize;
    int res;

    if((res=ZSTD_decompress((char*)origbuf, destsize, (char*)compbuf, compsize)) > 0){
        *origsize=(u64)res;
        return FSAERR_SUCCESS;
    }

    errprintf("ZSTD_decompress() failed, res=%d\n", res);

    return FSAERR_UNKNOWN;
}
#endif // OPTION_ZSTD_SUPPORT
