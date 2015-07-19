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

//Error reporting

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

//Sets whether IO operations on fd should block
//val<0 means do nothing
int fd_set_blocking(int fd, int val)
{
	int stat_flags, res;
	
	//Get flags
	stat_flags = fcntl(fd, F_GETFL, 0);
	if (stat_flags < 0)
		abort_with_liberror
			("Failed to get file descriptor flags"
			" for file descriptor %d:", fd);
	
	//Get current status
	res = !(stat_flags & O_NONBLOCK);
	
	//Set non-blocking
	if (val > 0)
		stat_flags &= (~O_NONBLOCK);
	else if (val == 0)
		stat_flags |= O_NONBLOCK;
	else
		return res;
	
	//Set back the flags
	if (fcntl(fd, F_SETFL, stat_flags) < 0)
		abort_with_liberror
			("Failed to set file descriptor flags"
			" for file descriptor %d:", fd);
	
	return res;
}

//Event loop

struct event_base *evbase;
