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

#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <stdlib.h>

#include "fsarchiver.h"
#include "dico.h"
#include "common.h"
#include "extract.h"
#include "create.h"
#include "showpart.h"
#include "archinfo.h"
#include "syncthread.h"
#include "comp_lzo.h"
#include "crypto.h"
#include "options.h"
#include "logfile.h"
#include "error.h"

char *valid_magic[]={FSA_MAGIC_MAIN, FSA_MAGIC_VOLH, FSA_MAGIC_VOLF, FSA_MAGIC_FSIN, 
    FSA_MAGIC_FSYB, FSA_MAGIC_DATF, FSA_MAGIC_OBJT, FSA_MAGIC_BLKH, FSA_MAGIC_FILF, NULL};

void usage(char *progname, bool examples)
{
    int lzo=false, lzma=false, crypto=false;
    
#ifdef OPTION_LZO_SUPPORT
    lzo=true;
#endif // OPTION_LZO_SUPPORT
#ifdef OPTION_LZMA_SUPPORT
    lzma=true;
#endif // OPTION_LZMA_SUPPORT
#ifdef OPTION_CRYPTO_SUPPORT
    crypto=true;
#endif // OPTION_CRYPTO_SUPPORT
    
    msgprintf(MSG_FORCE, "====> fsarchiver version %s (%s) - http://www.fsarchiver.org <====\n", FSA_VERSION, FSA_RELDATE);
    msgprintf(MSG_FORCE, "Distributed under the GPL v2 license (GNU General Public License v2).\n");
    msgprintf(MSG_FORCE, "<usage>\n");
    msgprintf(MSG_FORCE, " * usage: %s [<options>] <command> <archive> [<part1> [<part2> [...]]]\n", progname);
    msgprintf(MSG_FORCE, "<commands>\n");
    msgprintf(MSG_FORCE, " * savefs: save filesystems to an archive file (backup a partition to a file)\n");
    msgprintf(MSG_FORCE, " * restfs: restore filesystems from an archive (overwrites the existing data)\n");
    msgprintf(MSG_FORCE, " * savedir: save directories to the archive (similar to a compressed tarball)\n");
    msgprintf(MSG_FORCE, " * restdir: restore data from an archive which is not based on a filesystem\n");
    msgprintf(MSG_FORCE, " * archinfo: show information about an existing archive file and its contents\n");
    msgprintf(MSG_FORCE, " * probe simple|detailed: show list of filesystems detected on the disks\n");
    msgprintf(MSG_FORCE, "<options>\n");
    msgprintf(MSG_FORCE, " -o: overwrite the archive if it already exists instead of failing\n");
    msgprintf(MSG_FORCE, " -v: verbose mode (can be used several times to increase the level of details)\n");
    msgprintf(MSG_FORCE, " -d: debug mode (can be used several times to increase the level of details)\n");
    msgprintf(MSG_FORCE, " -A: allow to save a filesystem which is mounted in read-write (live backup)\n");
    msgprintf(MSG_FORCE, " -a: allow to run savefs when partition mounted without the acl/xattr options\n");
    msgprintf(MSG_FORCE, " -e <pattern>: exclude files and directories that match that pattern\n");
    msgprintf(MSG_FORCE, " -L <label>: set the label of the archive (comment about the contents)\n");
    msgprintf(MSG_FORCE, " -z <level>: valid compression level are between 1 (very fast) and 9 (very good)\n");
    msgprintf(MSG_FORCE, " -s <mbsize>: split the archive into several files of <mbsize> megabytes each\n");
    msgprintf(MSG_FORCE, " -j <count>: create more than one compression thread. useful on multi-core cpu\n");
#ifdef OPTION_CRYPTO_SUPPORT
    msgprintf(MSG_FORCE, " -c <password>: encrypt/decrypt data in archive. password length: %d to %d chars\n", FSA_MIN_PASSLEN, FSA_MAX_PASSLEN);
#endif // OPTION_CRYPTO_SUPPORT
    msgprintf(MSG_FORCE, " -h: show help and information about how to use fsarchiver with examples\n");
    msgprintf(MSG_FORCE, " -V: show program version and exit\n");
    msgprintf(MSG_FORCE, "Support for optional features: (enabled or disabled during compilation):\n");
    msgprintf(MSG_FORCE, " * support for lzo compression:............%s\n", (lzo==true)?"yes":"no");
    msgprintf(MSG_FORCE, " * support for lzma compression:...........%s\n", (lzma==true)?"yes":"no");
    msgprintf(MSG_FORCE, " * support for encryption:.................%s\n", (crypto==true)?"yes":"no");
    msgprintf(MSG_FORCE, "Warnings:\n");
    msgprintf(MSG_FORCE, " * fsarchiver is still in development, don't use it for critical data yet.\n");
    
    if (examples==true)
    {
        msgprintf(MSG_FORCE, "Examples:\n");
        msgprintf(MSG_FORCE, " * save only one filesystem (/dev/sda1) to an archive:\n");
        msgprintf(MSG_FORCE, "   fsarchiver savefs /data/myarchive1.fsa /dev/sda1\n");
        msgprintf(MSG_FORCE, " * save two filesystems (/dev/sda1 and /dev/sdb1) to an archive:\n");
        msgprintf(MSG_FORCE, "   fsarchiver savefs /data/myarchive2.fsa /dev/sda1 /dev/sdb1\n");
        msgprintf(MSG_FORCE, " * restore the first filesystem from an archive (first = number 0):\n");
        msgprintf(MSG_FORCE, "   fsarchiver restfs /data/myarchive2.fsa id=0,dest=/dev/sda1\n");
        msgprintf(MSG_FORCE, " * restore the second filesystem from an archive (second = number 1):\n");
        msgprintf(MSG_FORCE, "   fsarchiver restfs /data/myarchive2.fsa id=1,dest=/dev/sdb1\n");
        msgprintf(MSG_FORCE, " * restore two filesystems from an archive (number 0 and 1):\n");
        msgprintf(MSG_FORCE, "   fsarchiver restfs /data/arch2.fsa id=0,dest=/dev/sda1 id=1,dest=/dev/sdb1\n");
        msgprintf(MSG_FORCE, " * restore a filesystem from an archive and convert it to reiserfs:\n");
        msgprintf(MSG_FORCE, "   fsarchiver restfs /data/myarchive1.fsa id=0,dest=/dev/sda1,mkfs=reiserfs\n");
        msgprintf(MSG_FORCE, " * save the contents of /usr/src/linux to an archive (similar to tar):\n");
        msgprintf(MSG_FORCE, "   fsarchiver savedir /data/linux-sources.fsa /usr/src/linux\n");
        msgprintf(MSG_FORCE, " * save a filesystem (/dev/sda1) to an archive split into volumes of 680MB:\n");
        msgprintf(MSG_FORCE, "   fsarchiver savefs -s 680 /data/myarchive1.fsa /dev/sda1\n");
        msgprintf(MSG_FORCE, " * save a filesystem and exclude all files/dirs called 'pagefile.*'\n");
        msgprintf(MSG_FORCE, "   fsarchiver savefs /data/myarchive.fsa /dev/sda1 --exclude='pagefile.*'\n");
        msgprintf(MSG_FORCE, " * generic exclude for 'share' such as '/usr/share' and '/usr/local/share':\n");
        msgprintf(MSG_FORCE, "   fsarchiver savefs /data/myarchive.fsa --exclude=share\n");
        msgprintf(MSG_FORCE, " * absolute exclude valid for '/usr/share' but not for '/usr/local/share'\n");
        msgprintf(MSG_FORCE, "   fsarchiver savefs /data/myarchive.fsa --exclude=/usr/share\n");
        msgprintf(MSG_FORCE, " * save a filesystem (/dev/sda1) to an encrypted archive:\n");
        msgprintf(MSG_FORCE, "   fsarchiver savefs -c mypassword /data/myarchive1.fsa /dev/sda1\n");
        msgprintf(MSG_FORCE, " * extract an archive made of simple files to /tmp/extract:\n");
        msgprintf(MSG_FORCE, "   fsarchiver restdir /data/linux-sources.fsa /tmp/extract\n");
        msgprintf(MSG_FORCE, " * show information about an archive and its file systems:\n");
        msgprintf(MSG_FORCE, "   fsarchiver archinfo /data/myarchive2.fsa\n");
    }
}

static struct option const long_options[] =
{
    {"overwrite", no_argument, NULL, 'o'},
    {"allow-no-acl-xattr", no_argument, NULL, 'a'},
    {"allow-rw-mounted", no_argument, NULL, 'A'},
    {"verbose", no_argument, NULL, 'v'},
    {"debug", no_argument, NULL, 'd'},
    {"compress", required_argument, NULL, 'z'},
    {"jobs", required_argument, NULL, 'j'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {"split", required_argument, NULL, 's'},
    {"cryptpass", required_argument, NULL, 'c'},
    {"label", required_argument, NULL, 'L'},
    {"exclude", required_argument, NULL, 'e'},
    {NULL, 0, NULL, 0}
};

int process_cmdline(int argc, char **argv)
{
    char *probemode;
    sigset_t mask_set;
    bool probedetailed=0;
    char *command=NULL;
    char *archive=NULL;
    char *partition[32];
    char tempbuf[1024];
    char *progname;
    int fscount;
    int argcok;
    int ret=0;
    int cmd;
    int c;
    
    // init
    memset(partition, 0, sizeof(partition));
    progname=argv[0];
    
    // set default options
    g_options.overwrite=false;
    g_options.allowsaverw=false;
    g_options.dontcheckmountopts=false;
    g_options.verboselevel=0;
    g_options.debuglevel=0;
    g_options.compressjobs=1;
    g_options.fsacomplevel=3; // fsa level 3 = "gzip -6"
    g_options.compressalgo=FSA_DEF_COMPRESS_ALGO;
    g_options.compresslevel=FSA_DEF_COMPRESS_LEVEL; // default level for gzip
    g_options.datablocksize=FSA_DEF_BLKSIZE;
    g_options.encryptalgo=ENCRYPT_NONE;
    snprintf(g_options.archlabel, sizeof(g_options.archlabel), "<none>");
    g_options.encryptpass[0]=0;
    
    while ((c = getopt_long(argc, argv, "oaAvdz:j:hVs:c:L:e:", long_options, NULL)) != EOF)
    {
        switch (c)
        {
            case 'o': // overwrite existing archives
                g_options.overwrite=true;
                break;
            case 'a': // don't check the mount options for already-mounted filesystems
                g_options.dontcheckmountopts=true;
                break;
            case 'A': // allows to backup read/write mounted partition
                g_options.allowsaverw=true;
                break;
            case 'v': // verbose mode
                g_options.verboselevel++;
                break;
            case 'V': // version
                msgprintf(MSG_FORCE, "fsarchiver %s (%s)\n", FSA_VERSION, FSA_RELDATE);
                return 0;
            case 'd': // debug mode
                g_options.debuglevel++;
                break;
            case 'j': // compression jobs
                g_options.compressjobs=atoi(optarg);
                if (g_options.compressjobs<1 || g_options.compressjobs>FSA_MAX_COMPJOBS)
                {
                    errprintf("[%s] is not a valid job number. Must be between 1 and %d\n", optarg, FSA_MAX_COMPJOBS);
                    usage(progname, false);
                    return 1;
                }
                break;
            case 'e': // exclude files/directories
                strlist_add(&g_options.exclude, optarg);
                break;
            case 's': // split archive into several volumes
                g_options.splitsize=((u64)atoll(optarg))*((u64)1024LL*1024LL);
                if (g_options.splitsize==0)
                {
                    errprintf("argument of option -s is invalid (%s). It must be a valid integer\n", optarg);
                    usage(progname, false);
                    return -1;
                }
                else // show the calculated size since it was probably incorrect before due to an integer overflow
                {
                    msgprintf(MSG_FORCE, "Archive will be split into volumes of %lld bytes (%s)\n",
                        (long long)g_options.splitsize, format_size(g_options.splitsize, tempbuf, sizeof(tempbuf), 'h'));
                }
                break;
            case 'z': // compression level
                g_options.fsacomplevel=atoi(optarg);
                if (g_options.fsacomplevel<1 || g_options.fsacomplevel>9)
                {   errprintf("[%s] is not a valid compression level, it must be an integer between 1 and 9.\n", optarg);
                    usage(progname, false);
                    return -1;
                }
                if (options_select_compress_level(g_options.fsacomplevel)<0)
                    return -1;
                if (g_options.fsacomplevel>=8)
                    msgprintf(MSG_FORCE, "High compression levels may require a huge amount of memory\n"
                        "Please read the man page or \"http://www.fsarchiver.org/Compression\" for more details.\n");
                break;
            case 'c': // encryption
#ifdef OPTION_CRYPTO_SUPPORT
                g_options.encryptalgo=ENCRYPT_BLOWFISH;
                if (strlen(optarg)<FSA_MIN_PASSLEN || strlen(optarg)>FSA_MAX_PASSLEN)
                {   errprintf("the password lenght is incorrect, it must between %d and %d chars.\n", FSA_MIN_PASSLEN, FSA_MAX_PASSLEN);
                    usage(progname, false);
                    return -1;
                }
                snprintf((char*)g_options.encryptpass, FSA_MAX_PASSLEN, "%s", optarg);
#else // OPTION_CRYPTO_SUPPORT
                errprintf("support for encryption has been disabled at compilation, cannot use that option.\n");
                return 1;
#endif // OPTION_CRYPTO_SUPPORT
                break;
            case 'L': // archive label
                snprintf(g_options.archlabel, sizeof(g_options.archlabel), "%s", optarg);
                break;
            case 'h': // help
                usage(progname, true);
                return 0;
            default:
                usage(progname, false);
                return -1;
        }
    }
    
    argc -= optind;
    argv += optind;
    
    // in all cases we need at least 1 parameters
    if (argc < 1)
    {   fprintf(stderr, "the first argument must be a command.\n");
        usage(progname, false);
        return -1;
    }
    else // mandatory and unique parameters
    {   
        command=*argv++, argc--;
    }
    
    // calculate threshold for small files that are compressed together
    g_options.smallfilethresh=min(g_options.datablocksize/4, FSA_MAX_SMALLFILESIZE);
    msgprintf(MSG_DEBUG1, "Files smaller than %ld will be packed with other small files\n", (long)g_options.smallfilethresh);

    // convert commands to integers
    if (strcmp(command, "savefs")==0)
    {   cmd=OPER_SAVEFS;
        argcok=(argc>=2);
    }
    else if (strcmp(command, "restfs")==0)
    {   cmd=OPER_RESTFS;
        argcok=(argc>=2);
    }
    else if (strcmp(command, "savedir")==0)
    {   cmd=OPER_SAVEDIR;
        argcok=(argc>=2);
    }
    else if (strcmp(command, "restdir")==0)
    {   cmd=OPER_RESTDIR;
        argcok=(argc==2);
    }
    else if (strcmp(command, "archinfo")==0)
    {   cmd=OPER_ARCHINFO;
        argcok=(argc==1);
    }
    else if (strcmp(command, "probe")==0)
    {   cmd=OPER_PROBE;
        argcok=(argc==1);
    }
    else // command not found
    {   errprintf("[%s] is not a valid command.\n", command);
        usage(progname, false);
        return -1;
    }
    
    // check there are enough parameters on the cmd line
    if (argcok!=true)
    {   errprintf("invalid arguments on the command line\n");
        usage(progname, false);
        return -1;
    }
    
    // commands that require an archive as the first argument
    if (cmd==OPER_SAVEFS || cmd==OPER_RESTFS || cmd==OPER_SAVEDIR || cmd==OPER_RESTDIR || cmd==OPER_ARCHINFO)
    {
        archive=*argv++, argc--;
    }
    else if (cmd==OPER_PROBE)
    {
        probemode=*argv++, argc--;
        if (strcmp(probemode, "simple")==0)
        {   probedetailed=false;
        }
        else if (strcmp(probemode, "detailed")==0)
        {   probedetailed=true;
        }
        else
        {   errprintf("command 'probe' expects one argument: it must be either 'simple' or 'detailed'\n");
            usage(progname, false);
            return -1;
        }
    }
    
    // list of partitions to backup/restore
    for (fscount=0; (fscount < argc) && (argv[fscount]); fscount++)
        partition[fscount]=argv[fscount];
    
    // install signal handlers
    sigemptyset(&mask_set);
    sigaddset(&mask_set, SIGINT);
    sigaddset(&mask_set, SIGTERM);
    sigprocmask(SIG_SETMASK, &mask_set, NULL);
    
    switch (cmd)
    {
        case OPER_SAVEFS:
            ret=do_create(archive, partition, fscount, ARCHTYPE_FILESYSTEMS);
            break;
        case OPER_SAVEDIR:
            ret=do_create(archive, partition, fscount, ARCHTYPE_DIRECTORIES);
            break;
        case OPER_RESTFS:
        case OPER_RESTDIR:
        case OPER_ARCHINFO:
            ret=do_extract(archive, partition, fscount, cmd);
            break;
        case OPER_PROBE:
            ret=partlist_showlist(probedetailed);
            break;
        default:
            errprintf("[%s] is not a valid command.\n", command);
            usage(progname, false);
            ret=-1;
            break;
    };
    
    return ret;
}

int main(int argc, char **argv)
{
    int ret;
    
    // must be run as root
    if (geteuid()!=0)
    {   errprintf("%s must be run as root. cannot continue.\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // init the lzo library
#ifdef OPTION_LZO_SUPPORT
    if (lzo_init() != LZO_E_OK)
    {   errprintf("internal error - lzo_init() failed\n");
        exit(EXIT_FAILURE);
    }
#endif // OPTION_LZO_SUPPORT
    
    // init libgcrypt
#ifdef OPTION_CRYPTO_SUPPORT
    if (crypto_init()!=0)
    {   errprintf("cannot initialize the crypto environment\n");
        exit(EXIT_FAILURE);
    }
#endif // OPTION_CRYPTO_SUPPORT
    
    // init
    logfile_open();
    options_init(&g_options);
    queue_init(&g_queue, FSA_MAX_QUEUESIZE);
    
    // bulk of the program
    ret=process_cmdline(argc, argv);
    
    // cleanup
    queue_destroy(&g_queue);
    options_destroy(&g_options);
    logfile_close();
    
    // cleanup libgcrypt
#ifdef OPTION_CRYPTO_SUPPORT
    crypto_cleanup();
#endif // OPTION_CRYPTO_SUPPORT
    
    return !!ret;
}
