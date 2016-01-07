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

#include <string.h>

#include "fsarchiver.h"
#include "options.h"
#include "error.h"

coptions g_options;

int options_init()
{
    memset(&g_options, 0, sizeof(coptions));
    if (strlist_init(&g_options.exclude)!=0)
        return -1;
    return 0;
}

int options_destroy()
{
    if (strlist_destroy(&g_options.exclude)!=0)
        return -1;
    memset(&g_options, 0, sizeof(coptions));
    return 0;
}

int options_select_compress_level(int opt)
{
    switch (opt)
    {
#ifdef OPTION_LZO_SUPPORT
        case 1: // lzo
            g_options.compressalgo=COMPRESS_LZO;
            g_options.compresslevel=3;
            break;
#else
        case 1: // lzo
            errprintf("compression level %d is not available: lzo has been disabled at compilation time\n", opt);
            return -1;
#endif // OPTION_LZO_SUPPORT
        case 2: // gzip fast
            g_options.compressalgo=COMPRESS_GZIP;
            g_options.compresslevel=3;
            break;
        case 3: // gzip standard
            g_options.compressalgo=COMPRESS_GZIP;
            g_options.compresslevel=6;
            break;
        case 4: // gzip best
            g_options.compressalgo=COMPRESS_GZIP;
            g_options.compresslevel=9;
            break;
        case 5: // bzip2 fast
            g_options.compressalgo=COMPRESS_BZIP2;
            g_options.datablocksize=262144;
            g_options.compresslevel=2;
            break;
        case 6: // bzip2 good
            g_options.compressalgo=COMPRESS_BZIP2;
            g_options.datablocksize=524288;
            g_options.compresslevel=5;
            break;
#ifdef OPTION_LZMA_SUPPORT
        case 7: // lzma fast
            g_options.compressalgo=COMPRESS_LZMA;
            g_options.datablocksize=262144;
            g_options.compresslevel=1;
            break;
        case 8: // lzma medium
            g_options.compressalgo=COMPRESS_LZMA;
            g_options.datablocksize=524288;
            g_options.compresslevel=6;
            break;
        case 9: // lzma best
            g_options.compressalgo=COMPRESS_LZMA;
            g_options.datablocksize=FSA_MAX_BLKSIZE;
            g_options.compresslevel=9;
            break;
#else
        case 7: // lzma
        case 8: // lzma
        case 9: // lzma
            errprintf("compression level %d is not available: lzma has been disabled at compilation time\n", opt);
            return -1;
#endif
        default:
            errprintf("invalid compression level: %d\n", opt);
            return -1;
    }
    
    return 0;
}
