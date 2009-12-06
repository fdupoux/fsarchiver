/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2009 Francois Dupoux.  All rights reserved.
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

#ifndef __RESULT_H__
#define __RESULT_H__

typedef struct s_result
{
    char    text[4096];
    char    stack[8192];
    int        status;
    int        origline;
    char    origfunction[256];
    char    origfile[256];
} cres;

#define resnew(fmt, args...) _resnew(__FILE__ , __FUNCTION__, __LINE__, fmt, ## args)
cres _resnew(const char *file, const char *fct, const int line, int status, char *format, ...);
cres resappend(cres ret1, char *format, ...);

#endif // __RESULT_H__
