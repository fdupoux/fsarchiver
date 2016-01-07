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

#ifndef __ARCHINFO_H__
#define __ARCHINFO_H__

struct s_dico;
struct s_archreader;

int archinfo_show_mainhead(struct s_archreader *ai, struct s_dico *dicomainhead);
int archinfo_show_fshead(struct s_dico *dicofshead, int fsid);
char *compalgostr(int algo);
char *cryptalgostr(int algo);

#endif // __ARCHINFO_H__
