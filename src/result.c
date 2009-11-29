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

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <execinfo.h>

#include "fsarchiver.h"
#include "common.h"
#include "result.h"

#define STACK_DEPTH 20

// To have the function names in the stack trace, the program must be compiled with the
// following options: "gcc -0" (no optimization) and the linker must be used with "-rdynamic"

cres _resnew(const char *file, const char *fct, const int line, int status, char *format, ...)
{
	void *buffer[STACK_DEPTH];
	char **strings;
	cres ret;
	va_list ap;
	int nptrs;
	int i;
	
	// format the information given by the macro
	snprintf(ret.origfunction, sizeof(ret.origfunction), "%s", fct);
	snprintf(ret.origfile, sizeof(ret.origfile), "%s", file);
	ret.origline=line;
	
	// format information given by the programmer
	ret.status=status;
	va_start(ap, format);
	vsnprintf(ret.text, sizeof(ret.text), format, ap);
	va_end(ap);
	
	// format the backtrace (advanced error info)
	memset(ret.stack, 0, sizeof(ret.stack));
	nptrs=backtrace(buffer, STACK_DEPTH);
	strings=backtrace_symbols(buffer, nptrs);
	if (strings!=NULL)
	{
		for (i = 0; i < nptrs; i++)
			strlcatf(ret.stack, sizeof(ret.stack), "%s\n", strings[i]);
		free(strings);
	}
	
	return ret;
}

cres resappend(cres ret1, char *format, ...)
{
	cres ret2;
	va_list ap;
	int len;
	
	// copy info from old cres
	ret2.status=ret1.status;
	len=strlen(ret1.text);
	strncpy(ret2.text, ret1.text, sizeof(ret2.text));
	strncpy(ret2.stack, ret1.stack, sizeof(ret2.stack));
	
	va_start(ap, format);
	vsnprintf(ret2.text+len, sizeof(ret2.text)-len, format, ap);
	va_end(ap);
	
	return ret2;
}

