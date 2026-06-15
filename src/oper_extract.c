/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2018 Francois Dupoux.  All rights reserved.
 * Copyright (C) 2026 Gabriel Diaconu.
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

/*
 * oper_extract.c -- read-only operations "list" and "extractfiles".
 *
 * Reuses the archive-reading pipeline (reader + decompression threads, queue,
 * main-header/FSIN/DIRS parsing and the per-object restore helpers) from
 * oper_restore.c. Never calls the destructive mkfs/mount path: objects are
 * only drained. "list" prints metadata (text or JSONL); "extractfiles" writes
 * the files matching the given patterns into a destination directory.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

#include "fsarchiver.h"
#include "dico.h"
#include "common.h"
#include "options.h"
#include "oper_restore.h"
#include "oper_extract.h"
#include "archreader.h"
#include "thread_comp.h"
#include "thread_archio.h"
#include "syncthread.h"
#include "strlist.h"
#include "error.h"
#include "queue.h"

// ---------------------------------------------------------------------------
// JSON string escaping (RFC 8259): quotes, backslash, control chars.
// Bytes >=0x20 (incl. UTF-8 multibyte) are emitted verbatim.
// ---------------------------------------------------------------------------
static void erg_json_escape(FILE *out, const char *s)
{
    const unsigned char *p=(const unsigned char *)s;
    for (; *p; p++)
    {
        switch (*p)
        {
            case '\"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\b': fputs("\\b",  out); break;
            case '\f': fputs("\\f",  out); break;
            case '\n': fputs("\\n",  out); break;
            case '\r': fputs("\\r",  out); break;
            case '\t': fputs("\\t",  out); break;
            default:
                if (*p < 0x20)
                    fprintf(out, "\\u%04x", (unsigned int)*p);
                else
                    fputc(*p, out);
                break;
        }
    }
}

// one-letter type code used in JSON output (mirrors "tar"/ls conventions)
static char erg_objtype_code(unsigned int objtype)
{
    switch (objtype)
    {
        case OBJTYPE_DIR:           return 'd';
        case OBJTYPE_SYMLINK:       return 'l';
        case OBJTYPE_HARDLINK:      return 'h';
        case OBJTYPE_CHARDEV:       return 'c';
        case OBJTYPE_BLOCKDEV:      return 'b';
        case OBJTYPE_FIFO:          return 'p';
        case OBJTYPE_SOCKET:        return 's';
        case OBJTYPE_REGFILEUNIQUE: return 'f';
        case OBJTYPE_REGFILEMULTI:  return 'f';
        default:                    return '?';
    }
}

// render the permission/type field like "tar tv": e.g. "-rw-r--r--", "drwxr-xr-x"
static void erg_format_perms(char *buf, unsigned int objtype, unsigned int mode)
{
    char t;
    switch (objtype)
    {
        case OBJTYPE_DIR:      t='d'; break;
        case OBJTYPE_SYMLINK:  t='l'; break;
        case OBJTYPE_CHARDEV:  t='c'; break;
        case OBJTYPE_BLOCKDEV: t='b'; break;
        case OBJTYPE_FIFO:     t='p'; break;
        case OBJTYPE_SOCKET:   t='s'; break;
        default:               t='-'; break; // regfile, hardlink
    }
    buf[0]=t;
    buf[1]=(mode & 0400)?'r':'-';
    buf[2]=(mode & 0200)?'w':'-';
    buf[3]=(mode & 0100)?((mode & 04000)?'s':'x'):((mode & 04000)?'S':'-');
    buf[4]=(mode & 0040)?'r':'-';
    buf[5]=(mode & 0020)?'w':'-';
    buf[6]=(mode & 0010)?((mode & 02000)?'s':'x'):((mode & 02000)?'S':'-');
    buf[7]=(mode & 0004)?'r':'-';
    buf[8]=(mode & 0002)?'w':'-';
    buf[9]=(mode & 0001)?((mode & 01000)?'t':'x'):((mode & 01000)?'T':'-');
    buf[10]=0;
}

// ---------------------------------------------------------------------------
// erg_list_print_object: called from the restore helpers (in oper_restore.c)
// for each object while g_erg_listmode!=0. Prints one record to stdout.
// All metadata comes from the (uncompressed, never-encrypted) object header.
// ---------------------------------------------------------------------------
void erg_list_print_object(cdico *d, unsigned int objtype)
{
    char relpath[PATH_MAX];
    char target[PATH_MAX];
    char perms[12];
    u32 mode=0, uid=0, gid=0;
    u64 size=0, mtime=0;
    int hastarget=0;

    if (dico_get_data(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_PATH, relpath, sizeof(relpath), NULL)!=0)
        snprintf(relpath, sizeof(relpath), "(unknown)");

    dico_get_u32(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_MODE, &mode);
    dico_get_u32(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_UID, &uid);
    dico_get_u32(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_GID, &gid);
    dico_get_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_SIZE, &size);
    dico_get_u64(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_MTIME, &mtime);

    target[0]=0;
    if (objtype==OBJTYPE_SYMLINK)
        hastarget=(dico_get_string(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_SYMLINK, target, sizeof(target))>=0);
    else if (objtype==OBJTYPE_HARDLINK)
        hastarget=(dico_get_string(d, DICO_OBJ_SECTION_STDATTR, DISKITEMKEY_HARDLINK, target, sizeof(target))>=0);

    if (g_options.json) // JSONL: one JSON object per line
    {
        fprintf(stdout, "{\"type\":\"%c\",\"path\":\"", erg_objtype_code(objtype));
        erg_json_escape(stdout, relpath);
        fprintf(stdout, "\",\"size\":%llu,\"mode\":\"%04o\",\"uid\":%u,\"gid\":%u,\"mtime\":%llu",
            (unsigned long long)size, (unsigned int)(mode & 07777), (unsigned int)uid,
            (unsigned int)gid, (unsigned long long)mtime);
        if (hastarget)
        {
            fputs(",\"target\":\"", stdout);
            erg_json_escape(stdout, target);
            fputc('\"', stdout);
        }
        fputs("}\n", stdout);
    }
    else // text: tar-tv-like columns
    {
        erg_format_perms(perms, objtype, (unsigned int)mode);
        fprintf(stdout, "%s %u/%-u %12llu %llu %s",
            perms, (unsigned int)uid, (unsigned int)gid,
            (unsigned long long)size, (unsigned long long)mtime, relpath);
        if (hastarget)
            fprintf(stdout, " -> %s", target);
        fputc('\n', stdout);
    }
    fflush(stdout);
}

// ---------------------------------------------------------------------------
// erg_extract_pipeline: shared read-only driver for list & extractfiles.
// Mirrors the reader/decomp thread setup of oper_restore() but never makes or
// mounts a filesystem; for every fs in the archive it drains objects, calling
// the restore helpers (which write only the objects that are not excluded).
// ---------------------------------------------------------------------------
static int erg_extract_pipeline(char *archive, char *destdir)
{
    pthread_t thread_decomp[FSA_MAX_COMPJOBS];
    char magic[FSA_SIZEOF_MAGIC+1];
    cdico *dicomainhead=NULL;
    cdico *dicofsinfo=NULL;
    cdico *dicobegin=NULL;
    cdico *dicoend=NULL;
    cdico *dirsinfo=NULL;
    pthread_t thread_reader;
    cextractar exar;
    u64 totalerr=0;
    u32 temp32=0;
    int errors=0;
    int ret=0;
    int i;

    // init
    memset(&exar, 0, sizeof(exar));
    archreader_init(&exar.ai);
    for (i=0; i<FSA_MAX_COMPJOBS; i++)
        thread_decomp[i]=0;
    for (i=0; i<FSA_MAX_FSPERARCH; i++)
        g_fsbitmap[i]=0;
    thread_reader=0;

    snprintf(exar.ai.basepath, PATH_MAX, "%s", archive);

    // we want every filesystem's objects to reach the queue
    for (i=0; i<FSA_MAX_FSPERARCH; i++)
        g_fsbitmap[i]=1;

    // create decompression threads
    for (i=0; (i<g_options.compressjobs) && (i<FSA_MAX_COMPJOBS); i++)
    {
        if (pthread_create(&thread_decomp[i], NULL, thread_decomp_fct, NULL) != 0)
        {   errprintf("pthread_create(thread_decomp_fct) failed\n");
            goto erg_pipe_error;
        }
    }

    // create archive-reader thread
    if (pthread_create(&thread_reader, NULL, thread_reader_fct, (void*)&exar.ai) != 0)
    {   errprintf("pthread_create(thread_reader_fct) failed\n");
        goto erg_pipe_error;
    }

    // read archive main header (also verifies the password if the archive is encrypted)
    if (extractar_read_mainhead(&exar, &dicomainhead)<0)
    {   msgprintf(MSG_STACK, "read_mainhead(%s) failed\n", archive);
        goto erg_pipe_error;
    }

    if (exar.ai.archtype==ARCHTYPE_FILESYSTEMS)
    {
        // read the FSIN header for each filesystem
        for (i=0; (i < exar.ai.fscount) && (i<FSA_MAX_FSPERARCH); i++)
        {
            if (queue_dequeue_header(&g_queue, &dicofsinfo, magic, NULL)<=0)
            {   errprintf("queue_dequeue_header() failed: cannot read filesystem-info dico\n");
                goto erg_pipe_error;
            }
            if (memcmp(magic, FSA_MAGIC_FSIN, FSA_SIZEOF_MAGIC)!=0)
            {   errprintf("header is not what we expected: found=[%s] and expected=[%s]\n", magic, FSA_MAGIC_FSIN);
                goto erg_pipe_error;
            }
            dico_destroy(dicofsinfo);
            dicofsinfo=NULL;
        }

        // drain the objects of each filesystem (FSYB ... objects ... DATF)
        for (i=0; (i < exar.ai.fscount) && (i<FSA_MAX_FSPERARCH) && (get_abort()==false); i++)
        {
            exar.fsid=i;
            memset(&exar.stats, 0, sizeof(exar.stats));

            // read "begin of filesystem" header (FSYB)
            if (queue_dequeue_header(&g_queue, &dicobegin, magic, NULL)<=0)
            {   errprintf("queue_dequeue_header() failed: cannot read file system dico\n");
                goto erg_pipe_error;
            }
            dico_destroy(dicobegin);
            dicobegin=NULL;
            if (memcmp(magic, FSA_MAGIC_FSYB, FSA_SIZEOF_MAGIC)!=0)
            {   errprintf("header is not what we expected: found=[%s] and expected=[%s]\n", magic, FSA_MAGIC_FSYB);
                goto erg_pipe_error;
            }

            if (extractar_extract_read_objects(&exar, &errors, destdir, 0)!=0)
            {   errprintf("extract_read_objects() failed for filesystem %d\n", i);
                goto erg_pipe_error;
            }

            // read "end of filesystem" footer (DATF)
            if (queue_dequeue_header(&g_queue, &dicoend, magic, NULL)<=0)
            {   errprintf("queue_dequeue_header() failed: cannot read end-of-fs footer\n");
                goto erg_pipe_error;
            }
            dico_destroy(dicoend);
            dicoend=NULL;
            if ((get_interrupted()==false) && (memcmp(magic, FSA_MAGIC_DATF, FSA_SIZEOF_MAGIC)!=0))
            {   errprintf("header is not what we expected: found=[%s] and expected=[%s]\n", magic, FSA_MAGIC_DATF);
                goto erg_pipe_error;
            }

            if (g_erg_listmode==0)
                totalerr+=stats_errcount(exar.stats);
        }
    }
    else if (exar.ai.archtype==ARCHTYPE_DIRECTORIES)
    {
        // optional DIRS info header (fsarchiver >= 0.6.7)
        if (dico_get_u32(dicomainhead, 0, MAINHEADKEY_HASDIRSINFOHEAD, &temp32)==0 && temp32==true)
        {
            if (queue_dequeue_header(&g_queue, &dirsinfo, magic, NULL)<=0)
            {   errprintf("queue_dequeue_header() failed: cannot read the dirsinfo header\n");
                goto erg_pipe_error;
            }
            if (memcmp(magic, FSA_MAGIC_DIRS, FSA_SIZEOF_MAGIC)!=0)
            {   errprintf("header is not what we expected: found=[%s] and expected=[%s]\n", magic, FSA_MAGIC_DIRS);
                goto erg_pipe_error;
            }
            dico_destroy(dirsinfo);
            dirsinfo=NULL;
        }

        exar.fsid=0;
        memset(&exar.stats, 0, sizeof(exar.stats));
        if (extractar_extract_read_objects(&exar, &errors, destdir, 0)!=0)
        {   errprintf("extract_read_objects(%s) failed\n", destdir);
            goto erg_pipe_error;
        }
        if (g_erg_listmode==0)
            totalerr+=stats_errcount(exar.stats);
    }
    else
    {   errprintf("this archive has an unknown type: %d, cannot continue\n", exar.ai.archtype);
        goto erg_pipe_error;
    }

    if (get_abort()==true)
        msgprintf(MSG_FORCE, "operation aborted by user\n");

    // NB: we do NOT treat get_stopfillqueue()==true as an error here. The reader
    // thread sets it when it reaches the end of the archive, which for small
    // read-only operations may well happen before we get here (a benign race in
    // oper_restore that only matters because restdir usually has trailing data).
    if (get_abort()==false)
        goto erg_pipe_success;

erg_pipe_error:
    ret=-1;

erg_pipe_success:
    fflush(stderr); // close output cleanly at end of list/extractfiles
    set_stopfillqueue(); // ask thread-archio to terminate
    while (queue_count_items_todo(&g_queue)>0)
        usleep(10000);
    while (get_secthreads()>0 && queue_get_end_of_queue(&g_queue)==false)
        queue_destroy_first_item(&g_queue);

    for (i=0; (i<g_options.compressjobs) && (i<FSA_MAX_COMPJOBS); i++)
        if (thread_decomp[i] && pthread_join(thread_decomp[i], NULL) != 0)
            errprintf("pthread_join(thread_decomp) failed\n");
    if (thread_reader && pthread_join(thread_reader, NULL) != 0)
        errprintf("pthread_join(thread_reader) failed\n");

    if (dicomainhead) dico_destroy(dicomainhead);
    archreader_destroy(&exar.ai);

    // NB: we intentionally do NOT fail on stats_errcount(). The restore helpers
    // bump err_dir/err_symlink/err_special for every object that is *skipped*
    // because it did not match an include pattern (the "excluded" goto leads to
    // the per-type error label). Those skips are normal for list/extractfiles.
    // Genuine fatal errors (corrupt data, write failures) cause an early
    // goto erg_pipe_error above, so ret already reflects them.
    (void)totalerr;
    return ret;
}

// ---------------------------------------------------------------------------
// public entry points
// ---------------------------------------------------------------------------
int oper_list(char *archive)
{
    int ret;
    g_erg_listmode=1; // nothing gets written to disk
    // destdir is irrelevant in list mode (everything is excluded), but the
    // restore helpers build paths under it: use a harmless scratch root.
    ret=erg_extract_pipeline(archive, "/");
    g_erg_listmode=0;
    return ret;
}

int oper_extractfiles(char *archive, char *destdir, int patcount, char **patterns)
{
    struct stat st;
    int ret;
    int i;

    if (destdir==NULL || destdir[0]==0)
    {   errprintf("no output directory given (use -o <dir>)\n");
        return -1;
    }
    // create destdir if it does not exist (like a friendly "mkdir -p")
    if (stat(destdir, &st)!=0)
    {
        if (mkdir_recursive(destdir)!=0)
        {   errprintf("cannot create output directory [%s]\n", destdir);
            return -1;
        }
    }
    else if (!S_ISDIR(st.st_mode))
    {   errprintf("[%s] exists and is not a directory, cannot continue\n", destdir);
        return -1;
    }

    if (patcount<1)
    {   errprintf("no file pattern given for extractfiles\n");
        return -1;
    }
    for (i=0; i<patcount; i++)
        strlist_add(&g_options.include, patterns[i]);

    g_erg_listmode=0;
    ret=erg_extract_pipeline(archive, destdir);
    return ret;
}
