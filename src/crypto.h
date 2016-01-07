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

#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#include "types.h"

int crypto_init();
int crypto_blowfish(u64 insize, u64 *outsize, u8 *inbuf, u8 *outbuf, u8 *password, int passlen, int enc);
int crypto_random(u8 *buf, int bufsize);
int crypto_cleanup();

#endif // __CRYPTO_H__
