/* server.h
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

#include "server.h"

typedef struct
{
	GPollFD fd;
	InterfaceManager *manager;
} Server;

static gboolean server_prepare(GSource *source, gint *timeout)
{
	Server *server = (Server *) source;
	
	server->fd.revents = 0;
	*timeout = -1;
	return FALSE;
}

static gboolean server_check(GSource *source)
{
	Server *server = (Server *) source;
	
	if (server->fd.revents & G_IO_IN)
		return TRUE;
	return FALSE;
}

static gboolean server_dispatch
	(GSource *source, GSourceFunc cb, gpointer data)
{
	Server *server = (Server *) source;
	int client_fd;
	
	client_fd = accept(server->fd.fd, NULL, NULL);
	if (client_fd < 0)
		abort_with_liberror("accept()");
	
	connection_create(client_fd, server->manager);
	
	return G_SOURCE_CONTINUE;
}

static void server_finalize(GSource *source)
{
	Server *server = (Server *) source;
	
	close(server->fd.fd);
}

GSourceFuncs server_funcs = 
{
	server_prepare,
	server_check,
	server_dispatch,
	server_finalize
};

//Creates server socket listening at given interface
guint server_create(Interface *iface, InterfaceManager *manager)
{
	Server *server;
	guint tag;
	int i;
	
	//Create GSource
	server = (Server *) g_source_new(&server_funcs, sizeof(Server));
	
	//Create socket
	server->fd.fd = interface_open_server(iface);
	
	//Set nonblocking
	fd_set_blocking(server->fd.fd, 0);
	
	//Init
	server->fd.events = 0;
	server->manager = manager;
	
	//Add to default context
	tag = g_source_attach((GSource *) server, NULL);
	g_source_unref((GSource *) server);
	
	return tag;
}
