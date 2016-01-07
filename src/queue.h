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

#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <pthread.h>

enum {QITEM_STATUS_NULL=0, QITEM_STATUS_TODO, QITEM_STATUS_PROGRESS, QITEM_STATUS_DONE};
enum {QITEM_TYPE_NULL=0, QITEM_TYPE_BLOCK, QITEM_TYPE_HEADER};

struct s_dico;

struct s_blockinfo;
typedef struct s_blockinfo cblockinfo;

struct s_headinfo;
typedef struct s_headinfo cheadinfo;

struct s_queueitem;
typedef struct s_queueitem cqueueitem;

struct s_queue;
typedef struct s_queue cqueue;

struct s_blockinfo // used when (type==QITEM_TYPE_BLOCK)
{   char                 *blkdata; // pointer to data block as it is at a particular time (compressed or uncompressed)
    u32                  blkrealsize; // size of the data in the normal state (not compressed and not crypted)
    u64                  blkoffset; // offset of the block in the normal file
    u32                  blkarcsum; // checksum of the block as it it when it's in the archive (compressed and encrypted)
    u32                  blkarsize; // size of the block as it is in the archive (compressed and encrypted)
    u16                  blkcompalgo; // algo used to compressed the block
    u32                  blkcompsize; // size of the block after compression and before encryption
    u16                  blkcryptalgo; // algo used to compressed the block
    u16                  blkfsid; // id of filesystem to which the block belongs
    bool                 blklocked; // true if locked (being processed in the compress/crypt thread)
};

struct s_headinfo // used when (type==QITEM_TYPE_HEADER)
{   char                 magic[FSA_SIZEOF_MAGIC+1]; // magic which is used to identify the type of header
    u16                  fsid; // the filesystem to which this header belongs to, or FSA_FILESYSID_NULL if global header
    struct s_dico        *dico;
};

struct s_queueitem
{   int                  type; // QITEM_TYPE_BLOCK or QITEM_TYPE_HEADER
    int                  status; // compressed, being-compressed, not-yet-compressed
    s64                  itemnum; // unique identifier of the item in the queue
    cqueueitem           *next; // next block in the linked list
    cblockinfo           blkinfo; // used when type==QITEM_TYPE_BLOCK (for blocks only)
    cheadinfo            headinfo; // used when type==QITEM_TYPE_HEADER (for headers only)
};

struct s_queue
{   cqueueitem           *head; // head of the queue: first item
    pthread_mutex_t      mutex; // pthread mutex for data protection
    pthread_cond_t       cond; // condition for pthread synchronization
    s64                  curitemnum; // unique id given to every new item (block or header)
    u64                  itemcount; // how many items there are (headers + blocks)
    u64                  blkcount; // how many blocks items there are (items where type==QITEM_TYPE_BLOCK only)
    u64                  blkmax; // how many blocks items there can be before the queue is considered as full
    bool                 endofqueue; // set to true when no more data to put in queue (like eof): reader must stop
};

// ----return status
// a) ">0" means success for functions that return item numbers
// b) =0   means success for functions that do not return item numbers
// c) "<0" QERR error number

// init and destroy
s64  queue_init(cqueue *l, s64 blkmax);
s64  queue_destroy(cqueue *l);

// information functions
s64  queue_count(cqueue *l);
s64  queue_count_status(struct s_queue *l, int status);
s64  queue_is_first_item_ready(struct s_queue *q);
s64  queue_check_next_item(cqueue *q, int *type, char *magic);
s64  queue_count_items_todo(cqueue *q);

// modification functions
s64  queue_add_block(cqueue *q, cblockinfo *blkinfo, int status);
s64  queue_add_header(cqueue *q, struct s_dico *d, char *magic, u16 fsid);
s64  queue_add_header_internal(cqueue *q, cheadinfo *headinfo);
s64  queue_replace_block(cqueue *q, s64 itemnum, cblockinfo *blkinfo, int newstatus);
s64  queue_destroy_first_item(cqueue *q);

// end of queue functions
s64  queue_set_end_of_queue(cqueue *q, bool state);
bool queue_get_end_of_queue(cqueue *q);

// get item from queue functions
s64  queue_get_first_block_todo(cqueue *q, cblockinfo *blkinfo);
s64  queue_dequeue_header(cqueue *q, struct s_dico **d, char *magicbuf, u16 *fsid);
s64  queue_dequeue_header_internal(cqueue *q, cheadinfo *headinfo);
s64  queue_dequeue_block(cqueue *q, cblockinfo *blkinfo);
s64  queue_dequeue_first(cqueue *q, int *type, cheadinfo *headinfo, cblockinfo *blkinfo);

#endif // __QUEUE_H__
