/* flush.c
 * Asynchronous flush operator
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

typedef struct 
{
	int fd;
	char buf[BUFFER_SIZE];
	size_t len, start;
	struct event *evt;
} FlushInst;

static char dum_buf[BUFFER_SIZE];
static struct timeval timeout = {FLUSH_TIMEOUT, 0};

void flush_check(evutil_socket_t fd, short events, void *data)
{
	FlushInst *f = (FlushInst *) data;
	int res;
	
	if (events & EV_READ)
	{
		//Read data and discard
		while((res = read(f->fd, dum_buf, BUFFER_SIZE)) > 0)
			;
		if (res < 0)
			if (! IO_TEMP_ERROR(errno))
				goto destroy;
	}
	
	if (events & EV_WRITE)
	{
		res = write(f->fd, f->buf + f->start, f->len - f->start);
		if (res < 0)
		{
			if (! IO_TEMP_ERROR(errno))
				goto destroy;
		}
		else if (res == 0)
		{
			goto destroy;
		}
		else
		{
			f->start += res;
			if (f->start >= f->len)
				goto destroy;
		}
	}
	
	if (events & EV_TIMEOUT)
	{
		goto destroy;
	}
	
	return;
destroy:

	event_del(f->evt);
	event_free(f->evt);
	close(f->fd);
	free(f);
}

void flush_add(int fd, void *buf, int len)
{
	FlushInst *f = (FlushInst *) fs_malloc(sizeof(FlushInst));
	
	f->fd = fd;
	f->start = 0;
	f->len = len;
	memcpy(f->buf, buf, len);
	
	f->evt = event_new(evbase, fd, EV_READ | EV_WRITE | EV_PERSIST,
		flush_check, f);
	event_add(f->evt, &timeout);
}


