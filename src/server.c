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


struct _Server
{
	SocketHandle hd;
	struct event *evt;
	ServerEventCB cb;
	void *cb_data;
};

static void session_state_change_cb
		(Session *session, SessionState state, void *data)
{
	Server *server = (Server *) data;

	if (state == SESSION_CLOSED)
	{
		session_destroy(session);

		if (server->cb)
			(* server->cb)(server, SERVER_SESSION_CLOSE, server->cb_data);
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
	if (server->cb)
		(* server->cb)(server, SERVER_SESSION_OPEN, server->cb_data);
}

Server *server_create(const char *str)
{
	Server *server;
	SocketAddress addr;
	const Error *e;
	
	server = (Server *) fs_malloc(sizeof(Server));
	
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

	server->cb = NULL;
	server->cb_data = NULL;
	
	printf("Listening at %s\n", str);

	return server;
}

void server_destroy(Server *server)
{
	event_del(server->evt);
	event_free(server->evt);
	socket_handle_close(server->hd);
	free(server);
}

void server_set_cb(Server *server, ServerEventCB cb, void *cb_data)
{
	server->cb = cb;
	server->cb_data = cb_data;
}
