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

#include "fsarchiver.h"
#include "strlist.h"
#include "common.h"
#include "error.h"

int strlist_init(cstrlist *l)
{
    if (l==NULL)
        return -1;
    l->head=NULL;
    return 0;
}

int strlist_destroy(cstrlist *l)
{
    if (l==NULL)
        return -1;
    
    strlist_empty(l);
    
    return 0;
}

int strlist_empty(cstrlist *l)
{
    cstrlistitem *item, *next;
    
    if (l==NULL)
        return -1;
    
    for (item=l->head; item!=NULL; item=next)
    {   next=item->next;
        free(item->str);
        free(item);
    }
    l->head=NULL;
    
    return 0;
}

int strlist_add(cstrlist *l, char *str)
{
    cstrlistitem *item, *lnew;
    int len;
    
    if (!l || !str || !strlen(str))
    {   errprintf("invalid param\n");
        return -1;
    }
    
    if (strlist_exists(l, str)==true)
    {   errprintf("canot add dring: [%s] is already in the list\n", str);
        return -1;
    }
    
    if ((lnew=malloc(sizeof(cstrlistitem)))==NULL)
    {   errprintf("malloc() failed\n");
        return -1;
    }
    memset(lnew, 0, sizeof(cstrlistitem));
    lnew->next=NULL;    
    
    len=strlen(str);
    if ((lnew->str=malloc(len+1))==NULL)
    {   errprintf("malloc() failed\n");
        free(lnew);
        return -1;
    }
    memcpy(lnew->str, str, len+1);
    
    // go to the end of the item and check for duplicates
    if (l->head==NULL) // item is empty
    {   
        l->head=lnew;
    }
    else // item is not empty
    {
        for (item=l->head; (item!=NULL) && (item->next!=NULL); item=item->next);
        item->next=lnew;
    }
    
    return 0;
}

int strlist_getitem(cstrlist *l, int index, char *buf, int bufsize)
{
    cstrlistitem *item;
    int pos=0;

    if (!l || !buf || bufsize<=0)
    {   errprintf("invalid param\n");
        return -1;
    }
    
    for (item=l->head; (item!=NULL) && (pos++ < index); item=item->next);
    
    if (item!=NULL)
    {
        snprintf(buf, bufsize, "%s", item->str);
        return 0;
    }
    else
    {
        return -1;
    }
}

int strlist_remove(cstrlist *l, char *str)
{
    cstrlistitem *item, *next;
    
    if (!l || !str)
    {   errprintf("invalid param\n");
        return -1;
    }
    
    if (!l->head)
        return -1; // not found

    // if item to remove found in first pos
    item=l->head;
    if (strcmp(item->str, str)==0)
    {   free(item->str);
        l->head=item->next;
        free(item);
        return 0; // item removed
    }
    
    // item to remove not in first pos
    for (item=l->head; (item!=NULL) && (item->next!=NULL); item=item->next)
    {   
        next=item->next;
        if (strcmp(next->str, str)==0)
        {   free(next->str);
            item->next=next->next;
            free(next);
            return 0; // item removed
        }
    }
    
    return -1; // not found
}

char *strlist_merge(cstrlist *l, char *bufdat, int bufsize, char sep)
{
    cstrlistitem *item;
    
    if (!l || !bufdat || bufsize<=0)
    {   errprintf("invalid param\n");
        return NULL;
    }
    
    // init
    memset(bufdat, 0, bufsize);
    
    // item to remove not in first pos
    for (item=l->head; item!=NULL; item=item->next)
    {   
        if (item!=l->head) // not the first item: write separator
            strlcatf(bufdat, bufsize, "%c", sep);
        strlcatf(bufdat, bufsize, "%s", item->str);
    }
    
    return bufdat;
}

int strlist_exists(cstrlist *l, char *str)
{
    cstrlistitem *item;
    
    if (!l || !str)
    {   errprintf("invalid param\n");
        return -1; // error
    }
    
    if (!l->head)
        return false; // not found
    
    // item to remove not in first pos
    for (item=l->head; item!=NULL; item=item->next)
        if (strcmp(item->str, str)==0)
            return true; // item found
    
    return false; // not found
}

int strlist_split(cstrlist *l, char *text, char sep)
{
    char *textcopy;
    char delims[4];
    char *saveptr;
    char *result;
    int len;

    if (!l || !text)
    {   errprintf("invalid param\n");
        return -1;
    }
    
    // init
    len=strlen(text);
    snprintf(delims, sizeof(delims), "%c", sep);
    strlist_empty(l);
    
    if ((textcopy=malloc(len+1))==NULL)
    {   errprintf("malloc(%d) failed\n", len+1);
        return -1;
    }

    memcpy(textcopy, text, len+1);
    for (result=strtok_r(textcopy, delims, &saveptr); result!=NULL; result=strtok_r(NULL, delims, &saveptr))
    {
        if (strlist_add(l, result)!=0)
        {   errprintf("strlist_add(l, [%s]) failed\n", result);
            free(textcopy);
            return -1;
        }
    }
    
    free(textcopy);
    return 0;
}

int strlist_count(cstrlist *l)
{
    cstrlistitem *item;
    int count=0;
    
    if (!l)
    {   errprintf("invalid param\n");
        return -1; // error
    }
    
    if (!l->head)
    {   
        return 0;
    }
    else
    {
        for (item=l->head; (item!=NULL); item=item->next)
            count++;
    }
    
    return count;
}

int strlist_show(cstrlist *l)
{
    cstrlistitem *item;
    int count=0;
    
    if (!l)
    {   errprintf("invalid param\n");
        return -1; // error
    }
    
    if (!l->head)
    {   
        printf("list is empty");
    }
    else
    {
        for (item=l->head; (item!=NULL); item=item->next)
            printf("item[%d]: [%s]\n", count++, item->str);
    }
    
    return 0;
}
