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

#ifndef __THREAD_COMP_H__
#define __THREAD_COMP_H__

enum {COMPTHR_COMPRESS=1, COMPTHR_DECOMPRESS=2};

void *thread_comp_fct(void *args);
void *thread_decomp_fct(void *args);

#endif // __THREAD_COMP_H__
