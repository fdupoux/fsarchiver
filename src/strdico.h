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

#ifndef __STRDICO_H__
#define __STRDICO_H__

struct s_strdico;
struct s_strdicoitem;

typedef struct s_strdico cstrdico;
typedef struct s_strdicoitem cstrdicoitem;

struct s_strdico
{   cstrdicoitem *head; 
    char *validkeys;
};

struct s_strdicoitem
{   char         *key;
    char         *value;
    cstrdicoitem *next;
};

cstrdico *strdico_alloc();
int strdico_destroy(cstrdico *d);
int strdico_set_valid_keys(cstrdico *d, const char *keys);
int strdico_parse_string(cstrdico *d, const char *strdefs);
int strdico_set_value(cstrdico *d, const char *key, const char *value);
int strdico_get_string(cstrdico *d, char *outbuffer, int outbufsize, const char *key);
int strdico_get_s64(cstrdico *d, s64 *value, const char *key);
int strdico_print(cstrdico *d);

#endif // __STRDICO_H__
