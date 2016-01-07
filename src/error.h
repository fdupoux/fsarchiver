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

#ifndef __ERROR_H__
#define __ERROR_H__

struct s_stats;
typedef struct s_stats cstats;

struct s_stats
{   u64    cnt_regfile;
    u64    cnt_dir;
    u64    cnt_symlink;
    u64    cnt_hardlink;
    u64    cnt_special;
    u64    err_regfile;
    u64    err_dir;
    u64    err_symlink;
    u64    err_hardlink;
    u64    err_special;
};

int fsaprintf(int level, bool showerrno, bool showloc, const char *file, 
    const char *fct, int line, char *format, ...) 
    __attribute__ ((format (printf, 7, 8)));

// ---- message levels
enum {MSG_FORCE=0,     // always show this messages whatever the level is
      MSG_VERB1=1,     // normal messages that have to be shown when verbose>=1 (fsarchive -v)
      MSG_VERB2=2,     // detailed messages that have to be shown when verbose>=2 (fsarchive -vv)
      MSG_STACK=3,     // messages shown when a function exists (propagate error at upper level)
      MSG_DEBUG1=4,    // debugging messages level 1 (very basic info)
      MSG_DEBUG2=5,    // debugging messages level 2
      MSG_DEBUG3=6,    // debugging messages level 3
      MSG_DEBUG4=7,    // debugging messages level 4
      MSG_DEBUG5=8     // debugging messages level 5 (very detailed debug)
};

char *error_int_to_string(s64 err);

// use sysprintf to print an error that follows a libc function and to show errno
#define sysprintf(fmt, args...) fsaprintf(0, true, true, __FILE__, __FUNCTION__, __LINE__, fmt, ## args)

// use errprintf to print an error that does not come from a libc function
#define errprintf(fmt, args...) fsaprintf(0, false, true, __FILE__, __FUNCTION__, __LINE__, fmt, ## args)

// use msgprintf with a level to show normal messages or debug messages
#define msgprintf(level, fmt, args...) fsaprintf(level, false, level>=3, __FILE__, __FUNCTION__, __LINE__, fmt, ## args)

#endif // __ERROR_H__
