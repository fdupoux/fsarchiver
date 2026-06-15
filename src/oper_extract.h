/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2018 Francois Dupoux.  All rights reserved.
 * Copyright (C) 2026 Gabriel Diaconu.
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

/*
 * oper_extract.h -- public interface for the "list" and "extractfiles" verbs.
 */

#ifndef __OPER_EXTRACT_H__
#define __OPER_EXTRACT_H__

#include "dico.h"

// public entry points (dispatched from fsarchiver.c)
//   oper_list:         list the contents of an archive (text or JSONL on stdout)
//   oper_extractfiles: extract individual files matching patterns into destdir
int oper_list(char *archive);
int oper_extractfiles(char *archive, char *destdir, int patcount, char **patterns);

// listing hook called from oper_restore.c while g_erg_listmode!=0 (prints one object)
void erg_list_print_object(cdico *d, unsigned int objtype);

#endif // __OPER_EXTRACT_H__
