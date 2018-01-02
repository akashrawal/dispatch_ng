/* libtest.c
 * Common code for all tests
 * 
 * Copyright 2015-2018 Akash Rawal
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

#include "libtest.h"

//variables
static struct
{
	unsigned char *data;
	size_t len;
} variables['z' - 'p' + 1];

void set_variable_loc(char id, void *data, size_t len)
{
	abort_if_fail(id >= 'p' && id <= 'z',
			"Incorrect variable letter");

	variables[id - 'p'].data = (unsigned char *) data;
	variables[id - 'p'].len = len;
}

//Script

static int get_hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else
		return -1;
}

ScriptElement *script_build(const char *src)
{
	ScriptElement *script;
	int n_dialogs;
	int i;

	//Find number of dialogs
	n_dialogs = 0;
	for (i = 0; src[i]; i++)
	{
		if (src[i] == 'l')
			n_dialogs++;
	}

	//Allocate array
	script = fs_malloc(sizeof(ScriptElement) * (n_dialogs + 1));

	{
		ScriptElement *ptr = NULL;
		size_t alloc_len = 0;

#define data_add(dchr, mchr) \
		do { \
			abort_if_fail(ptr, \
					"Cannot add bytes before creating a new dialogue"); \
			if (alloc_len >= ptr->len) \
			{ \
				alloc_len *= 2; \
				ptr->data = fs_realloc(ptr->data, alloc_len); \
				ptr->mask = fs_realloc(ptr->mask, alloc_len); \
			} \
			ptr->data[ptr->len] = dchr; \
			ptr->mask[ptr->len] = mchr; \
			ptr->len++; \
		} while (0)
			

		for (i = 0; src[i]; i++)
		{
			char c = src[i];
			int tens, ones;

			//Byte
			if ((tens = get_hex_digit(c)) >= 0)
			{
				abort_if_fail((ones = get_hex_digit(src[++i])) >= 0,
						"Another hex digit expected");
				ones += tens * 16;

				data_add(ones, 0);
			}
			else if (c == 'k')
			{
				data_add(0, 1);
			}
			else if (c >= 'p' && c <= 'z')
			{
				int j;
				for (j = 0; j < variables[c - 'p'].len; j++)	
					data_add(variables[c - 'p'].data[j], 0);
			}
			else if (c == 'l')
			{
				if (ptr)
					ptr++;
				else
					ptr = script;

				alloc_len = 8;
				ptr->len = 0;
				ptr->data = fs_malloc(alloc_len);
				ptr->mask = fs_malloc(alloc_len);
			}
		}
	}

	script[n_dialogs].len = 0;
	script[n_dialogs].data = NULL;
	script[n_dialogs].mask = NULL;

	return script;
}

void script_free(ScriptElement *script)
{
	int i;

	for (i = 0; script[i].data; i++)
	{
		free(script[i].data);
		free(script[i].mask);
	}
	free(script);
}


//Actor
struct _Actor
{
	SocketHandle hd;
	ScriptElement *script;
	int is_odd_sprite; //< Strictly 0 or 1
	int dialog, pos;
	struct event *event;
	/* if (p->dialog % 2 == p->is_odd_sprite)
	 *     write(p->script[p->dialog]);
	 * else
	 *     read(p->script[p->dialog]);
	 */
};

#define ACTOR_SERVER_MODE -1
#define ACTOR_CLIENT_MODE -2

static void actor_prepare(Actor *p);

static void actor_check(evutil_socket_t fd, short events, void *data)
{
	Actor *p = (Actor *) data;

	const Error *e = NULL;

	event_free(p->event);
	p->event = NULL;

	if (p->dialog == ACTOR_SERVER_MODE)
	{
		SocketHandle new_hd;
		e = socket_handle_accept(p->hd, &new_hd);
		if (e)
		{
			abort_if_fail(e->type == socket_error_again, "Error: %s",
					error_desc(e));
			error_handle(e);
			e = NULL;
		}
		else
		{
			p->dialog = 0;
			socket_handle_close(p->hd);
			abort_on_error(socket_handle_set_blocking(new_hd, 0));
			p->hd = new_hd;
		}
	}
	else
	{
		ScriptElement cl = p->script[p->dialog];
		abort_if_fail(cl.data, "Assertion failure");
		abort_if_fail(cl.mask, "Assertion failure");

		if (p->dialog % 2 == p->is_odd_sprite)
		{
			size_t out;

			//Write
			abort_if_fail(events & EV_WRITE && (! (events & EV_READ)),
					"Assertion failure");

		
			e = socket_handle_write(p->hd,
					cl.data + p->pos, cl.len - p->pos,
					&out);
			if (e)
			{
				if (e->type == socket_error_again)
				{
					error_handle(e);
					e = NULL;
				}
				else
				{
					abort_with_error("socket_handle_write() failed: %s",
							error_desc(e));
				}
			}
			else
			{
				abort_if_fail(out > 0, "Assertion failure");
				abort_if_fail(out <= cl.len - p->pos,
						"Too much data sent");
				p->pos += out;
				if (p->pos == cl.len)
				{
					p->dialog++;
					p->pos = 0;
				}
			}
		}
		else
		{
			size_t out;
			unsigned char buf[64];

			//Read
			abort_if_fail(events & EV_READ && (! (events & EV_WRITE)),
					"Assertion failure");

			e = socket_handle_read(p->hd, buf, sizeof(buf), &out);
			if (e)
			{
				if (e->type == socket_error_again)
				{
					error_handle(e);
					e = NULL;
				}
				else
				{
					abort_with_error("socket_handle_write() failed: %s",
							error_desc(e));
				}
			}
			else
			{
				int i;
				abort_if_fail(out > 0, "Assertion failure");
				abort_if_fail(out <= cl.len - p->pos,
						"Too much data received");
				for (i = 0; i < out; i++)
				{
					if (cl.mask[p->pos + i] == 1)
						continue;
					abort_if_fail(buf[i] == cl.data[p->pos + i],
							"Incorrect data received, i = %d, p->pos = %d",
							(int) i, (int) p->pos);
				}

				p->pos += out;
				if (p->pos == cl.len)
				{
					p->dialog++;
					p->pos = 0;
				}
			}
		}
	}

	actor_prepare(p);
}

static void actor_prepare(Actor *p)
{
	short flags = 0;

	abort_if_fail(! p->event, "Assertion failure");

	//Handle special cases
	if (p->dialog == ACTOR_SERVER_MODE)
	{
		flags = EV_READ;
	}
	//If we have finished the script then release the event loop
	else if (! p->script[p->dialog].data)
	{
		evloop_release();
		socket_handle_close(p->hd);
		return;
	}
	else
	{
		if (p->dialog % 2 == p->is_odd_sprite)
			flags |= EV_WRITE;
		else
			flags |= EV_READ;
	}

	p->event = socket_handle_create_event(p->hd, flags, actor_check, p);
	event_add(p->event, NULL);
}

void actor_destroy(Actor *p)
{
	socket_handle_close(p->hd);
	if (p->event)
	{
		event_free(p->event);
		p->event = NULL;
	}
	free(p);
}

static Actor *actor_create(SocketHandle hd, ScriptElement *script, int is_odd_sprite)
{
	Actor *p;

	p = fs_malloc(sizeof(Actor));

	p->hd = hd;
	p->script = script;
	p->dialog = 0;
	p->pos = 0;
	p->event = NULL;
	p->is_odd_sprite = is_odd_sprite;
	
	evloop_hold();

	return p;
}

Actor *actor_create_server(SocketHandle hd, ScriptElement *script)
{
	Actor *p;

	p = actor_create(hd, script, 1);	
	p->dialog = ACTOR_SERVER_MODE;
	actor_prepare(p);
	return p;
}

Actor *actor_create_client(SocketAddress addr, ScriptElement *script)
{
	SocketHandle hd;
	const Error *e;
	Actor *p;
	SocketAddress local_addr;

	memset(&local_addr, 0, sizeof(SocketAddress));
	local_addr.host.type = addr.host.type;

	abort_on_error(socket_handle_create_bound(local_addr, &hd));
	abort_on_error(socket_handle_set_blocking(hd, 0));

	e = socket_handle_connect(hd, addr);	
	if (e)
	{
		abort_if_fail(e->type == socket_error_in_progress, "Error: %s",
				error_desc(e));
		error_handle(e);
		e = NULL;
	}

	p = actor_create(hd, script, 0);
	actor_prepare(p);

	return p;
}

void test_open_listener
	(const char *host, SocketHandle *hd_out, SocketAddress *addr_out)
{
	SocketAddress bind_addr;
	SocketAddress addr;
	SocketHandle hd;

	abort_if_fail(host_address_from_str(host, &bind_addr.host),
			"Incorrect host address");
	bind_addr.port = 0;

	abort_on_error(socket_handle_create_listener(bind_addr, &hd));
	abort_on_error(socket_handle_getsockname(hd, &addr));

	*hd_out = hd;
	*addr_out = addr;
}

