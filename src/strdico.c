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

/*
 * Defines an object to store a dictionnary where the key and the 
 * associated value are both strings. It's used to store string of
 * parameters such as "id=0,dest=/dev/sda1,mkfs=reiserfs"
 * cstrdico *d;
 * d=strdico_alloc();
 * strdico_set_valid_keys(d, "name,phone,fax");
 * strdico_parse_string(d, "name=john,phone=123456789,fax=");
 * strdico_parse_string(d, "phone=987654321");
 * strdico_get_string(d, mybuffer, sizeof(mybuffer), "phone");
 * strdico_destroy(d);
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

#include "fsarchiver.h"
#include "strdico.h"
#include "common.h"
#include "error.h"

cstrdico *strdico_alloc()
{
    cstrdico *d;
    if ((d=malloc(sizeof(cstrdico)))==NULL)
        return NULL;
    d->head=NULL;
    d->validkeys=NULL;
    return d;
}

int strdico_destroy(cstrdico *d)
{
    cstrdicoitem *item, *next;
    
    assert(d);
    
    item=d->head;
    while (item!=NULL)
    {
        next=item->next;
        free(item->key);
        if (item->value!=NULL)
            free(item->value);
        free(item);
        item=next;
    }
    
    if (d->validkeys!=NULL)
        free(d->validkeys);
    
    free(d);
    
    return FSAERR_SUCCESS;
}


int strdico_set_valid_keys(cstrdico *d, const char *keys)
{
    assert(d);
    
    if ((d->validkeys=strdup(keys))==NULL)
    {   errprintf("strdup() failed: out of memory\n");
        return FSAERR_ENOMEM;
    }
    
    return FSAERR_SUCCESS;
}

int strdico_parse_string(cstrdico *d, const char *strdefs)
{
    char *bakdefs;
    char *saveptr;
    char *result;
    char delims[]=",;\t\n";
    char key[1024];
    char value[1024];
    int i, pos;
    int res;
    
    assert(d);
    assert(strdefs);
    
    // init
    if ((bakdefs=strdup(strdefs))==NULL)
    {   errprintf("strdup() failed: out of memory\n");
        return FSAERR_ENOMEM;
    }
    
    result=strtok_r(bakdefs, delims, &saveptr);
    while (result!=NULL)
    {
        memset(key, 0, sizeof(key));
        memset(value, 0, sizeof(value));
        
        for (i=0; (result[i]!=0) && (result[i]!='=') && (i<sizeof(key)-1); i++)
        {
            key[i]=result[i];
        }
        if (result[i++]!='=')
        {   errprintf("Incorrect syntax in \"%s\" . Cannot find symbol '=' to separate the key and the value. "
                "expected something like \"name1=val1,name2=val2\"\n", result);
            free(bakdefs);
            return FSAERR_EINVAL;
        }
        for (pos=0; (result[i]!=0) && (pos<sizeof(value)-1); pos++)
        {
            value[pos]=result[i++];
        }
        
        if ((res=strdico_set_value(d, key, value))!=0)
        {   free(bakdefs);
            return res;
        }
        result=strtok_r(NULL, delims, &saveptr);
    }
    
    free(bakdefs);
    return FSAERR_SUCCESS;
}

int strdico_set_value(cstrdico *d, const char *key, const char *value)
{
    cstrdicoitem *existingitem;
    cstrdicoitem *lnew;
    cstrdicoitem *item;
    char *bakvalidkeys;
    char *result;
    char *saveptr;
    char delims[]=",;\t\n";
    int validkey;
    char *oldvalue;
    int vallen;
    int keylen;
    
    assert(d);
    assert(key);
    assert(value);
    
    // init
    keylen=strlen(key);
    vallen=strlen(value);
    
    // if there is a restriction on keys then check that the key is valid
    if (d->validkeys!=NULL) // if there is a restriction on keys
    {
        if ((bakvalidkeys=strdup(d->validkeys))==NULL)
        {   errprintf("strdup() failed: out of memory\n");
            return FSAERR_ENOMEM;
        }
        
        validkey=false;
        result=strtok_r(bakvalidkeys, delims, &saveptr);
        while ((result!=NULL) && (validkey==false))
        {
            if (strcmp(result, key)==0)
                validkey=true;
            result=strtok_r(NULL, delims, &saveptr);
        }
        free(bakvalidkeys);
        if (validkey==false)
        {   errprintf("unexpected key \"%s\". valid keys are \"%s\"\n", key, d->validkeys);
            return FSAERR_EINVAL;
        }
    }
    
    // check if key already exists
    existingitem=NULL;
    for (item=d->head; (item!=NULL) && (existingitem==NULL); item=item->next)
        if (strcmp(item->key, key)==0)
           existingitem=item;
    
    // case_1: modify the existing item if found
    if (existingitem!=NULL)
    {
        oldvalue=existingitem->value;
        if ((existingitem->value=malloc(vallen+1))==NULL)
        {   errprintf("malloc(%d) failed: out of memory\n", vallen+1);
            return -FSAERR_ENOMEM;
        }
        snprintf(existingitem->value, vallen+1, "%s", value);
        if (oldvalue!=NULL)
            free(oldvalue); // only free oldvalue if malloc is successful
        return FSAERR_SUCCESS;
    }
    
    // case_2: insert new pair in list
    if ((lnew=malloc(sizeof(cstrdicoitem)))==NULL)
    {   errprintf("malloc(%ld) failed: out of memory\n", (long)sizeof(cstrdicoitem));
        return -FSAERR_ENOMEM;
    }
    memset(lnew, 0, sizeof(cstrdicoitem));
    if ((lnew->key=malloc(keylen+1))==NULL)
    {   errprintf("malloc(%d) failed: out of memory\n", keylen+1);
        free(lnew);
        return -FSAERR_ENOMEM;
    }
    snprintf(lnew->key, keylen+1, "%s", key);
    if ((lnew->value=malloc(vallen+1))==NULL)
    {   errprintf("malloc(%d) failed: out of memory\n", vallen+1);
        free (lnew);
        return -FSAERR_ENOMEM;
    }
    snprintf(lnew->value, vallen+1, "%s", value);
    lnew->next=d->head;
    d->head=lnew;
    
    return FSAERR_SUCCESS;
}

int strdico_get_string(cstrdico *d, char *outbuffer, int outbufsize, const char *key)
{
    cstrdicoitem *item=NULL;
    
    assert(d);
    assert(outbuffer);
    assert(key);
    
    // init
    memset(outbuffer, 0, outbufsize);
    
    for (item=d->head; item!=NULL; item=item->next)
    {
        if (strcmp(item->key, key)==0) // found item with that key
        {   
            if (item->value!=NULL)
                snprintf(outbuffer, outbufsize, "%s", item->value);
            return FSAERR_SUCCESS;
        }
    }
    
    return FSAERR_ENOENT;
}

int strdico_get_s64(cstrdico *d, s64 *value, const char *key)
{
    char buffer[1024];
    char *endptr=NULL;
    int res;
    
    assert(d);
    assert(value);
    assert(key);
    
    *value=-1;
    
    if ((res=strdico_get_string(d, buffer, sizeof(buffer), key)) != 0)
        return res;
    
    if (strlen(buffer)<=0)
    {   errprintf("key \"%s\" has an empty value. expected a valid number\n", key);
        return FSAERR_EINVAL;
    }
    
    errno=0;
    *value=strtoll(buffer, &endptr, 10);
    if ((errno!=0) || (*endptr!=0))
    {   errprintf("key \"%s\" does not contain a valid number: \"%s\"\n", key, buffer);
        return FSAERR_EINVAL;
    }
    
    return FSAERR_SUCCESS;
}

int strdico_print(cstrdico *d)
{
    cstrdicoitem *item=NULL;
    int pos=0;
    
    assert(d);
    
    for (item=d->head; item!=NULL; item=item->next)
        printf("item[%d]: key=[%s] value=[%s]\n", pos++, item->key, item->value);
    
    return FSAERR_SUCCESS;
}
