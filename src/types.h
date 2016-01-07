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

#ifndef __TYPES_H__
#define __TYPES_H__

#include <endian.h>
#include <stdint.h>
#include <stdbool.h>
#include <byteswap.h>
#include <linux/types.h>

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;

#define le8_to_cpu(v) (v)
#define cpu_to_le8(v) (v)

#if __BYTE_ORDER == __BIG_ENDIAN // CPU == big endian (sparc, ppc, ...)
#    define cpu_to_le16(x) ((u16)(bswap_16(x)))
#    define le16_to_cpu(x) ((u16)(bswap_16(x)))
#    define cpu_to_le32(x) ((u32)(bswap_32(x)))
#    define le32_to_cpu(x) ((u32)(bswap_32(x)))
#    define cpu_to_le64(x) ((u64)(bswap_64(x)))
#    define le64_to_cpu(x) ((u64)(bswap_64(x)))
#    define be16_to_cpu(x) ((u16)(x))
#    define cpu_to_be16(x) ((u16)(x))
#    define be32_to_cpu(x) ((u32)(x))
#    define cpu_to_be32(x) ((u32)(x))
#    define be64_to_cpu(x) ((u64)(x))
#    define cpu_to_be64(x) ((u64)(x))
#else // CPU == little endian (intel)
#    define cpu_to_le16(x) ((u16)(x))
#    define le16_to_cpu(x) ((u16)(x))
#    define cpu_to_le32(x) ((u32)(x))
#    define le32_to_cpu(x) ((u32)(x))
#    define cpu_to_le64(x) ((u64)(x))
#    define le64_to_cpu(x) ((u64)(x))
#    define be16_to_cpu(x) ((bswap_16(x)))
#    define cpu_to_be16(x) ((bswap_16(x)))
#    define be32_to_cpu(x) ((bswap_32(x)))
#    define cpu_to_be32(x) ((bswap_32(x)))
#    define be64_to_cpu(x) ((bswap_64(x)))
#    define cpu_to_be64(x) ((bswap_64(x)))
#endif

// atomic type.
typedef struct 
{   volatile int counter;
} atomic_t;

// read atomic variable
#define atomic_read(v) ((v)->counter)
 
// Set atomic variable
#define atomic_set(v,i) (((v)->counter) = (i))

#endif // __TYPES_H__
