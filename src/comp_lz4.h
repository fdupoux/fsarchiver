
/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2017 Cristian Vazquez.  All rights reserved.
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

#ifndef __COMPRESS_LZ4_H__
#define __COMPRESS_LZ4_H__

#ifdef OPTION_LZ4_SUPPORT

#include <lz4.h>

int compress_block_lz4(u64 origsize, u64 *compsize, u8 *origbuf, u8 *compbuf, u64 compbufsize, int level);
int uncompress_block_lz4(u64 compsize, u64 *origsize, u8 *origbuf, u64 origbufsize, u8 *compbuf);

#endif // OPTION_LZ4_SUPPORT

#endif // __COMPRESS_LZ4_H__
