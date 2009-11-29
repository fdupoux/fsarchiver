/*
 * uuid.h -- utility routines for manipulating UUID's.
 * code from e2fsprogs-1.40.11
 * This code comes from e2fsprogs-1.40.11/lib/e2p, it's available under 
 * the GNU Library General Public License Version 2
 */

#ifndef __UUID_H__
#define __UUID_H__

struct uuid;
int e2p_is_null_uuid(void *uu);
const char *e2p_uuid2str(void *uu);

#endif // __UUID_H__
