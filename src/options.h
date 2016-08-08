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

#ifndef __OPTIONS_H__
#define __OPTIONS_H__

#include "strlist.h"

struct s_options;
typedef struct s_options coptions;

// struct that stores the options passed on the command line
struct s_options
{   bool     overwrite;
    bool     allowsaverw;
    bool     experimental;
    bool     dontcheckmountopts;
    int      verboselevel;
    int      debuglevel;
    int      compresslevel;
    int      compressjobs;
    u16      compressalgo;
    u32      datablocksize;
    u32      smallfilethresh;
    u64      splitsize;
    u16      encryptalgo;
    u16      fsacomplevel;
	char     archlabel[FSA_MAX_LABELLEN];
    u8       encryptpass[FSA_MAX_PASSLEN+1];
    cstrlist exclude;
};

extern coptions g_options;

int options_init();
int options_destroy();
int options_select_compress_level(int opt);

#endif // __OPTIONS_H__
