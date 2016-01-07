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

#ifndef __STRLIST_H__
#define __STRLIST_H__

struct s_strlist;
typedef struct s_strlist cstrlist;

struct s_strlistitem;
typedef struct s_strlistitem cstrlistitem;

struct s_strlistitem
{   char         *str;
    cstrlistitem *next;
};

struct s_strlist
{
    cstrlistitem *head;
};

int  strlist_destroy(cstrlist *l);
int  strlist_init(cstrlist *l);
int  strlist_empty(cstrlist *l);
int  strlist_add(cstrlist *l, char *str);
int  strlist_remove(cstrlist *l, char *str);
int  strlist_exists(cstrlist *l, char *str);
int  strlist_getitem(cstrlist *l, int index, char *buf, int bufsize);
char *strlist_merge(cstrlist *l, char *bufdat, int bufsize, char sep);
int  strlist_split(cstrlist *l, char *text, char sep);
int  strlist_count(cstrlist *l);
int  strlist_show(cstrlist *l);

#endif // __STRLIST_H__
