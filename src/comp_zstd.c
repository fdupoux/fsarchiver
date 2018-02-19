
/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2018 Francois Dupoux.  All rights reserved.
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
    int res=0;

    if (ZSTD_isError((res=ZSTD_compress((char*)compbuf, compbufsize, (const char*)origbuf, (int)origsize, level))))
    {   errprintf("ZSTD_compress(): failed: res=%d\n", res);
        return FSAERR_UNKNOWN;
    }
    else
    {   *compsize=(u64)res;
        return FSAERR_SUCCESS;
    }
}

int uncompress_block_zstd(u64 compsize, u64 *origsize, u8 *origbuf, u64 origbufsize, u8 *compbuf)
{
    int res=0;

    if (ZSTD_isError((res=ZSTD_decompress((char*)origbuf, origbufsize, (char*)compbuf, compsize))))
    {   errprintf("ZSTD_decompress(): failed: res=%d\n", res);
        return FSAERR_UNKNOWN;
    }
    else
    {   *origsize=(u64)res;
        return FSAERR_SUCCESS;
    }
}
#endif // OPTION_ZSTD_SUPPORT
