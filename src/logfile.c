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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>

#include "fsarchiver.h"
#include "logfile.h"
#include "common.h"
#include "error.h"

int g_logfile=-1;

int logfile_open()
{
    char logpath[PATH_MAX];
    char timestamp[1024];
    char *logdir="/var/log";  
    
    format_time(timestamp, sizeof(timestamp), time(NULL));
    snprintf(logpath, sizeof(logpath), "%s/fsarchiver_%s_%ld.log", logdir, timestamp, (long)getpid());
    mkdir_recursive(logdir);
    
    g_logfile=open64(logpath, O_RDWR|O_CREAT|O_TRUNC|O_LARGEFILE, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (g_logfile>=0)
    {   msgprintf(MSG_VERB1, "Creating logfile in %s\n", logpath);
        msgprintf(MSG_VERB1, "Running fsarchiver version=[%s], fileformat=[%s]\n", FSA_VERSION, FSA_FILEFORMAT);
        return FSAERR_SUCCESS;
    }
    else
    {   sysprintf("Cannot create logfile in %s\n", logpath);
        return FSAERR_UNKNOWN;
    }
}

int logfile_close()
{
    close(g_logfile);
    return FSAERR_SUCCESS;
}

int logfile_write(char *str, int len)
{
    if (g_logfile>=0)
        return write(g_logfile, str, len);
    else
        return FSAERR_UNKNOWN;
}
