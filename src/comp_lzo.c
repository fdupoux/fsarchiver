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
#include "comp_lzo.h"
#include "error.h"

#ifdef OPTION_LZO_SUPPORT

int compress_block_lzo(u64 origsize, u64 *compsize, u8 *origbuf, u8 *compbuf, u64 compbufsize, int level)
{
    lzo_uint destsize=(lzo_uint)compbufsize;
    char workmem[LZO1X_1_MEM_COMPRESS];
    
    switch (lzo1x_1_compress((lzo_bytep)origbuf, (lzo_uint)origsize, (lzo_bytep)compbuf, (lzo_uintp)&destsize, (lzo_voidp)workmem))
    {
        case LZO_E_OK:
            *compsize=(u64)destsize;
            return FSAERR_SUCCESS;
        case LZO_E_OUT_OF_MEMORY:
            return FSAERR_ENOMEM;
        default:
            return FSAERR_UNKNOWN;
    }
    
    return FSAERR_UNKNOWN; 
}

int uncompress_block_lzo(u64 compsize, u64 *origsize, u8 *origbuf, u64 origbufsize, u8 *compbuf)
{
    lzo_uint new_len=origbufsize;
    int res;
    
    switch ((res=lzo1x_decompress_safe(compbuf, compsize, origbuf, &new_len, NULL)))
    {
        case LZO_E_OK:
            *origsize=(u64)new_len;
            return FSAERR_SUCCESS;
        case LZO_E_OUT_OF_MEMORY:
            return FSAERR_ENOMEM;
        default:
            errprintf("lzo1x_decompress_safe() failed, res=%d\n", res);
            return FSAERR_UNKNOWN;
    }
    
    return FSAERR_UNKNOWN;
}

#endif // OPTION_LZO_SUPPORT
