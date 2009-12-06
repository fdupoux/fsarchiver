/*
 * fsarchiver: Filesystem Archiver
 *
 * Copyright (C) 2008-2009 Francois Dupoux.  All rights reserved.
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

#include "fsarchiver.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#include "queue.h"
#include "common.h"
#include "syncthread.h"

#define printwait()                                                                \
{                                                                       \
    msgprintf(MSG_DEBUG4, "WAIT: %s() and qlen=%ld\n", __FUNCTION__, (long)q->itemcount);                            \
}

#define fct_in()                                                                \
{                                                                       \
    msgprintf(MSG_DEBUG5, "[thr=%.2ld] FCT_IN: %s() and qlen=%ld\n", (long)(pthread_self()%100), __FUNCTION__, (long)q->itemcount);        \
}
#define fct_out()                                                                \
{                                                                       \
    msgprintf(MSG_DEBUG5, "[thr=%.2ld] FCT_OUT: %s() and qlen=%ld\n", (long)(pthread_self()%100), __FUNCTION__, (long)q->itemcount);    \
}

char *qerr(s64 err)
{
    switch (err)
    {
        case QERR_SUCCESS: return "QERR_SUCCESS";
        case QERR_FAIL: return "QERR_FAIL";
        case QERR_NOMEM: return "QERR_NOMEM";
        case QERR_INVAL: return "QERR_INVAL";
        case QERR_NOTFOUND: return "QERR_NOTFOUND";
        case QERR_ENDOFQUEUE: return "QERR_ENDOFQUEUE";
        case QERR_WRONGTYPE: return "QERR_WRONGTYPE";
        case QERR_CLOSED: return "QERR_CLOSED";
        default: return "QERR_UNKNOWN";
    }
}

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
        return QERR_INVAL;
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
        return QERR_FAIL;
    }
    
    if (pthread_cond_init(&q->cond,NULL)!=0)
    {   msgprintf(3, "pthread_cond_init failed\n");
        return QERR_FAIL;
    }
    
    return QERR_SUCCESS;
}

s64 queue_destroy(cqueue *q)
{
    struct s_queueitem *cur;
    struct s_queueitem *next;
    
    if (!q)
    {   errprintf("q is NULL\n");
        return QERR_INVAL;
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
    
    return QERR_SUCCESS;
}

s64 queue_set_end_of_queue(cqueue *q, bool state)
{
    if (!q)
    {   errprintf("q is NULL\n");
        return QERR_INVAL;
    }

    fct_in();
    assert(pthread_mutex_lock(&q->mutex)==0);
    q->endofqueue=state;
    assert(pthread_mutex_unlock(&q->mutex)==0);
    pthread_cond_broadcast(&q->cond);
    fct_out();
    return QERR_SUCCESS;
}

bool queue_get_end_of_queue(cqueue *q)
{
    bool res;
    if (!q)
    {   errprintf("q is NULL\n");
        return QERR_INVAL;
    }

    fct_in();
    assert(pthread_mutex_lock(&q->mutex)==0);
    res=((q->itemcount<1) && (q->endofqueue==true));
    assert(pthread_mutex_unlock(&q->mutex)==0);
    fct_out();
    return res;
}

bool queuelocked_get_end_of_queue(cqueue *q)
{
    bool res;
    if (!q)
    {   errprintf("q is NULL\n");
        return QERR_INVAL;
    }

    fct_in();
    res=((q->itemcount<1) && (q->endofqueue==true));
    fct_out();
    return res;
}

// runs with the mutex unlocked (external users)
s64 queue_count(cqueue *q)
{
    s64 itemcount;
    
    if (!q)
    {   errprintf("q is NULL\n");
        return QERR_INVAL;
    }

    fct_in();
    assert(pthread_mutex_lock(&q->mutex)==0);
    itemcount = q->itemcount;
    assert(pthread_mutex_unlock(&q->mutex)==0);
    fct_out();
    
    return itemcount;
}

// how many items in the queue have a particular status
s64 queue_count_status(cqueue *q, int status)
{
    struct s_queueitem *cur;
    int count=0;
    
    if (!q)
    {   errprintf("q is NULL\n");
        return QERR_INVAL;
    }

    fct_in();
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    for (cur=q->head; cur!=NULL; cur=cur->next)
    {
        if (status==QITEM_STATUS_NULL || cur->status==status)
            count++;
    }
    
    assert(pthread_mutex_unlock(&q->mutex)==0);
    fct_out();
    
    return count;
}

// add a block at the end of the queue
s64 queue_add_block(cqueue *q, struct s_blockinfo *blkinfo, int status)
{
    struct s_queueitem *item;
    struct s_queueitem *cur;
    
    if (!q || !blkinfo)
    {   errprintf("a parameter is NULL\n");
        return QERR_INVAL;
    }
    
    // create the new item in memory
    item=malloc(sizeof(struct s_queueitem));
    if (!item)
    {   errprintf("malloc(%ld) failed: out of memory\n", (long)sizeof(struct s_queueitem));
        return QERR_NOMEM;
    }
    item->type=QITEM_TYPE_BLOCK;
    item->status=status;
    item->blkinfo=*blkinfo;
    item->next=NULL;
    
    fct_in();
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    // does not make sense to add item on a queue where endofqueue is true
    if (q->endofqueue==true)
    {   assert(pthread_mutex_unlock(&q->mutex)==0);
        fct_out();
        return QERR_ENDOFQUEUE;
    }
    
    // wait while (queue-is-full) to let the other threads remove items first
    while (q->blkcount > q->blkmax)
    {
        struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
        printwait();
    }
    
    if (q->head==NULL) // if list empty: item is head
    {
        q->head=item;
    }
    else // list not empty: add items at the end
    {   for (cur=q->head; (cur!=NULL) && (cur->next!=NULL); cur=cur->next);
        cur->next=item;
    }
    
    q->blkcount++;
    q->itemcount++;
    item->itemnum=q->curitemnum++;
    
    assert(pthread_mutex_unlock(&q->mutex)==0);
    pthread_cond_broadcast(&q->cond);
    fct_out();
    
    return QERR_SUCCESS;
}

s64 queue_add_header(cqueue *q, cdico *d, char *magic, u16 fsid)
{
    struct s_headinfo headinfo;
    
    if (!q || !d || !magic)
    {   errprintf("parameter is null\n");
        return QERR_INVAL;
    }
    
    memset(&headinfo, 0, sizeof(headinfo));
    memcpy(headinfo.magic, magic, FSA_SIZEOF_MAGIC);
    headinfo.fsid=fsid;
    headinfo.dico=d;
    
    return     queue_add_header_internal(q, &headinfo);
}

s64 queue_add_header_internal(cqueue *q, struct s_headinfo *headinfo)
{
    struct s_queueitem *item;
    struct s_queueitem *cur;
    
    if (!q || !headinfo)
    {   errprintf("parameter is null\n");
        return QERR_INVAL;
    }
    
    // create the new item in memory
    item=malloc(sizeof(struct s_queueitem));
    if (!item)
    {   errprintf("malloc(%ld) failed: out of memory 1\n", (long)sizeof(struct s_queueitem));
        return QERR_NOMEM;
    }
    
    item->headinfo=*headinfo;
    item->type=QITEM_TYPE_HEADER;
    item->status=QITEM_STATUS_DONE;
    item->next=NULL;
    
    fct_in();
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    // does not make sense to add item on a queue where endofqueue is true
    if (q->endofqueue==true)
    {   assert(pthread_mutex_unlock(&q->mutex)==0);
        fct_out();
        return QERR_ENDOFQUEUE;
    }
    
    // wait while (queue-is-full) to let the other threads remove items first
    while (q->blkcount > q->blkmax)
    {
        struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
        printwait();
    }
    
    item->itemnum=q->curitemnum++;
    if (q->head==NULL) // if list empty
    {
        q->head=item;
    }
    else // list not empty
    {   for (cur=q->head; (cur!=NULL) && (cur->next!=NULL); cur=cur->next);
        cur->next=item;
    }
    
    q->itemcount++;
    assert(pthread_mutex_unlock(&q->mutex)==0);
    pthread_cond_broadcast(&q->cond);
    fct_out();
    
    return QERR_SUCCESS;
}

// function called by the compression thread when a block has been compressed
s64 queue_replace_block(cqueue *q, s64 itemnum, struct s_blockinfo *blkinfo, int newstatus)
{
    struct s_queueitem *cur;
    
    if (!q || !blkinfo)
    {   errprintf("a parameter is null\n");
        return QERR_INVAL;
    }
    
    fct_in();
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    if (q->head==NULL)
    {   assert(pthread_mutex_unlock(&q->mutex)==0);
        msgprintf(MSG_DEBUG1, "q->head is NULL: list is empty\n");
        fct_out();
        return QERR_NOTFOUND; // item not found
    }
    
    for (cur=q->head; cur!=NULL; cur=cur->next)
    {
        if (cur->itemnum==itemnum) // block found
        {
            cur->status=newstatus;
            cur->blkinfo=*blkinfo;
            assert(pthread_mutex_unlock(&q->mutex)==0);
            pthread_cond_broadcast(&q->cond);
            fct_out();
            return QERR_SUCCESS;
        }
    }
    
    assert(pthread_mutex_unlock(&q->mutex)==0);
    fct_out();
    return QERR_NOTFOUND; // not found
}

// get number of items to be processed
s64 queue_count_items_todo(cqueue *q)
{
    struct s_queueitem *cur;
    s64 count=0;
    
    if (!q)
    {   errprintf("a parameter is null\n");
        return QERR_INVAL;
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
s64 queue_get_first_block_todo(cqueue *q, struct s_blockinfo *blkinfo)
{
    struct s_queueitem *cur;
    s64 itemfound=-1;
    int res;
    
    if (!q || !blkinfo)
    {   errprintf("a parameter is null\n");
        return QERR_INVAL;
    }
    
    fct_in();
    
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
                fct_out();
                return itemfound; // ">0" means item found
            }
        }
        
        struct timespec t=get_timeout();
        if ((res=pthread_cond_timedwait(&q->cond, &q->mutex, &t))!=0 && res!=ETIMEDOUT)
        {   assert(pthread_mutex_unlock(&q->mutex)==0);
            fct_out();
            return QERR_FAIL;
        }
        
        printwait();
    }
    
    // if it failed at the other end of the queue
    assert(pthread_mutex_unlock(&q->mutex)==0);
    fct_out();
    
    if (queuelocked_get_end_of_queue(q))
        return QERR_ENDOFQUEUE;
    else
        return QERR_FAIL;
}

// the writer thread requires the first block of the queue if it ready to go
//s64 queue_dequeue_first(cqueue *q, int *type, char *magic, cdico **dico, struct s_blockinfo *blkinfo)
s64 queue_dequeue_first(cqueue *q, int *type, struct s_headinfo *headinfo, struct s_blockinfo *blkinfo)
{
    struct s_queueitem *cur=NULL;
    s64 itemfound=-1;
    int ret;
    
    if (!q || !headinfo || !blkinfo)
    {   errprintf("a parameter is null\n");
        return QERR_INVAL;
    }
    
    fct_in();
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
                fct_out();
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
                fct_out();
                return itemfound; // ">0" means item found
            }
            else
            {
                errprintf("invalid item type in queue\n");
                assert(pthread_mutex_unlock(&q->mutex)==0);
                fct_out();
                return QERR_INVAL;
            }
        }
        
        struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
        printwait();
    }
    
    // if it failed at the other end of the queue
    ret=(queuelocked_get_end_of_queue(q)==true)?QERR_ENDOFQUEUE:QERR_FAIL;
    assert(pthread_mutex_unlock(&q->mutex)==0);
    fct_out();
    
    return ret;
}

// the extract function wants to read headers from the queue
s64 queue_dequeue_block(cqueue *q, struct s_blockinfo *blkinfo)
{
    struct s_queueitem *cur;
    s64 itemnum;
    
    if (!q || !blkinfo)
    {   errprintf("a parameter is null\n");
        return QERR_INVAL;
    }
    
    fct_in();
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    // while ((first-item-of-the-queue-is-not-ready) && (not-at-the-end-of-the-queue))
    while ( (((cur=q->head)==NULL) || (cur->status!=QITEM_STATUS_DONE)) && (queuelocked_get_end_of_queue(q)==false) )
    {
        struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
        printwait();
    }
    
    // if it failed at the other end of the queue
    if (queuelocked_get_end_of_queue(q))
    {   assert(pthread_mutex_unlock(&q->mutex)==0);
        fct_out();
        return QERR_ENDOFQUEUE;
    }
    
    // should not happen since queuelocked_is_first_block_ready means there is at least one block in the queue
    assert((cur=q->head)!=NULL);
    
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
        fct_out();
        return itemnum;
    }
    else
    {
        errprintf("dequeue - wrong type of data in the queue: wanted a block, found an header\n");
        assert(pthread_mutex_unlock(&q->mutex)==0);
        pthread_cond_broadcast(&q->cond);
        fct_out();
        return QERR_WRONGTYPE;  // ok but not found
    }
}

s64 queue_dequeue_header(cqueue *q, cdico **d, char *magicbuf, u16 *fsid)
{
    struct s_headinfo headinfo;
    s64 lres;
    
    if (!q || !d || !magicbuf)
    {   errprintf("a parameter is null\n");
        return QERR_INVAL;
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

s64 queue_dequeue_header_internal(cqueue *q, struct s_headinfo *headinfo)
{
    struct s_queueitem *cur;
    s64 itemnum;
    
    if (!q || !headinfo)
    {   errprintf("a parameter is null\n");
        return QERR_INVAL;
    }
    
    fct_in();
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    // while ((first-item-of-the-queue-is-not-ready) && (not-at-the-end-of-the-queue))
    while ( (((cur=q->head)==NULL) || (cur->status!=QITEM_STATUS_DONE)) && (queuelocked_get_end_of_queue(q)==false) )
    {
        struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
        printwait();
    }
    
    // if it failed at the other end of the queue
    if (queuelocked_get_end_of_queue(q))
    {   assert(pthread_mutex_unlock(&q->mutex)==0);
        fct_out();
        return QERR_ENDOFQUEUE;
    }
    
    // should not happen since queuelocked_is_first_block_ready means there is at least one block in the queue
    assert ((cur=q->head)!=NULL);
    
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
            fct_out();
            return itemnum;
        case QITEM_TYPE_BLOCK:
            errprintf("dequeue - wrong type of data in the queue: expected a dico and found a block\n");
            assert(pthread_mutex_unlock(&q->mutex)==0);
            pthread_cond_broadcast(&q->cond);
            fct_out();
            return QERR_WRONGTYPE;  // ok but not found
        default: // should never happen
            errprintf("dequeue - wrong type of data in the queue: expected a dico and found an unknown item\n");
            assert(pthread_mutex_unlock(&q->mutex)==0);
            pthread_cond_broadcast(&q->cond);
            fct_out();
            return QERR_WRONGTYPE;  // ok but not found
    }
}

// returns true if the first item in queue is a dico or a block which is ready
bool queuelocked_is_first_item_ready(cqueue *q)
{
    struct s_queueitem *cur;
    
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
    struct s_queueitem *cur;
    
    if (!q || !type || !magic)
    {   errprintf("a parameter is null\n");
        return QERR_INVAL;
    }
    
    memset(magic, 0, FSA_SIZEOF_MAGIC);
    *type=0;
    
    fct_in();
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    // while ((first-item-of-the-queue-is-not-ready) && (not-at-the-end-of-the-queue))
    while ( (((cur=q->head)==NULL) || (cur->status!=QITEM_STATUS_DONE)) && (queuelocked_get_end_of_queue(q)==false) )
    {
        struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
        printwait();
    }
    
    // if it failed at the other end of the queue
    if (queuelocked_get_end_of_queue(q))
    {   assert(pthread_mutex_unlock(&q->mutex)==0);
        fct_out();
        return QERR_ENDOFQUEUE;
    }
    
    // test the first item
    if (((cur=q->head)!=NULL) && (cur->status==QITEM_STATUS_DONE))
    {
        if (cur->type==QITEM_TYPE_BLOCK) // item to dequeue is a block
        {
            *type=cur->type;
            memset(magic, 0, FSA_SIZEOF_MAGIC);
            assert(pthread_mutex_unlock(&q->mutex)==0);
            fct_out();
            return QERR_SUCCESS;
        }
        else if (cur->type==QITEM_TYPE_HEADER) // item to dequeue is a dico
        {
            memcpy(magic, cur->headinfo.magic, FSA_SIZEOF_MAGIC); // header contents
            *type=cur->type;
            assert(pthread_mutex_unlock(&q->mutex)==0);
            fct_out();
            return QERR_SUCCESS;
        }
        else
        {
            errprintf("invalid item type in queue: type=%d\n", cur->type);
            assert(pthread_mutex_unlock(&q->mutex)==0);
            fct_out();
            return QERR_INVAL;
        }
    }
    
    assert(pthread_mutex_unlock(&q->mutex)==0);
    pthread_cond_broadcast(&q->cond);
    fct_out();
    
    return QERR_NOTFOUND;  // not found
}

// destroy the first item in the queue (similar to dequeue but do not read it)
s64 queue_destroy_first_item(cqueue *q)
{
    struct s_queueitem *cur;
    
    if (!q)
    {   errprintf("a parameter is null\n");
        return QERR_INVAL;
    }
    
    fct_in();
    assert(pthread_mutex_lock(&q->mutex)==0);
    
    // while ((first-item-of-the-queue-is-not-ready or first-item-is-being-processed-by-comp-thread) && (not-at-the-end-of-the-queue))
    while ( (((cur=q->head)==NULL) || (cur->status==QITEM_STATUS_PROGRESS)) && (queuelocked_get_end_of_queue(q)==false) )
    {   struct timespec t=get_timeout();
        pthread_cond_timedwait(&q->cond, &q->mutex, &t);
        printwait();
    }
    
    // if it failed at the other end of the queue
    if (queuelocked_get_end_of_queue(q))
    {   assert(pthread_mutex_unlock(&q->mutex)==0);
        fct_out();
        return QERR_ENDOFQUEUE;
    }
    
    // should not happen since queuelocked_is_first_block_ready means there is at least one block in the queue
    assert((cur=q->head)!=NULL);
    
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
    fct_out();
    return QERR_SUCCESS;
}
