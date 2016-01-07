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

#ifndef __DICO_H__
#define __DICO_H__

#include "types.h"

enum {DICO_ESUCCESS=0, DICO_ENOENT, DICO_EINVAL, DICO_EBADSIZE, DICO_EFULL, DICO_EMEM, DICO_EDUPLICATE, DICO_EINVALCHAR};
enum {DICTYPE_NULL=0, DICTYPE_U8, DICTYPE_U16, DICTYPE_U32, DICTYPE_U64, DICTYPE_DATA, DICTYPE_STRING};

struct s_dico;
struct s_dicoitem;

typedef struct s_dico cdico;
typedef struct s_dicoitem cdicoitem;

struct s_dico
{
    struct s_dicoitem *head; 
};

struct s_dicoitem
{   u8         type;
    u8         section;
    u16        key;
    u16        size;
    char       *data;
    cdicoitem  *next;
};

cdico *dico_alloc();
int   dico_destroy(cdico *d);
int   dico_show(cdico *d, u8 section, char *debugtxt);
int   dico_count_all_sections(cdico *d);
int   dico_count_one_section(cdico *d, u8 section);
int   dico_add_data(cdico *d, u8 section, u16 key, const void *data, u16 size);
int   dico_add_generic(cdico *d, u8 section, u16 key, const void *data, u16 size, u8 type);
int   dico_get_generic(cdico *d, u8 section, u16 key, void *data, u16 maxsize, u16 *size);
int   dico_get_data(cdico *d, u8 section, u16 key, void *data, u16 maxsize, u16 *size);
int   dico_add_u16(cdico *d, u8 section, u16 key, u16 data);
int   dico_add_u32(cdico *d, u8 section, u16 key, u32 data);
int   dico_add_u64(cdico *d, u8 section, u16 key, u64 data);
int   dico_get_u16(cdico *d, u8 section, u16 key, u16 *data);
int   dico_get_u32(cdico *d, u8 section, u16 key, u32 *data);
int   dico_get_u64(cdico *d, u8 section, u16 key, u64 *data);
int   dico_add_string(cdico *d, u8 section, u16 key, const char *szstring);
int   dico_get_string(cdico *d, u8 section, u16 key, char *buffer, u16 bufsize);

#endif // __DICO_H__
