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

#ifndef __DICHL_H__
#define __DICHL_H__

#include "types.h"

struct s_dichl;
typedef struct s_dichl cdichl;

struct s_dichlitem;
typedef struct s_dichlitem cdichlitem;

struct s_dichl
{
    cdichlitem *head;
};

struct s_dichlitem
{   u64         key1;
    u64         key2;
    char        *str;
    cdichlitem  *next;
};

cdichl *dichl_alloc();
int    dichl_destroy(cdichl *d);
int    dichl_add(cdichl *d, u64 key1, u64 key2, char *str);
int    dichl_get(cdichl *d, u64 key1, u64 key2, char *buf, int bufsize);

#endif // __DICHL_H__
