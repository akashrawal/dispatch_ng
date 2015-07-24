/* utils.h
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



#ifndef EWOULDBLOCK
#define IO_TEMP_ERROR(e) ((e == EAGAIN) || (e == EINTR))
#else
#if (EAGAIN == EWOULDBLOCK)
#define IO_TEMP_ERROR(e) ((e == EAGAIN) || (e == EINTR))
#else
#define IO_TEMP_ERROR(e) ((e == EAGAIN) || (e == EWOULDBLOCK) || (e == EINTR))
#endif
#endif


//Error reporting

void abort_with_error(const char *fmt, ...);

void abort_with_liberror(const char *fmt, ...);

int fd_set_blocking(int fd, int val);

//Allocation

void *fs_malloc(size_t size);

void *fs_realloc(void *mem, size_t size);

//Event loop (Requires init)
extern struct event_base *evbase;

void utils_init();
