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
#include <unistd.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <wordexp.h>
#include <fnmatch.h>
#include <time.h>
#include <limits.h>

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#include "fsarchiver.h"
#include "syncthread.h"
#include "strlist.h"
#include "common.h"
#include "error.h"

int stream_readline(FILE *f, char *buf, int buflen)
{
    int i;
    char c;

    memset(buf, 0, buflen);
    for (i=0; (!feof(f)) && ((c=fgetc(f))!='\n') && (!feof(f)) && (i < buflen); i++)
        buf[i]=c;
    return i;
}

void concatenate_paths(char *buffer, int maxbufsize, char *p1, char *p2)
{
    int i;

    memset(buffer, 0, maxbufsize);
    for (i=0; (p1[i]) && (p1[i]!='/' || p1[i+1]); i++)
        *(buffer++)=p1[i];
    *(buffer++)='/';
    while (*p2=='/')
        p2++;
    strlcatf(buffer, maxbufsize, "%s", p2);
}

char *format_size(u64 size, char *text, int max, char units)
{   
    double dSize;
    
    u64 llKiloB = 1024LL;
    u64 llMegaB = 1024LL * llKiloB;
    u64 llGigaB = 1024LL * llMegaB;
    u64 llTeraB = 1024LL * llGigaB;
    
    if ((units=='b') || ((units=='h') && size < llKiloB)) // In Bytes
    {   snprintf(text, max, "%lld bytes", (long long)size);
    }
    else if ((units=='k') || ((units=='h') && size < llMegaB)) // In KiloBytes
    {   dSize = ((double)size) / ((double)llKiloB);
        snprintf(text, max, "%.2f KB", (float) dSize);
    }
    else if ((units=='m') || ((units=='h') && size < llGigaB)) // In MegaBytes
    {   dSize = ((double)size) / ((double)llMegaB);
        snprintf(text, max, "%.2f MB", (float) dSize);
    }
    else if ((units=='g') || ((units=='h') && size < llTeraB)) // In Gigabytes
    {   dSize = ((double)size) / ((double)llGigaB);
        snprintf(text, max, "%.2f GB", (float) dSize);
    }
    else // bigger than 1TB
    {   dSize = ((double)size) / ((double)llTeraB);
        snprintf(text, max, "%.2f TB", (float) dSize);
    }
    
    return text;
}

int mkdir_recursive(char *path)
{
    char buffer[PATH_MAX];
    struct stat64 statbuf;
    int len;
    int pos;

    len=strlen(path);

    for (pos=0; pos<=len; pos++)
    {
        if (path[pos]=='/' || path[pos]==0)
        {
            memset(buffer, 0, sizeof(buffer));
            memcpy(buffer, path, pos);
            if (stat64(buffer, &statbuf)==-1 && errno==ENOENT)
                mkdir(buffer, 0755);
        }
    }
    
    return 0;
}

int extract_dirpath(char *filepath, char *dirbuf, int dirbufsize)
{
    int i;

    snprintf(dirbuf, dirbufsize, "%s", filepath);
    for (i=0; (i<dirbufsize) && (dirbuf[i]!=0); i++);
    while ((i>=0) && (dirbuf[i]!='/'))
        dirbuf[i--]=0;
    if ((i>0) && (dirbuf[i]=='/'))
        dirbuf[i]=0;
    return 0;
}

int extract_basename(char *filepath, char *basenamebuf, int basenamebufsize)
{
    int i;

    for (i=0; filepath[i]; i++);
    while ((i>0) && (filepath[i]!='/') && (filepath[i-1]!='/'))
        i--;
    snprintf(basenamebuf, basenamebufsize, "%s", &filepath[i]);

    return 0;
}

char *get_objtype_name(int objtype)
{
    switch (objtype)
    {
        case OBJTYPE_DIR:
            return ("DIR     ");
        case OBJTYPE_SYMLINK:
            return ("SYMLINK ");
        case OBJTYPE_REGFILEUNIQUE:
            return ("REGFILE ");
        case OBJTYPE_REGFILEMULTI:
            return ("REGFILEM");
        case OBJTYPE_HARDLINK:
            return ("HARDLINK");
        case OBJTYPE_CHARDEV:
            return ("CHARDEV ");
        case OBJTYPE_BLOCKDEV:
            return ("BLOCKDEV");
        case OBJTYPE_FIFO:
            return ("FIFO    ");
        case OBJTYPE_SOCKET:
            return ("SOCKET  ");
        default:
            return ("UNKNOWN ");
    }
}

int is_dir_empty(char *path)
{
    char fullpath[PATH_MAX];
    struct stat64 statbuf;
    struct dirent *dir;
    DIR *dirdesc;
    
    dirdesc = opendir(path);
    if (!dirdesc)
    {      sysprintf("cannot open directory %s\n", path);
        return -1;
    }
    
    while ((dir = readdir(dirdesc)) != NULL)
    {
        concatenate_paths(fullpath, sizeof(fullpath), path, dir->d_name);
        if (lstat64(fullpath, &statbuf)!=0)
        {   sysprintf ("cannot stat %s\n", fullpath);
            closedir(dirdesc);
            return -1;
        }
        if (strcmp(dir->d_name,".")!=0 && strcmp(dir->d_name,"..")!=0)
        {
            closedir(dirdesc);
            return 1; // an item was found
        }        
    }

    closedir(dirdesc);
    return 0;
}

// generate a non-null random u32
u32 generate_random_u32_id(void)
{
    struct timeval now;
    u32 archid;

    memset(&now, 0, sizeof(struct timeval));
    do
    {   gettimeofday(&now, NULL);
        archid=((u32)now.tv_sec)^((u32)now.tv_usec);
    } while (archid==0);
    return archid;
}

u32 fletcher32(u8 *data, u32 len)
{
    u32 sum1 = 0xffff, sum2 = 0xffff;
    
    while (len)
    {
        unsigned tlen = len > 360 ? 360 : len;
        len -= tlen;
        do {
            sum1 += *data++;
            sum2 += sum1;
        } while (--tlen);
        sum1 = (sum1 & 0xffff) + (sum1 >> 16);
        sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    }
    // Second reduction step to reduce sums to 16 bits
    sum1 = (sum1 & 0xffff) + (sum1 >> 16);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    return sum2 << 16 | sum1;
}

int regfile_exists(char *filepath)
{
    struct stat64 st;
    int res;

    res=stat64(filepath, &st);
    if ((res==0) && S_ISREG(st.st_mode))
        return true; // file exists
    if (res==-1 && errno==ENOENT)
        return false; // does not exist
    return -1; // don't know
}

int getpathtoprog(char *buffer, int bufsize, char *prog)
{
    char pathtest[PATH_MAX];
    char delims[]=":\t\n";
    struct stat bufstat;
    char pathenv[4096];
    char *saveptr=0;
    char *result;
    char *vp;
    int i;
    
    memset(buffer, 0, bufsize);
    if ((vp=getenv("PATH")) == NULL)
        return -1;
    snprintf(pathenv, sizeof(pathenv), "%s", vp);
    result=strtok_r(pathenv, delims, &saveptr);
    for(i=0; result != NULL; i++)
    {
        snprintf(pathtest, sizeof(pathtest), "%s/%s", result, prog);
        if (stat(pathtest, &bufstat)==0 && access(pathtest, X_OK)==0)
        {
            snprintf(buffer, bufsize, "%s", pathtest);
            return 0;
        }
        result = strtok_r(NULL, delims, &saveptr);
    }
    return -1;
}

int exec_command(char *command, int cmdbufsize, int *exitst, char *stdoutbuf, int stdoutsize, char *stderrbuf, int stderrsize, char *format, ...)
{
    char pathtoprog[PATH_MAX]; // full path to the program to run
    const int max_argv=128; // maximum arguments to a command
    char *argv[max_argv]; // pointer to arguments processed by wordexp()
    int outpos=0; // how many bytes have already been stored in the buffer for stdout
    int errpos=0; // how many bytes have already been stored in the buffer for stderr
    int pfildes1[2];
    int pfildes2[2];
    wordexp_t p;
    int status;
    int mystdout;
    int mystderr;
    va_list ap;
    int cmdpid;
    int flags;
    int pid;
    int res;
    char c;
    int i;
    
    // init
    memset(pathtoprog, 0, sizeof(pathtoprog));
    for (i=0; i < max_argv; argv[i++]=NULL);
    if (exitst)
        *exitst=-1;
    
    // format the string
    if (stdoutbuf && stdoutsize)
        memset(stdoutbuf, 0, stdoutsize);
    if (stderrbuf && stderrsize)
        memset(stderrbuf, 0, stderrsize);
    memset(command, 0, cmdbufsize);
    va_start(ap, format);
    vsnprintf(command, cmdbufsize, format, ap);
    va_end(ap);
    
    // do shell expansion to parse the quotes for args with spaces
    wordexp(command, &p, 0); // will require wordfree to free the memory
    for(i=0; (i < p.we_wordc) && (i<max_argv); i++)
    {   argv[i]=p.we_wordv[i];
        msgprintf(MSG_DEBUG1, "argv[%d]=[%s]\n", i, argv[i]);
    }
    
    // get the full path to the program
    if (getpathtoprog(pathtoprog, sizeof(pathtoprog), argv[0])!=0)
    {   errprintf("program [%s] no found in PATH or bad permissions on that program\n", argv[0]);
        wordfree(&p);
        return -1;
    }
    msgprintf(MSG_VERB2, "getpathtoprog(%s)=[%s]\n", argv[0], pathtoprog);
    
    // execute the command
    if ((pipe(pfildes1)==-1) || (pipe(pfildes2)==-1))
    {   errprintf("pipe() failed\n");
        wordfree(&p);
        return -1;
    }
    
    msgprintf(MSG_VERB1, "executing [%s]...\n", command);
    
    if ((pid = fork()) == -1)
    {   sysprintf("fork() failed\n");
        wordfree(&p);
        return -1;
    }
    
    if (pid == 0) // child process --> command to be executed
    {   close(pfildes1[0]); // close read end of pipe
        close(pfildes2[0]); // close read end of pipe
        dup2(pfildes1[1],1); // make 1 same as write-to end of pipe
        close(pfildes1[1]); // close excess fildes
        dup2(pfildes2[1],2); // make 1 same as write-to end of pipe
        close(pfildes2[1]); // close excess fildes
        setenv("LC_ALL", "C", 1); // kill internationalization
        execvp(pathtoprog, argv);
        errprintf("execvp(%s) failed\n", pathtoprog); // still around? exec failed
        wordfree(&p);
        exit(EXIT_FAILURE);
    }
    else // parent --> fsarchiver
    {   mystdout=pfildes1[0];
        mystderr=pfildes2[0];
        cmdpid=pid;
        close(pfildes1[1]); // close write end of pipe
        close(pfildes2[1]); // close write end of pipe
        
        // set non blocking reads to make sure we dont block on read(stderr) when buffer for stdout is full for instance
        flags = fcntl(mystdout, F_GETFL, 0);
        fcntl(mystdout, F_SETFL, flags | O_NONBLOCK);
        flags = fcntl(mystderr, F_GETFL, 0);
        fcntl(mystderr, F_SETFL, flags | O_NONBLOCK);
        
        do
        {   // read data stored in the stdout pipe (read to prevents the sub-process command from blocking on write)
            do
            {   res=read(mystdout, &c, 1);
                if ((stdoutbuf!=NULL) && (res>0) && (outpos+1 < stdoutsize))
                    stdoutbuf[outpos++]=c;
            } while (res>0);
            
            // read data stored in the stderr pipe (read to prevents the sub-process command from blocking on write)
            do
            {   res=read(mystderr, &c, 1);
                if ((stderrbuf!=NULL) && (res>0) && (errpos+1 < stderrsize))
                    stderrbuf[errpos++]=c;
            } while (res>0);
            
            usleep(100000); // don't spin the cpu
        } while ( ((res=waitpid(cmdpid, &status, WNOHANG))!=cmdpid) && (res!=-1) );
        
        // read the remaining data in the pipes 
        if ((stdoutbuf!=NULL) && (outpos+1 < stdoutsize))
            read(mystdout, stdoutbuf+outpos, stdoutsize-outpos-1);
        if ((stderrbuf!=NULL) && (errpos+1 < stderrsize))
            read(mystderr, stderrbuf+errpos, stderrsize-errpos-1);
        
        msgprintf(MSG_VERB1, "command [%s] returned %d\n", command, WEXITSTATUS(status));
        if (exitst)
            *exitst=WEXITSTATUS(status);
        
        if ((stdoutbuf!=NULL) && (outpos>0))
            msgprintf(MSG_DEBUG1, "\n----stdout----\n%s\n----stdout----\n\n", stdoutbuf);
        if ((stderrbuf!=NULL) && (errpos>0))
            msgprintf(MSG_DEBUG1, "\n----stderr----\n%s\n----stderr----\n\n", stderrbuf);
        
        wordfree(&p);
        return 0;
    }
}

int generate_random_tmpdir(char *buffer, int bufsize, int n)
{
    struct tm tbreak;
    time_t abstime;
    struct timeval tv;
    
    gettimeofday(&tv, NULL);
    abstime=tv.tv_sec;
        localtime_r(&abstime, &tbreak);
    
    snprintf(buffer, bufsize, "/tmp/fsa/%.4d%.2d%.2d-%.2d%.2d%.2d-%.8x-%.2d",
        tbreak.tm_year+1900, tbreak.tm_mon+1, tbreak.tm_mday,
         tbreak.tm_hour, tbreak.tm_min, tbreak.tm_sec, (u32)tv.tv_usec, n);
    return 0;
}

// returns true if magic is a valid magic-string
int is_magic_valid(char *magic)
{
    int i;
    for (i=0; valid_magic[i]!=NULL; i++)
        if (memcmp(magic, valid_magic[i], FSA_SIZEOF_MAGIC)==0)
            return true;
    return false;
}

// just copies the path if it has the right extension or add the extension
int path_force_extension(char *buf, int bufsize, char *origpath, char *ext)
{
    int oldlen, extlen;
    
    if (!buf || !origpath || !ext)
    {   errprintf("a parameter is null\n");
        return -1;
    }
    
    oldlen=strlen(origpath);
    extlen=strlen(ext);
    
    if ((oldlen < extlen) || memcmp(origpath+oldlen-extlen, ext, extlen)!=0)
        snprintf(buf, bufsize, "%s%s", origpath, ext);
    else // the extension has been found at the end of origpath
        snprintf(buf, bufsize, "%s", origpath);
    
    return 0;
}

// convert a binary md5 to an hexadecimal format
char *format_md5(char *buf, int maxbuf, u8 *md5bin)
{
    int i;
    
    memset(buf, 0, maxbuf);
    for (i=0; i<16; i++)
        strlcatf(buf, maxbuf, "%.2x", md5bin[i]);
    
    return buf;
}

char *format_time(char *buffer, int bufsize, u64 t)
{
    struct tm timeres;
    time_t t2;
    
    if (!buffer)
        return NULL;
    t2=t;
    
    localtime_r(&t2, &timeres);
    snprintf(buffer, bufsize, "%.4d-%.2d-%.2d_%.2d-%.2d-%.2d",
        timeres.tm_year+1900, timeres.tm_mon+1, timeres.tm_mday,
        timeres.tm_hour, timeres.tm_min, timeres.tm_sec);
    return buffer;
}

// add a formatted string at the end of a buffer that already contains a string
char *strlcatf(char *dest, int destbufsize, char *format, ...)
{
    va_list ap;
    int len1;
    
    // if buffer already full, don't cat the second string
    if ((len1=strnlen(dest, destbufsize))==destbufsize)
        return dest;
    
    // at the new formatted string at the end of the first one
    va_start(ap, format);
    vsnprintf(dest+len1, destbufsize-len1, format, ap);
    va_end(ap);
    
    return dest;
}

int get_parent_dir_time_attrib(char *filepath, char *parentdirbuf, int bufsize, struct timeval *tv)
{
    struct stat64 statbuf;
    
    extract_dirpath(filepath, parentdirbuf, bufsize);
    
    if (lstat64(parentdirbuf, &statbuf)!=0)
    {   sysprintf("cannot lstat64(%s)\n", parentdirbuf);
        return -1;
    }
    
    if (!S_ISDIR(statbuf.st_mode))
    {   sysprintf("error: [%s] is not a directory\n", parentdirbuf);
        return -1;
    }
    
    // prepare structure to be passed to utimes
    tv[0].tv_usec=0;
    tv[0].tv_sec=statbuf.st_atime;
    tv[1].tv_usec=0;
    tv[1].tv_sec=statbuf.st_mtime;

    return 0;
}

int stats_show(cstats stats, int fsid)
{
    msgprintf(MSG_FORCE, "Statistics for filesystem %d\n", fsid);
    msgprintf(MSG_FORCE, "* files successfully processed:....regfiles=%lld, directories=%lld, "
        "symlinks=%lld, hardlinks=%lld, specials=%lld\n", 
        (long long)stats.cnt_regfile, (long long)stats.cnt_dir, (long long)stats.cnt_symlink, 
        (long long)stats.cnt_hardlink, (long long)stats.cnt_special);
    msgprintf(MSG_FORCE, "* files with errors:...............regfiles=%lld, directories=%lld, "
        "symlinks=%lld, hardlinks=%lld, specials=%lld\n", 
        (long long)stats.err_regfile, (long long)stats.err_dir, (long long)stats.err_symlink, 
        (long long)stats.err_hardlink, (long long)stats.err_special);
    return 0;
}

u64 stats_errcount(cstats stats)
{
    return stats.err_regfile+stats.err_dir+stats.err_symlink+stats.err_hardlink+stats.err_special;
}

int format_stacktrace(char *buffer, int bufsize)
{
#ifdef HAVE_EXECINFO_H
    const int stack_depth=20;
    void *temp[stack_depth];
    char **strings;
    int nptrs;
    int i;
    
    // format the backtrace (advanced error info)
    memset(buffer, 0, bufsize);
    nptrs=backtrace(temp, stack_depth);
    strings=backtrace_symbols(temp, nptrs);
    if (strings!=NULL)
    {
        for (i = 0; i < nptrs; i++)
            strlcatf(buffer, bufsize, "%s\n", strings[i]);
        free(strings);
    }
#endif
    
    return 0;
}

int exclude_check(cstrlist *patlist, char *string)
{
    char pattern[1024];
    int count;
    int i;
    
    count=strlist_count(patlist);
    for (i=0; i < count; i++)
    {
        strlist_getitem(patlist, i, pattern, sizeof(pattern));
        if (fnmatch(pattern, string, 0)==0)
            return true;
    }
    return false;
}

int get_path_to_volume(char *newvolbuf, int bufsize, char *basepath, long curvol)
{
    char prefix[PATH_MAX];
    int pathlen;
    
    if ((pathlen=strlen(basepath))<4) // all archives terminates with ".fsa"
    {   errprintf("archive has an invalid basepath: [%s]\n", basepath);
        return -1;
    }
    
    if (curvol==0) // first volume
    {
        if (realpath(basepath, newvolbuf)!=newvolbuf)
            snprintf(newvolbuf, bufsize, "%s", basepath);
    }
    else // not the first volume
    {
        memset(prefix, 0, sizeof(prefix));
        memcpy(prefix, basepath, pathlen-2);
        snprintf(newvolbuf, bufsize, "%s%.2ld", prefix, (long)curvol);
    }
    
    return 0;
}

s64 get_device_size(char *partition)
{
    s64 devsize;
    int fd;

    if ((fd=open64(partition, O_RDONLY|O_LARGEFILE))<0)
        return -1;
    if ((devsize=lseek64(fd, 0, SEEK_END))<0)
        return -1;
    close(fd);

    return devsize;
}
