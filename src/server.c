/* server.c
 * Server socket
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
	int n_connections;
	SocketHandle hd;
	struct event *evt;
} Server;

static void session_state_change_cb
		(Session *session, SessionState state, void *data)
{
	Server *server = (Server *) data;

	if (state == SESSION_CLOSED)
	{
		session_destroy(session);

		if (server->n_connections > 0)
		{
			server->n_connections--;
			if (server->n_connections <= 0)
			{
				event_base_loopbreak(evbase);
			}
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

void server_create(const char *str, int n_connections)
{
	Server *server;
	SocketAddress addr;
	const Error *e;
	
	server = (Server *) fs_malloc(sizeof(Server));
	server->n_connections = n_connections;
	abort_if_fail(n_connections != 0, "Assertion failure");
	
 	abort_if_fail (socket_address_from_str(str, &addr) == STATUS_SUCCESS,
			"Failed to read binding address");
	if(addr.port == 0)
		addr.port = htons(1080);
	e = socket_handle_create_listener(addr, &(server->hd));
	if (e)
		abort_with_error("Failed to create listener socket: %s", 
				error_desc(e));
	socket_handle_set_blocking(server->hd, 0);
	server->evt = socket_handle_create_event
		(server->hd, EV_READ | EV_PERSIST, server_check, server);
	event_add(server->evt, NULL);
	
	printf("Listening at %s\n", str);
}
