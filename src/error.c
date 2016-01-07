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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "fsarchiver.h"
#include "error.h"
#include "common.h"
#include "options.h"
#include "logfile.h"

int fsaprintf(int level, bool showerrno, bool showloc, const char *file, const char *fct, int line, char *format, ...)
{
    char buffer[8192];
    char temp[1024];
    bool msgscreen;
    bool msglogfile;
    va_list ap;
    
    // init
    memset(buffer, 0, sizeof(buffer));
    msgscreen=(level <= g_options.verboselevel);
    msglogfile=(level <= g_options.debuglevel);
    
    if (msgscreen || msglogfile)
    {
        // 1. format errno and its meaning
        if (showerrno)
            strlcatf(buffer, sizeof(buffer), "[errno=%d, %s]: ", errno, strerror(errno));
        
        // 2. format location of the message
        if (showloc)
            strlcatf(buffer, sizeof(buffer), "%s#%d,%s(): ", file, line, fct);
        
        // 3. format text message
        va_start(ap, format);
        vsnprintf(temp, sizeof(temp), format, ap);
        va_end(ap);
        strlcatf(buffer, sizeof(buffer), "%s", temp);
        
        // 4. show message on screen
        if (msgscreen)
        {   fprintf(stderr, "%s", buffer);
            fflush(stderr);
        }
        
        // 5. write message in logfile if requested
        if (msglogfile)
            logfile_write(buffer, strlen(buffer));
    }
    
    return 0;
}

char *error_int_to_string(s64 err)
{
    switch (err)
    {
        case FSAERR_SUCCESS:    return "FSAERR_SUCCESS";
        case FSAERR_UNKNOWN:    return "FSAERR_UNKNOWN";
        case FSAERR_ENOMEM:     return "FSAERR_ENOMEM";
        case FSAERR_EINVAL:     return "FSAERR_EINVAL";
        case FSAERR_ENOENT:     return "FSAERR_ENOENT";
        case FSAERR_ENDOFFILE:  return "FSAERR_ENDOFFILE";
        case FSAERR_WRONGTYPE:  return "FSAERR_WRONGTYPE";
        case FSAERR_NOTOPEN:    return "FSAERR_NOTOPEN";
        default:                return "FSAERR_?";
    }
}
