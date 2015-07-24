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
	int fd;
	struct event *evt;
} Server;

void server_check(evutil_socket_t fd, short events, void *data)
{
	int client_fd;
	
	client_fd = accept(fd, NULL, NULL);
	if (client_fd < 0)
		abort_with_liberror("accept()");
	
	session_create(client_fd);
}

void server_create(const char *str)
{
	Server *server;
	Address addr;
	int port;
	
	server = (Server *) fs_malloc(sizeof(Server));
	
	address_read(&addr, str, &port, PORT);
	if (port <= 0)
		port = 1080;
	server->fd = address_open_svr(&addr, port);
	
	fd_set_blocking(server->fd, 0);
	server->evt = event_new(evbase, server->fd, EV_READ | EV_PERSIST, 
		server_check, server);
	event_add(server->evt, NULL);
}
