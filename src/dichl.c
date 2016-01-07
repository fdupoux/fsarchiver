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

#include "fsarchiver.h"
#include "dichl.h"
#include "common.h"
#include "error.h"

cdichl *dichl_alloc()
{
    cdichl *d;
    if ((d=malloc(sizeof(cdichl)))==NULL)
        return NULL;
    d->head=NULL;
    return d;
}

int dichl_destroy(cdichl *d)
{
    cdichlitem *item, *next;
    
    if (d==NULL)
        return -1;
    
    item=d->head;
    while (item!=NULL)
    {
        next=item->next;
        free(item->str);
        free(item);
        item=next;
    }
    
    free(d);
    
    return 0;
}

int dichl_add(cdichl *d, u64 key1, u64 key2, char *str)
{
    cdichlitem *item, *lnew, *last;
    int len;
    
    if (d==NULL || !str)
    {   errprintf("invalid parameters\n");
        return -1;
    }
    len=strlen(str);
    
    // allocate object
    lnew=malloc(sizeof(cdichlitem));
    if (!lnew)
    {   errprintf("malloc(%ld) failed: out of memory\n", (long)sizeof(cdichlitem));
        return -1;
    }
    memset(lnew, 0, sizeof(cdichlitem));
    lnew->str=malloc(len+1);
    if (!lnew->str)
    {   
        free(lnew);
        errprintf("malloc(%ld) failed: out of memory\n", (long)len+1);
        return -1;
    }
    
    memcpy(lnew->str, str, len+1);
    lnew->key1=key1;
    lnew->key2=key2;
    lnew->next=NULL;
    
    // go to the end of the item and check for duplicates
    if (d->head==NULL) // item is empty
    {   
        d->head=lnew;
    }
    else // item is not empty
    {   
        for (item=d->head; item!=NULL; item=item->next)
        {   
            last=item;
            if (item->key1==key1 && item->key2==key2)
            {   errprintf("dichl_add_internal(): item with key1=%ld and key2=%ld is already in dico\n", (long)item->key1, (long)item->key2);
                return -1;
            }
        }
        last->next=lnew;
    }
    
    return 0;
}

int dichl_get(cdichl *d, u64 key1, u64 key2, char *buf, int bufsize)
{
    cdichlitem *item;
    int len;
    
    if (d==NULL || !buf)
    {   errprintf("invalid dichl\n");
        return -1;
    }
    
    for (item=d->head; item!=NULL; item=item->next)
    {   
        if ((item!=NULL) && (item->key1==key1) && (item->key2==key2))
        {
            len=strlen(item->str);
            if (bufsize<len+1)
                return -2;
            snprintf(buf, bufsize, "%s", item->str);
            return 0;
        }
    }
    
    return -3; // not found
}
