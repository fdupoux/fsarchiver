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

#include <signal.h>

#include "fsarchiver.h"
#include "fsarchiver.h"
#include "syncthread.h"
#include "queue.h"

// queue use to share data between the three sort of threads
cqueue g_queue;

// filesystem bitmap used by do_extract() to say to threadio_readimg which filesystems to skip
// eg: "g_fsbitmap[0]=1,g_fsbitmap[1]=0" means that we want to read filesystem 0 and skip fs 1
u8 g_fsbitmap[FSA_MAX_FSPERARCH];

// g_stopfillqueue is set to true when the threads that reads the queue wants to stop
// either because there is an error or because it does not need the next data
atomic_t g_stopfillqueue={ (false) };
atomic_t g_aborted={ (false) };

void set_stopfillqueue()
{   
    atomic_set(&g_stopfillqueue, true);
}

bool get_stopfillqueue()
{
    return atomic_read(&g_stopfillqueue);
}

// how many secondary threads are running (compression/decompression and archio threads)
atomic_t g_secthreads={ (0) };

void inc_secthreads()
{
    (void)__sync_add_and_fetch(&g_secthreads.counter, 1);
}

void dec_secthreads()
{
    (void)__sync_sub_and_fetch(&g_secthreads.counter, 1);
}

int get_secthreads()
{
    return atomic_read(&g_secthreads);
}

bool get_interrupted()
{
    return (get_abort()==true || get_stopfillqueue()==true);
}

// get_abort() returns true if a SIGINT/SIGTERM has been received (interrupted by the user)
int get_abort()
{
    int mysigs[]={SIGINT, SIGTERM, -1};
    sigset_t mask_set;
    sigpending(&mask_set);
    int i;
    
    if (atomic_read(&g_aborted)==true)
        return true;
    
    if (sigpending(&mask_set)==0)
    {
        for (i=0; mysigs[i]!=-1; i++)
        {
            if (sigismember(&mask_set, mysigs[i]))
            {   //msgprintf(MSG_FORCE, "get_terminate(): received signal %d\n", mysigs[i]);
                atomic_set(&g_aborted, true);
                return true;
            }
        }
    }
    
    return false;
}
