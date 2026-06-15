/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2018 Francois Dupoux.  All rights reserved.
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

#ifndef __OPER_RESTORE_H__
#define __OPER_RESTORE_H__

#include "dico.h"
#include "strdico.h"
#include "archreader.h"
#include "error.h" // for cstats

// shared extraction context (was previously private to oper_restore.c).
// Exposed so oper_extract.c (list / extractfiles) can reuse the same pipeline helpers.
typedef struct s_extractar
{   carchreader ai;
    int         fsid;
    cstats      stats;
    u64         cost_global;
    u64         cost_current;
} cextractar;

int oper_restore(char *archive, int argc, char **argv, int oper);

// ---- helpers reused by oper_extract.c (defined in oper_restore.c) ----
int extractar_read_mainhead(cextractar *exar, cdico **dicomainhead);
int extractar_restore_object(cextractar *exar, int *errors, char *destdir, cdico *dicoattr, int fstype);
int extractar_extract_read_objects(cextractar *exar, int *errors, char *destdir, int fstype);
int is_filedir_excluded(char *relpath);
extern int g_erg_listmode; // 1 during "list" (nothing written to disk)

#endif // __OPER_RESTORE_H__
