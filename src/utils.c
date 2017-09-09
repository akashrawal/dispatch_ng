/* utils.c
 * Common utility functions
 * 
 * Copyright 2015 Akash Rawal
 * This file is part of dispatch_ng.
 * 
 * dispatch_ng is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * dispatch_ng is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with dispatch_ng.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "incl.h"

#include <fcntl.h>

//Abort functions

void abort_with_error(const char *fmt, ...)
{
	va_list arglist;
	
	fprintf(stderr, "ERROR: ");
	
	va_start(arglist, fmt);
	vfprintf(stderr, fmt, arglist);
	va_end(arglist);
	
	fprintf(stderr, "\n");
	
	abort();
}

void abort_with_liberror(const char *fmt, ...)
{
	va_list arglist;
	
	fprintf(stderr, "ERROR: ");
	
	va_start(arglist, fmt);
	vfprintf(stderr, fmt, arglist);
	va_end(arglist);
	
	fprintf(stderr, ": %s\n", strerror(errno));
	
	abort();
}

void *fs_malloc(size_t size)
{
	void *mem = malloc(size);
	
	if (! mem)
		abort_with_error("Failed to allocate %ld bytes, aborting...",
			(long) size);
	
	return mem;
}

void *fs_realloc(void *mem, size_t size)
{
	mem = realloc(mem, size);
	
	if (! mem)
		abort_with_error("Failed to allocate %ld bytes, aborting...",
			(long) size);
	
	return mem;
}

//Splits given string with given delimiter.
//Only left string (returned one) needs to be freed.
//If delimiter is not found, NULL is returned.
//Rightmost delimeter is considered.
char *split_string(const char *str, char delim, char **right_out)
{
	int len, delim_index = -1;
	char *str_copy;

	delim_index = -1;
	for (len = 0; str[len]; len++)
		if (str[len] == delim)
			delim_index = len;

	if (delim_index == -1)
		return NULL;

	str_copy = fs_malloc(len + 1);
	strcpy(str_copy, str);
	str_copy[delim_index] = 0;

	if (right_out)
		*right_out = str_copy + delim_index + 1;
	return str_copy;
}

char *fs_strdup_vprintf(const char *fmt, va_list arglist)
{
#ifdef _WIN32
#define _rpl_vsnprintf _vsnprintf
#else
#define _rpl_vsnprintf vsnprintf
#endif

	va_list arglist_bak;
	char *res;
	int len;

	va_copy(arglist_bak, arglist);
	len = _rpl_vsnprintf(NULL, 0, fmt, arglist_bak);
	va_end(arglist_bak);
	
	if (len < 0)
		abort_with_error("strdup_printf(): failed to fmt string");

	res = fs_malloc(len + 1);
	len = _rpl_vsnprintf(res, len + 1, fmt, arglist);

	if (len < 0)
		abort_with_error("strdup_printf(): failed to fmt string");

	return res;

#undef _rpl_vsnprintf
}

char *fs_strdup_printf(const char *fmt, ...)
{
	char *res;
	va_list arglist;

	va_start(arglist, fmt);
	res = fs_strdup_vprintf(fmt, arglist);
	va_end(arglist);

	return res;
}

Status parse_long(const char *str, long *out)
{
	if (*str)
	{
		const char *end_ptr;
		long res = strtol(str, (char **) &end_ptr, 10);

		if (! * end_ptr)
		{
			*out = res;
			return STATUS_SUCCESS;
		}
	}

	return STATUS_FAILURE;
}

//Error reporting

//Generic error

define_static_error(error_generic, "Unidentified error");

const Error *error_printf(const char *type, const char *fmt, ...)
{
	va_list arglist;
	Error *e = (Error *) fs_malloc(sizeof(Error));

	e->type = type;

	va_start(arglist, fmt);
	e->desc = fs_strdup_vprintf(fmt, arglist);
	va_end(arglist);

	return e;
}

//Event loop

struct event_base *evbase;
struct evdns_base *evdns_base;

//Module initializer
void utils_init()
{
	evbase = event_base_new();
	evdns_base = evdns_base_new(evbase, 1);
}
