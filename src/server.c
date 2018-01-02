/* server.c
 * Server socket
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

#include "incl.h"


struct _Server
{
	SocketHandle hd;
	struct event *evt;
	int test_mode;
};

static void session_state_change_cb
		(Session *session, SessionState state, void *data)
{
	Server *server = (Server *) data;

	if (state == SESSION_CLOSED)
	{
		session_destroy(session);

		if (server->test_mode)
		{
			event_del(server->evt);
			event_free(server->evt);
			server->evt = NULL;
			evloop_release();
		}
	}

}

void server_check(evutil_socket_t fd, short events, void *data)
{
	Server *server = (Server *) data;

	SocketHandle client_hd;
	Session *session;
	const Error *e;
	
	e = socket_handle_accept(server->hd, &client_hd);
	if (e)
		abort_with_error("Failed to accept new connection: %s",
				error_desc(e));
	
	session = session_create(client_hd);
	session_set_callback(session, session_state_change_cb, server);
}

static Server *server_create_internal(SocketHandle hd, int test_mode)
{
	Server *server = (Server *) fs_malloc(sizeof(Server));

	server->hd = hd;

	server->evt = socket_handle_create_event
		(server->hd, EV_READ | EV_PERSIST, server_check, server);
	event_add(server->evt, NULL);
	evloop_hold();

	server->test_mode = test_mode;

	return server;
}

Server *server_create(const char *str)
{
	SocketAddress addr;
	SocketHandle hd;
	const Error *e;
	
 	abort_if_fail (socket_address_from_str(str, &addr) == STATUS_SUCCESS,
			"Failed to read binding address");
	if(addr.port == 0)
		addr.port = htons(1080);
	e = socket_handle_create_listener(addr, &hd);
	abort_if_fail(!e, "Failed to create listener socket: %s", error_desc(e));
	e = socket_handle_set_blocking(hd, 0);
	abort_if_fail(!e, "Failed to enable nonblocking: %s", error_desc(e));

	printf("Listening at %s\n", str);
	return server_create_internal(hd, 0);
}

void server_destroy(Server *server)
{
	if (server->evt)
	{
		event_del(server->evt);
		event_free(server->evt);
		evloop_release();
	}
	socket_handle_close(server->hd);
	free(server);
}

Server *server_create_test(SocketHandle hd)
{
	return server_create_internal(hd, 1);
}
