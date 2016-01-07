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

#ifndef __COMPRESS_LZMA_H__
#define __COMPRESS_LZMA_H__

#ifdef OPTION_LZMA_SUPPORT

int compress_block_lzma(u64 origsize, u64 *compsize, u8 *origbuf, u8 *compbuf, u64 compbufsize, int level);
int uncompress_block_lzma(u64 compsize, u64 *origsize, u8 *origbuf, u64 origbufsize, u8 *compbuf);

#endif // OPTION_LZMA_SUPPORT

#endif // __COMPRESS_LZMA_H__
