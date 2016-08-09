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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#include "fsarchiver.h"
#include "queue.h"
#include "dico.h"
#include "common.h"
#include "syncthread.h"
#include "error.h"

struct timespec get_timeout()
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_sec++;
    return t;
}

s64 queue_init(cqueue *q, s64 blkmax)
{
    pthread_mutexattr_t attr;

    if (!q)
    {   errprintf("q is NULL\n");
        return FSAERR_EINVAL;
    }
    
    // ---- init default attributes
    q->head=NULL;
    q->curitemnum=1;
    q->itemcount=0;
    q->blkcount=0;
    q->blkmax=blkmax;
    q->endofqueue=false;
    
    // ---- init pthread structures
    assert(pthread_mutexattr_init(&attr)==0);
    assert(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK)==0);
    if (pthread_mutex_init(&q->mutex, &attr)!=0)
    {   msgprintf(3, "pthread_mutex_init failed\n");
        return FSAERR_UNKNOWN;
    }
    
    if (pthread_cond_init(&q->cond,NULL)!=0)
    {   msgprintf(3, "pthread_cond_init failed\n");
        return FSAERR_UNKNOWN;
    }
    
    return FSAERR_SUCCESS;
}

s64 queue_destroy(cqueue *q)
{
    cqueueitem *cur;
    cqueueitem *next;
    
    if (!q)
    {   errprintf("q is NULL\n");
        return FSAERR_EINVAL;
    }
    
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    for (cur=q->head; cur!=NULL; cur=next)
    {
        next=cur->next;
        free(cur);
    }
    q->head=NULL;
    q->itemcount=0;
    
    assert(pthread_mutex_unlock(&q->mutex)==0);
    
    assert(pthread_mutex_destroy(&q->mutex)==0);
    assert(pthread_cond_destroy(&q->cond)==0);
    
    return FSAERR_SUCCESS;
}

s64 queue_set_end_of_queue(cqueue *q, bool state)
{
    if (!q)
    {   errprintf("q is NULL\n");
        return FSAERR_EINVAL;
    }

    assert(pthread_mutex_lock(&q->mutex)==0);
    q->endofqueue=state;
    assert(pthread_mutex_unlock(&q->mutex)==0);
    pthread_cond_broadcast(&q->cond);
    return FSAERR_SUCCESS;
}

bool queue_get_end_of_queue(cqueue *q)
{
    bool res;
    if (!q)
    {   errprintf("q is NULL\n");
        return FSAERR_EINVAL;
    }

    assert(pthread_mutex_lock(&q->mutex)==0);
    res=((q->itemcount<1) && (q->endofqueue==true));
    assert(pthread_mutex_unlock(&q->mutex)==0);
    return res;
}

bool queuelocked_get_end_of_queue(cqueue *q)
{
    bool res;
    if (!q)
    {   errprintf("q is NULL\n");
        return FSAERR_EINVAL;
    }

    res=((q->itemcount<1) && (q->endofqueue==true));
    return res;
}

// runs with the mutex unlocked (external users)
s64 queue_count(cqueue *q)
{
    s64 itemcount;
    
    if (!q)
    {   errprintf("q is NULL\n");
        return FSAERR_EINVAL;
    }

    assert(pthread_mutex_lock(&q->mutex)==0);
    itemcount = q->itemcount;
    assert(pthread_mutex_unlock(&q->mutex)==0);
    
    return itemcount;
}

// how many items in the queue have a particular status
s64 queue_count_status(cqueue *q, int status)
{
    cqueueitem *cur;
    int count=0;
    
    if (!q)
    {   errprintf("q is NULL\n");
        return FSAERR_EINVAL;
    }

    assert(pthread_mutex_lock(&q->mutex)==0);
    
    for (cur=q->head; cur!=NULL; cur=cur->next)
    {
        if (status==QITEM_STATUS_NULL || cur->status==status)
            count++;
    }
    
    assert(pthread_mutex_unlock(&q->mutex)==0);
    
    return count;
}

// add a block at the end of the queue
s64 queue_add_block(cqueue *q, cblockinfo *blkinfo, int status)
{
    cqueueitem *item;
    cqueueitem *cur;
    
    if (!q || !blkinfo)
    {   errprintf("a parameter is NULL\n");
        return FSAERR_EINVAL;
    }
    
    // create the new item in memory
    item=malloc(sizeof(cqueueitem));
    if (!item)
    {   errprintf("malloc(%ld) failed: out of memory\n", (long)sizeof(cqueueitem));
        return FSAERR_ENOMEM;
    }
    item->type=QITEM_TYPE_BLOCK;
    item->status=status;
    item->blkinfo=*blkinfo;
    item->next=NULL;
    
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    // does not make sense to add item on a queue where endofqueue is true
    if (q->endofqueue==true)
    {   free (item);
        assert(pthread_mutex_unlock(&q->mutex)==0);
        return FSAERR_ENDOFFILE;
    }
    
    // wait while (queue-is-full) to let the other threads remove items first
    while (q->blkcount > q->blkmax)
    {
        struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
    }
    
    if (q->head==NULL) // if list empty: item is head
    {
        q->head=item;
    }
    else // list not empty: add items at the end
    {
        for (cur=q->head; (cur!=NULL) && (cur->next!=NULL); cur=cur->next);
        cur->next=item;
    }
    
    q->blkcount++;
    q->itemcount++;
    item->itemnum=q->curitemnum++;
    
    assert(pthread_mutex_unlock(&q->mutex)==0);
    pthread_cond_broadcast(&q->cond);
    
    return FSAERR_SUCCESS;
}

s64 queue_add_header(cqueue *q, cdico *d, char *magic, u16 fsid)
{
    cheadinfo headinfo;
    
    if (!q || !d || !magic)
    {   errprintf("parameter is null\n");
        return FSAERR_EINVAL;
    }
    
    memset(&headinfo, 0, sizeof(headinfo));
    memcpy(headinfo.magic, magic, FSA_SIZEOF_MAGIC);
    headinfo.fsid=fsid;
    headinfo.dico=d;
    
    return     queue_add_header_internal(q, &headinfo);
}

s64 queue_add_header_internal(cqueue *q, cheadinfo *headinfo)
{
    cqueueitem *item;
    cqueueitem *cur;
    
    if (!q || !headinfo)
    {   errprintf("parameter is null\n");
        return FSAERR_EINVAL;
    }
    
    // create the new item in memory
    item=malloc(sizeof(cqueueitem));
    if (!item)
    {   errprintf("malloc(%ld) failed: out of memory 1\n", (long)sizeof(cqueueitem));
        return FSAERR_ENOMEM;
    }
    
    item->headinfo=*headinfo;
    item->type=QITEM_TYPE_HEADER;
    item->status=QITEM_STATUS_DONE;
    item->next=NULL;
    
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    // does not make sense to add item on a queue where endofqueue is true
    if (q->endofqueue==true)
    {   free(item);
        assert(pthread_mutex_unlock(&q->mutex)==0);
        return FSAERR_ENDOFFILE;
    }
    
    // wait while (queue-is-full) to let the other threads remove items first
    while (q->blkcount > q->blkmax)
    {
        struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
    }
    
    item->itemnum=q->curitemnum++;
    if (q->head==NULL) // if list empty
    {
        q->head=item;
    }
    else // list not empty
    {
        for (cur=q->head; (cur!=NULL) && (cur->next!=NULL); cur=cur->next);
        cur->next=item;
    }
    
    q->itemcount++;
    assert(pthread_mutex_unlock(&q->mutex)==0);
    pthread_cond_broadcast(&q->cond);
    
    return FSAERR_SUCCESS;
}

// function called by the compression thread when a block has been compressed
s64 queue_replace_block(cqueue *q, s64 itemnum, cblockinfo *blkinfo, int newstatus)
{
    cqueueitem *cur;
    
    if (!q || !blkinfo)
    {   errprintf("a parameter is null\n");
        return FSAERR_EINVAL;
    }
    
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    if (q->head==NULL)
    {   assert(pthread_mutex_unlock(&q->mutex)==0);
        msgprintf(MSG_DEBUG1, "q->head is NULL: list is empty\n");
        return FSAERR_ENOENT; // item not found
    }
    
    for (cur=q->head; cur!=NULL; cur=cur->next)
    {
        if (cur->itemnum==itemnum) // block found
        {
            cur->status=newstatus;
            cur->blkinfo=*blkinfo;
            assert(pthread_mutex_unlock(&q->mutex)==0);
            pthread_cond_broadcast(&q->cond);
            return FSAERR_SUCCESS;
        }
    }
    
    assert(pthread_mutex_unlock(&q->mutex)==0);
    return FSAERR_ENOENT; // not found
}

// get number of items to be processed
s64 queue_count_items_todo(cqueue *q)
{
    cqueueitem *cur;
    s64 count=0;
    
    if (!q)
    {   errprintf("a parameter is null\n");
        return FSAERR_EINVAL;
    }
    
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    for (cur=q->head; cur!=NULL; cur=cur->next)
    {
        if ((cur->type==QITEM_TYPE_BLOCK) && (cur->status!=QITEM_STATUS_DONE))
            count++;
    }
    
    assert(pthread_mutex_unlock(&q->mutex)==0);
    
    return count;
}

// the compression thread requires the first block which has not yet been compressed
s64 queue_get_first_block_todo(cqueue *q, cblockinfo *blkinfo)
{
    cqueueitem *cur;
    s64 itemfound=-1;
    int res;
    
    if (!q || !blkinfo)
    {   errprintf("a parameter is null\n");
        return FSAERR_EINVAL;
    }
    
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    while (queuelocked_get_end_of_queue(q)==false)
    {
        for (cur=q->head; cur!=NULL; cur=cur->next)
        {
            if ((cur->type==QITEM_TYPE_BLOCK) && (cur->status==QITEM_STATUS_TODO))
            {
                *blkinfo=cur->blkinfo;
                cur->status=QITEM_STATUS_PROGRESS;
                itemfound=cur->itemnum;
                assert(pthread_mutex_unlock(&q->mutex)==0);
                pthread_cond_broadcast(&q->cond);
                return itemfound; // ">0" means item found
            }
        }
        
        struct timespec t=get_timeout();
        if ((res=pthread_cond_timedwait(&q->cond, &q->mutex, &t))!=0 && res!=ETIMEDOUT)
        {   assert(pthread_mutex_unlock(&q->mutex)==0);
            return FSAERR_UNKNOWN;
        }
        
    }
    
    // if it failed at the other end of the queue
    assert(pthread_mutex_unlock(&q->mutex)==0);
    
    if (queuelocked_get_end_of_queue(q))
        return FSAERR_ENDOFFILE;
    else
        return FSAERR_UNKNOWN;
}

// the writer thread requires the first block of the queue if it ready to go
//s64 queue_dequeue_first(cqueue *q, int *type, char *magic, cdico **dico, cblockinfo *blkinfo)
s64 queue_dequeue_first(cqueue *q, int *type, cheadinfo *headinfo, cblockinfo *blkinfo)
{
    cqueueitem *cur=NULL;
    s64 itemfound=-1;
    int ret;
    
    if (!q || !headinfo || !blkinfo)
    {   errprintf("a parameter is null\n");
        return FSAERR_EINVAL;
    }
    
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    while (queuelocked_get_end_of_queue(q)==false)
    {
        if (((cur=q->head)!=NULL) && (cur->status==QITEM_STATUS_DONE))
        {
            if (cur->type==QITEM_TYPE_BLOCK) // item to dequeue is a block
            {
                q->blkcount--;
                *type=cur->type;
                itemfound=cur->itemnum;
                *blkinfo=cur->blkinfo;
                q->head=cur->next;
                free(cur);
                q->itemcount--;
                assert(pthread_mutex_unlock(&q->mutex)==0);
                pthread_cond_broadcast(&q->cond);
                return itemfound; // ">0" means item found
            }
            else if (cur->type==QITEM_TYPE_HEADER) // item to dequeue is a dico
            {
                *headinfo=cur->headinfo;
                *type=cur->type;
                itemfound=cur->itemnum;
                q->head=cur->next;
                free(cur);
                q->itemcount--;
                assert(pthread_mutex_unlock(&q->mutex)==0);
                pthread_cond_broadcast(&q->cond);
                return itemfound; // ">0" means item found
            }
            else
            {
                errprintf("invalid item type in queue\n");
                assert(pthread_mutex_unlock(&q->mutex)==0);
                return FSAERR_EINVAL;
            }
        }
        
        struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
    }
    
    // if it failed at the other end of the queue
    ret=(queuelocked_get_end_of_queue(q)==true)?FSAERR_ENDOFFILE:FSAERR_UNKNOWN;
    assert(pthread_mutex_unlock(&q->mutex)==0);
    
    return ret;
}

// the extract function wants to read headers from the queue
s64 queue_dequeue_block(cqueue *q, cblockinfo *blkinfo)
{
    cqueueitem *cur;
    s64 itemnum;
    
    if (!q || !blkinfo)
    {   errprintf("a parameter is null\n");
        return FSAERR_EINVAL;
    }
    
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    // while ((first-item-of-the-queue-is-not-ready) && (not-at-the-end-of-the-queue))
    while ( (((cur=q->head)==NULL) || (cur->status!=QITEM_STATUS_DONE)) && (queuelocked_get_end_of_queue(q)==false) )
    {
        struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
    }
    
    // if it failed at the other end of the queue
    if (queuelocked_get_end_of_queue(q))
    {   assert(pthread_mutex_unlock(&q->mutex)==0);
        return FSAERR_ENDOFFILE;
    }
    
    cur=q->head;
    assert(cur!=NULL); // queuelocked_is_first_block_ready means there is at least one block in the queue
    
    // test the first item
    if ((cur->type==QITEM_TYPE_BLOCK) && (cur->status==QITEM_STATUS_DONE))
    {
        *blkinfo=cur->blkinfo;
        q->head=cur->next;
        itemnum=cur->itemnum;
        free(cur);
        q->blkcount--;
        q->itemcount--;
        assert(pthread_mutex_unlock(&q->mutex)==0);
        pthread_cond_broadcast(&q->cond);
        return itemnum;
    }
    else
    {
        errprintf("dequeue - wrong type of data in the queue: wanted a block, found an header\n");
        assert(pthread_mutex_unlock(&q->mutex)==0);
        pthread_cond_broadcast(&q->cond);
        return FSAERR_WRONGTYPE;  // ok but not found
    }
}

s64 queue_dequeue_header(cqueue *q, cdico **d, char *magicbuf, u16 *fsid)
{
    cheadinfo headinfo;
    s64 lres;
    
    if (!q || !d || !magicbuf)
    {   errprintf("a parameter is null\n");
        return FSAERR_EINVAL;
    }
    
    if ((lres=queue_dequeue_header_internal(q, &headinfo))<=0)
    {   msgprintf(MSG_STACK, "queue_dequeue_header_internal() failed\n");
        return lres;
    }
    
    memcpy(magicbuf, headinfo.magic, FSA_SIZEOF_MAGIC);
    *d=headinfo.dico;
    if (fsid!=NULL) *fsid=headinfo.fsid;
    
    return lres;
}

s64 queue_dequeue_header_internal(cqueue *q, cheadinfo *headinfo)
{
    cqueueitem *cur;
    s64 itemnum;
    
    if (!q || !headinfo)
    {   errprintf("a parameter is null\n");
        return FSAERR_EINVAL;
    }
    
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    // while ((first-item-of-the-queue-is-not-ready) && (not-at-the-end-of-the-queue))
    while ( (((cur=q->head)==NULL) || (cur->status!=QITEM_STATUS_DONE)) && (queuelocked_get_end_of_queue(q)==false) )
    {
        struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
    }
    
    // if it failed at the other end of the queue
    if (queuelocked_get_end_of_queue(q))
    {   assert(pthread_mutex_unlock(&q->mutex)==0);
        return FSAERR_ENDOFFILE;
    }
    
    cur=q->head;
    assert (cur!=NULL); // queuelocked_is_first_block_ready means there is at least one block in the queue
    
    // test the first item
    switch (cur->type)
    {
        case QITEM_TYPE_HEADER:
            q->head=cur->next;
            *headinfo=cur->headinfo;
            itemnum=cur->itemnum;
            free(cur);
            q->itemcount--;
            assert(pthread_mutex_unlock(&q->mutex)==0);
            pthread_cond_broadcast(&q->cond);
            return itemnum;
        case QITEM_TYPE_BLOCK:
            errprintf("dequeue - wrong type of data in the queue: expected a dico and found a block\n");
            assert(pthread_mutex_unlock(&q->mutex)==0);
            pthread_cond_broadcast(&q->cond);
            return FSAERR_WRONGTYPE;  // ok but not found
        default: // should never happen
            errprintf("dequeue - wrong type of data in the queue: expected a dico and found an unknown item\n");
            assert(pthread_mutex_unlock(&q->mutex)==0);
            pthread_cond_broadcast(&q->cond);
            return FSAERR_WRONGTYPE;  // ok but not found
    }
}

// returns true if the first item in queue is a dico or a block which is ready
bool queuelocked_is_first_item_ready(cqueue *q)
{
    cqueueitem *cur;
    
    if (!q)
    {   errprintf("a parameter is null\n");
        return false; // not found
    }
    
    if ((cur=q->head)==NULL)
        return false; // list empty
    else if (cur->type==QITEM_TYPE_HEADER)
        return true; // a dico is always ready
    else if ((cur->type==QITEM_TYPE_BLOCK) && (cur->status==QITEM_STATUS_DONE))
        return true; // a block which has been prepared is ready
    else // other cases: block not yet prepared
        return false;
}

// say what the next item which is ready in the queue is but do not remove it
s64 queue_check_next_item(cqueue *q, int *type, char *magic)
{
    cqueueitem *cur;
    
    if (!q || !type || !magic)
    {   errprintf("a parameter is null\n");
        return FSAERR_EINVAL;
    }
    
    memset(magic, 0, FSA_SIZEOF_MAGIC);
    *type=0;
    
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    // while ((first-item-of-the-queue-is-not-ready) && (not-at-the-end-of-the-queue))
    while ( (((cur=q->head)==NULL) || (cur->status!=QITEM_STATUS_DONE)) && (queuelocked_get_end_of_queue(q)==false) )
    {
        struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
    }
    
    // if it failed at the other end of the queue
    if (queuelocked_get_end_of_queue(q))
    {   assert(pthread_mutex_unlock(&q->mutex)==0);
        return FSAERR_ENDOFFILE;
    }
    
    // test the first item
    if (((cur=q->head)!=NULL) && (cur->status==QITEM_STATUS_DONE))
    {
        if (cur->type==QITEM_TYPE_BLOCK) // item to dequeue is a block
        {
            *type=cur->type;
            memset(magic, 0, FSA_SIZEOF_MAGIC);
            assert(pthread_mutex_unlock(&q->mutex)==0);
            return FSAERR_SUCCESS;
        }
        else if (cur->type==QITEM_TYPE_HEADER) // item to dequeue is a dico
        {
            memcpy(magic, cur->headinfo.magic, FSA_SIZEOF_MAGIC); // header contents
            *type=cur->type;
            assert(pthread_mutex_unlock(&q->mutex)==0);
            return FSAERR_SUCCESS;
        }
        else
        {
            errprintf("invalid item type in queue: type=%d\n", cur->type);
            assert(pthread_mutex_unlock(&q->mutex)==0);
            return FSAERR_EINVAL;
        }
    }
    
    assert(pthread_mutex_unlock(&q->mutex)==0);
    pthread_cond_broadcast(&q->cond);
    
    return FSAERR_ENOENT;  // not found
}

// destroy the first item in the queue (similar to dequeue but do not read it)
s64 queue_destroy_first_item(cqueue *q)
{
    cqueueitem *cur;
    
    if (!q)
    {   errprintf("a parameter is null\n");
        return FSAERR_EINVAL;
    }
    
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    // while ((first-item-of-the-queue-is-not-ready or first-item-is-being-processed-by-comp-thread) && (not-at-the-end-of-the-queue))
    while ( (((cur=q->head)==NULL) || (cur->status==QITEM_STATUS_PROGRESS)) && (queuelocked_get_end_of_queue(q)==false) )
    {   struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
    }
    
    // if it failed at the other end of the queue
    if (queuelocked_get_end_of_queue(q))
    {   assert(pthread_mutex_unlock(&q->mutex)==0);
        return FSAERR_ENDOFFILE;
    }
    
    cur=q->head;
    assert(cur!=NULL); // queuelocked_is_first_block_ready means there is at least one block in the queue
    
    switch (cur->type)
    {
        case QITEM_TYPE_BLOCK:
            q->blkcount--;
            free(cur->blkinfo.blkdata);
            break;
        case QITEM_TYPE_HEADER:
            dico_destroy(cur->headinfo.dico);
            break;
    }
    
    q->head=cur->next;
    free(cur);
    q->itemcount--;
    assert(pthread_mutex_unlock(&q->mutex)==0);
    pthread_cond_broadcast(&q->cond);
    return FSAERR_SUCCESS;
}
