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

#ifndef __SYNCTHREAD_H__
#define __SYNCTHREAD_H__

// global threads sync data
extern struct s_queue g_queue; // queue use to share data between the three sort of threads

// global threads sync functions
int get_abort(); // returns true if threads must exit because an error or signal received

bool get_interrupted(); // returns true if either abort is true of stopfillqueue is true

// say to the thread that is filling the queue to stop
void set_stopfillqueue();
bool get_stopfillqueue();

// secondary threads counter
void inc_secthreads();
void dec_secthreads();
int get_secthreads();

// filesystem bitmap used by do_extract() to say to threadio_readimg which filesystems to skip
// eg: "g_fsbitmap[0]=1,g_fsbitmap[1]=0" means that we want to read filesystem 0 and skip fs 1
extern u8 g_fsbitmap[FSA_MAX_FSPERARCH];

#endif // __SYNCTHREAD_H__
