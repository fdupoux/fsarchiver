/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2010 Francois Dupoux.  All rights reserved.
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fsarchiver.h"
#include "logfile.h"
#include "common.h"
#include "error.h"

int g_logfile=-1;

int logfile_open()
{
    mkdir_recursive("/var/log");
    g_logfile=open64("/var/log/fsarchiver.log", O_RDWR|O_CREAT|O_TRUNC|O_LARGEFILE, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (g_logfile>=0)
    {   msgprintf(1, "Creating logfile in /var/log/fsarchiver.log\n");
        msgprintf(1, "Running fsarchiver version=[%s], fileformat=[%s]\n", FSA_VERSION, FSA_FILEFORMAT);
        return 0;
    }
    else
    {   sysprintf("Cannot create logfile in /var/log/fsarchiver.log\n");
        return -1;
    }
}

int logfile_close()
{
    close(g_logfile);
    return 0;
}

int logfile_write(char *str, int len)
{
    if (g_logfile>=0)
        return write(g_logfile, str, len);
    else
        return -1;
}
