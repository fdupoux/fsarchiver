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
#include <stdio.h>
#include <assert.h>

#include "fsarchiver.h"
#include "dico.h"
#include "common.h"
#include "error.h"

cdico *dico_alloc()
{
    cdico *d;
    if ((d=malloc(sizeof(cdico)))==NULL)
        return NULL;
    d->head=NULL;
    return d;
}

int dico_destroy(cdico *d)
{
    cdicoitem *item, *next;
    
    if (d==NULL)
        return -1;
    
    item=d->head;
    while (item!=NULL)
    {
        next=item->next;
        if (item->data!=NULL)
            free(item->data);
        free(item);
        item=next;
    }
    
    free(d);
    
    return 0;
}

int dico_add_data(cdico *d, u8 section, u16 key, const void *data, u16 size)
{
    return dico_add_generic(d, section, key, data, size, DICTYPE_DATA);
}

// add an item to the dico, fails if an item with that (section,key) already exists
int dico_add_generic(cdico *d, u8 section, u16 key, const void *data, u16 size, u8 type)
{
    cdicoitem *item, *lnew, *last;
    
    assert (d);
    
    // allocate object
    lnew=malloc(sizeof(cdicoitem));
    if (!lnew)
    {   errprintf("malloc(%ld) failed: out of memory\n", (long)sizeof(cdicoitem));
        return -3;
    }
    memset(lnew, 0, sizeof(cdicoitem));
    
    // go to the end of the item and check for duplicates
    if (d->head==NULL) // item is empty
    {   
        d->head=lnew;
    }
    else // item is not empty
    {   
        for (item=d->head; item!=NULL; item=item->next)
        {   last=item;
            if (item->section==section && item->key==key)
            {   errprintf("dico_add_generic(): item with key=%ld is already in dico\n", (long)item->key);
                return -3;
            }
        }
        last->next=lnew;
    }
    
    // copy key
    lnew->key=key;
    lnew->section=section;
    lnew->size=size;
    lnew->type=type;
    lnew->data=NULL;
    
    // allocate memory for data
    if (size > 0)
    {
        lnew->data=malloc(size);
        if (!lnew->data)
        {   errprintf("malloc(%ld) failed: out of memory\n", (long)size);
            return -3;
        }
        
        // copy data
        memcpy(lnew->data, data, size);
    }
    
    return 0;
}

int dico_get_data(cdico *d, u8 section, u16 key, void *data, u16 maxsize, u16 *size)
{
    return dico_get_generic(d, section, key, data, maxsize, size);
}

int dico_get_generic(cdico *d, u8 section, u16 key, void *data, u16 maxsize, u16 *size)
{
    cdicoitem *item;
    
    assert(d);
    assert(data);
    
    // size can be NULL if the user does not want to know the size
    if (size!=NULL)
        *size=0;
    
    if (d->head==NULL)
    {   msgprintf(MSG_DEBUG1, "dico is empty\n");
        return -1;
    }
    
    if (maxsize<1)
    {   msgprintf(MSG_DEBUG1, "case1: maxsize=%d\n", maxsize);
        return -3;
    }
    
    for (item=d->head; item!=NULL; item=item->next)
    {
        if ((item!=NULL) && (item->key==key && item->section==section))
        {
            if (item->size > maxsize) // item is too big
            {   msgprintf(MSG_DEBUG1, "case2: (item->size > maxsize): item->size =%d, maxsize=%d\n", item->size, maxsize);
                return -4;
            }
            if ((item->size>0) && (item->data!=NULL)) // there may be no data (size==0)
                memcpy(data, item->data, item->size);
            if (size!=NULL)
                *size=item->size;
            return 0;
        }
    }
    
    msgprintf(MSG_DEBUG1, "case3: not found\n");
    return -5; // not found
}

int dico_count_one_section(cdico *d, u8 section)
{
    cdicoitem *item;
    int count;
    
    assert(d);
    
    count=0;
    for (item=d->head; item!=NULL; item=item->next)
        if (item->section==section)
            count++;
    
    return count;
}

int dico_count_all_sections(cdico *d)
{
    cdicoitem *item;
    int count;
    
    assert(d);
    
    count=0;
    for (item=d->head; item!=NULL; item=item->next)
        count++;
    
    return count;
}

int dico_add_u16(cdico *d, u8 section, u16 key, u16 data)
{
    u16 ledata;
    assert(d);
    ledata=cpu_to_le16(data);
    return dico_add_generic(d, section, key, &ledata, sizeof(ledata), DICTYPE_U16);
}

int dico_add_u32(cdico *d, u8 section, u16 key, u32 data)
{
    u32 ledata;
    assert(d);
    ledata=cpu_to_le32(data);
    return dico_add_generic(d, section, key, &ledata, sizeof(ledata), DICTYPE_U32);
}

int dico_add_u64(cdico *d, u8 section, u16 key, u64 data)
{
    u64 ledata;
    assert (d);
    ledata=cpu_to_le64(data);
    return dico_add_generic(d, section, key, &ledata, sizeof(ledata), DICTYPE_U64);
}

int dico_add_string(cdico *d, u8 section, u16 key, const char *szstring)
{
    u16 len;
    assert(d);
    assert(szstring);
    len=strlen(szstring);
    return dico_add_generic(d, section, key, szstring, len+1, DICTYPE_STRING);
}

int dico_get_u16(cdico *d, u8 section, u16 key, u16 *data)
{
    u16 ledata;
    u16 size;
    
    assert(d);
    assert(data);
    
    *data=0;
    if (dico_get_data(d, section, key, &ledata, sizeof(ledata), &size)!=0)
        return -1;
    *data=le16_to_cpu(ledata);
    return 0;
}

int dico_get_u32(cdico *d, u8 section, u16 key, u32 *data)
{
    u32 ledata;
    u16 size;
    
    assert(d);
    assert(data);
    
    *data=0;
    if (dico_get_data(d, section, key, &ledata, sizeof(ledata), &size)!=0)
        return -1;
    *data=le32_to_cpu(ledata);
    return 0;
}

int dico_get_u64(cdico *d, u8 section, u16 key, u64 *data)
{
    u64 ledata;
    u16 size;
    
    assert(d);
    assert(data);
    
    *data=0;
    if (dico_get_data(d, section, key, &ledata, sizeof(ledata), &size)!=0)
        return -1;
    *data=le64_to_cpu(ledata);
    return 0;
}

int dico_get_string(cdico *d, u8 section, u16 key, char *buffer, u16 bufsize)
{
    u16 size;
    
    assert(d);
    assert(buffer);
    
    memset(buffer, 0, bufsize);
    return dico_get_data(d, section, key, buffer, bufsize, &size);
}

int dico_show(cdico *d, u8 section, char *debugtxt)
{
    char buffer[2048];
    char text[2048];
    cdicoitem *item;
    
    assert(d);
    msgprintf(MSG_FORCE, "\n-----------------debug-dico-begin(%s)---------------\n", debugtxt);
    
    if (d->head)
    {
        for (item=d->head; item!=NULL; item=item->next)
        {
            if (item->section==section)
            {
                snprintf(buffer, sizeof(buffer), "key=[%ld], sizeof(data)=[%d], ", (long)item->key, (int)item->size);
                
                switch (item->type)
                {
                    case DICTYPE_U8:
                        snprintf(text, sizeof(text), "type=u8, size=[%d]", (int)item->size);
                        break;
                    case DICTYPE_U16:
                        snprintf(text, sizeof(text), "type=u16, size=[%d]", (int)item->size);
                        break;
                    case DICTYPE_U32:
                        snprintf(text, sizeof(text), "type=u32, size=[%d]", (int)item->size);
                        break;
                    case DICTYPE_U64:
                        snprintf(text, sizeof(text), "type=u64, size=[%d]", (int)item->size);
                        break;
                    case DICTYPE_STRING:
                        snprintf(text, sizeof(text), "type=str, size=[%d], data=[%s]", (int)item->size, (char*)item->data);
                        break;
                    case DICTYPE_DATA:
                        snprintf(text, sizeof(text), "type=dat, size=[%d]", (int)item->size);
                        break;
                    default:
                        snprintf(text, sizeof(text), "type=unknown");
                        break;
                }
                strlcatf(buffer, sizeof(buffer) ,"%s", text);
                msgprintf(MSG_FORCE, "%s\n", buffer);
            }
        }
    }
    else
    {
        msgprintf(MSG_FORCE, "dico is empty\n");
    }
    
    msgprintf(MSG_FORCE, "-----------------debug-dico-end(%s)------------------\n\n", debugtxt);
    return 0;
}
