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

#include <pthread.h>
#include <errno.h>

#ifdef OPTION_CRYPTO_SUPPORT

#include "common.h"
#include "crypto.h"

// required for safety with multi-threading in gcrypt
GCRY_THREAD_OPTION_PTHREAD_IMPL;

int crypto_init()
{
    // init gcrypt for multi-threading
    gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
    
    // check that header files and library match
    if (!gcry_check_version(GCRYPT_VERSION))
    {
        errprintf("libgcrypt version mismatch\n");
        return -1;
    }
    
    // disable secure memory
    gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
    
    // tell libgcrypt that initialization has completed.
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
    
    return 0;
}

int crypto_cleanup()
{
    return 0;
}

int crypto_blowfish(u64 insize, u64 *outsize, u8 *inbuf, u8 *outbuf, u8 *password, int passlen, int enc)
{
    gcry_cipher_hd_t hd;
    u8 iv[] = "fsarchiv";
    int res;
    
    // init
    if ((password==NULL) || (passlen==0))
        return -1;
    
    // gcry_error_t gcry_cipher_open (gcry_cipher_hd_t *handle, int algo, int mode, unsigned int flags);
    if ((res=gcry_cipher_open(&hd, GCRY_CIPHER_BLOWFISH, GCRY_CIPHER_MODE_CFB, GCRY_CIPHER_SECURE))!=0)
    {
        errprintf("gcry_cipher_open() failed\n");
        gcry_cipher_close(hd);
        return -1;
    }
    
    // gcry_error_t gcry_cipher_setkey (gcry_cipher_hd_t hd, const void *key, size_t keylen);
    if ((res=gcry_cipher_setkey(hd, password, passlen))!=0)
    {
        errprintf("gcry_cipher_setkey() failed\n");
        gcry_cipher_close(hd);
        return -1;
    }
    
    // gcry_error_t gcry_cipher_setiv (gcry_cipher_hd_t h, const void *k, size_t l)
    if (gcry_cipher_setiv(hd, iv, strlen((char*)iv)))
    {
        errprintf("gcry_cipher_setiv() failed\n");
        gcry_cipher_close(hd);
        return -1;
    }
    
    switch(enc)
    {
        case 1: // encrypt
            // gcry_error_t gcry_cipher_encrypt (gcry_cipher_hd_t h, void *out, size_t outsize, const void *in, size_t inlen);
            res=gcry_cipher_encrypt(hd, outbuf, insize, inbuf, insize);
            break;
        case 0: // decrypt
            // gcry_error_t gcry_cipher_decrypt (gcry_cipher_hd_t h, void *out, size_t outsize, const void *in, size_t inlen);
            res=gcry_cipher_decrypt(hd, outbuf, insize, inbuf, insize);
            break;
        default: // invalid
            errprintf("invalid parameter: enc=%d\n", (int)enc);
            gcry_cipher_close(hd);
            return -1;
    }
    
    // void gcry_md_close (gcry_md_hd_t hd);    
    gcry_cipher_close(hd);
    
    *outsize=insize;
    return (res==0)?(0):(-1);
}

int crypto_random(u8 *buf, int bufsize)
{
    memset(buf, 0, bufsize);
    gcry_randomize(buf, bufsize, GCRY_STRONG_RANDOM);
    return 0;
}

#endif // OPTION_CRYPTO_SUPPORT
