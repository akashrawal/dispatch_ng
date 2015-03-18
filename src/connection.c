/* connection.c
 * Handles client connections.
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

#include "connection.h"

#define CONNECTION_BUFFER (2048)

typedef enum 
{
	CONNECTION_CLIENT = 0,
	CONNECTION_REMOTE = 1
};

typedef enum
{
	CONNECTION_AUTH,
	CONNECTION_REQUEST,
	CONNECTION_CONNECTING,
	CONNECTION_CONNECTED,
	CONNECTION_CLOSE
} ConnectionState;

typedef struct
{
	GSource source;
	
	struct {
		GPollFD in_fd;
		guint8 buffer[CONNECTION_BUFFER];
		int start, end;
	} lanes[2];
	
	int state;
	InterfaceManager *manager;
	Interface *iface;
	
} Connection;

//Peeks on data received from client into buffer.
static char *connection_peek(Connection *connection, int len)
{
	if (connection->lanes[CONNECTION_CLIENT].start + len 
		< connection->lanes[CONNECTION_CLIENT].end)
		return NULL;
	return connection->lanes[CONNECTION_CLIENT].buffer 
		+ connection->lanes[CONNECTION_CLIENT].start;
}

//Returns data received from client into buffer.
static char *connection_read(Connection *connection, int len)
{
	char *res = connection->lanes[CONNECTION_CLIENT].buffer 
		+ connection->lanes[CONNECTION_CLIENT].start;
	if (connection->lanes[CONNECTION_CLIENT].start + len 
		< connection->lanes[CONNECTION_CLIENT].end)
		return NULL;
	connection->lanes[CONNECTION_CLIENT].start += len;
	return res;
}

//Allocate space on buffer for writing
static char *connection_write_alloc(Connection *connection, int len)
{
	char *res = connection->lanes[CONNECTION_REMOTE].buffer 
		+ connection->lanes[CONNECTION_REMOTE].end;
	connection->lanes[CONNECTION_REMOTE].end += len;
	return res;
}

//Closes connection, after writing all data
static void connection_close(Connection *connection)
{
	connection->status = CONNECTION_CLOSE;
	
	if (connection->lanes[CONNECTION_REMOTE].end 
			- connection->lanes[CONNECTION_REMOTE].start == 0)
		g_source_destroy(connection);
}

static void connection_log
	(Connection *connection, const char *format, ...)
{
	va_list args;
	
	va_start(args, format);
	
	printf("Connection %d: ", 
			connection->lanes[CONNECTION_CLIENT].in_fd.fd); 
	
	vprintf(format, args);
	
	printf("\n"); 
	
	va_end(args);
}

static void connection_write_socks_error
	(Connection *connection, int socks_errcode)
{
	char *buffer;
	
	buffer = connection_write_alloc(connection, 10);
	buffer[0] = 5;
	buffer[1] = socks_errcode;
	buffer[2] = 0;
	buffer[3] = 1;
	buffer[4] = 0;
	buffer[5] = 0;
	buffer[6] = 0;
	buffer[7] = 0;
	buffer[8] = 0;
	buffer[9] = 0;
	connection_log(connection, 
		"SOCKS error code %d sent", socks_errcode);
	connection_close(connection);
}

void connection_connect_cb(ConnectorData *info)
{
	Connection *connection = (Connection *) info->data;
	guint8 *buffer;
	int reply_size;
	
	//Handle errors
	if (info->socks_status != 0)
	{
		connection_write_socks_error(connection, info->socks_status);
		return;
	}
	
	//Add reply
	if (info->iface->addr.sa_family == AF_INET)
	{
		reply_size = 10;
	}
	else
	{
		reply_size = 22;
	}
	
	buffer = connection_write_alloc(connection, reply_size);
	buffer[0] = 5;
	buffer[1] = 0;
	buffer[2] = 0;
	if (info->iface->addr.sa_family == AF_INET)
	{
		struct sockaddr_in *addr;
			= (struct sockaddr_in *) (info->iface->addr);
		buffer[3] = 1;
		memcpy(buffer + 4, &(addr->sin_addr), 4);
		memcpy(buffer + 8, &(addr->sin_port), 2);
	}
	else
	{
		struct sockaddr_in6 *addr;
			= (struct sockaddr_in *) (info->iface->addr);
		buffer[3] = 4;
		memcpy(buffer + 4, &(addr->sin6_addr), 16);
		memcpy(buffer + 20, &(addr->sin6_port), 2);
	}
	
	//Setup connection
	connection->lanes[1].in_fd.fd = info->remote_fd;
	connection->iface = info->iface;
	connection->status = CONNECTION_CONNECTED;
	fd_set_blocking(info->remote_fd, 0);
	
	//Assertions
	if (! connection->iface)
		abort_with_error("Assertion failure");
}

//Manages all authentication
void connection_authenticator(Connection *connection)
{
	int remote_fd;
	int i;
	guint8 *buffer;
	
	//SOCKS5 handshake
	if (connection->status == CONNECTION_AUTH)
	{
		int n_methods;
		guint8 selected = 0xff;
		
		buffer = connection_peek(connection, 2);
		if (! buffer)
			return;
		
		if (buffer[0] != 5)
		{
			connection_log("Unsupported SOCKS version %d", (int) buffer[0]);
			connection_close(connection);
			return;
		}
		n_methods = buffer[1];
		buffer = connection_peek(connection, 2 + n_methods);
		if (! buffer)
			return;
		
		//Select method only
		for (i = 0; i < n_methods; i++)
		{
			if (buffer[2 + i] == 0)
			{
				selected = 0;
				break;
			}
		}
		
		//Write reply
		buffer = connection_write_alloc(connection, 2);
		buffer[0] = 5;
		buffer[1] = selected;
	
		if (selected == 0xff)
		{
			connection_close(connection);
			return;
		}
		
		connection_read(connection, 2 + n_methods);
		connection->status = CONNECTION_REQUEST;
	}
	
	//Connection request
	if (connection->status == CONNECTION_REQUEST)
	{
		int socks_errcode = 0;
		
		buffer = connection_peek(connection, 4);
		if (! buffer)
			return;
		
		if (buffer[0] != 5 || buffer[2] != 0)
			socks_errcode = 1;
		else if (buffer[1] != 1)
			socks_errcode = 7;
		
		if (socks_errcode)
		{
			connection_write_socks_error(connection, socks_errcode);
			return;
		}
		
		//Read request
		if (buffer[3] == 2)
		{
			//Domain name
			int domain_len;
			char domain[257];
			int port;
			
			buffer = connection_peek(connection, 5);
			if (! buffer)
				return;
			domain_len = buffer[4];
			
			buffer = connection_read
				(connection, 4 + 1 + domain_len + 2);
			if (! buffer)
				return;
			
			//Copy out domain name
			memcpy(domain, buffer + 5, domain_len);
			domain[domain_len] = 0;
			
			//Copy out port
			port = ntohs(*((guint16 *) (buffer + 5 + domain_len)));
			
			//Connect
			connector_connect_by_name(domain, port, 
				connection->manager, connection_connect_cb, 
				connection);
		}
		else if (buffer[3] == 1)
		{
			//IPv4 address
			struct sockaddr_in addr;
			
			buffer = connection_read(connection, 4 + 4 + 2);
			if (! buffer)
				return;
			
			memcpy(&(addr.sin_addr), buffer + 4, 4);
			memcpy(&(addr.sin_port), buffer + 8, 2);
			
			//Connect
			connector_connect_by_name(&addr, 
				connection->manager, connection_connect_cb, 
				connection);
		}
		else if (buffer[3] == 4)
		{
			//IPv6 address
			struct sockaddr_in addr;
			
			buffer = connection_read(connection, 4 + 4 + 2);
			if (! buffer)
				return;
			
			memcpy(&(addr.sin_addr), buffer + 4, 4);
			memcpy(&(addr.sin_port), buffer + 8, 2);
			
			//Connect
			connector_connect_by_name(&addr, 
				connection->manager, connection_connect_cb, 
				connection);
		}
		else
		{
			connection_write_socks_error(connection, 8);
			return;
		}
		
		connection->status = CONNECTION_CONNECTING;
	}
	
	
}


//GSourceFuncs::prepare
static gboolean connection_prepare(GSource *source, gint *timeout)
{
	Connection *connection = (Connection *) source;
	int i, opposite;
	
	//Remove connection if nothing to write
	if (connection->status == CONNECTION_CLOSE)
	{
		if (connection->lanes[CONNECTION_REMOTE].end 
				- connection->lanes[CONNECTION_REMOTE].start == 0)
			g_source_destroy(connection);
	}
	
	for (i = 0; i < 2; i++)
		connection->lanes[i].in_fd.events = G_IO_HUP | G_IO_ERR;
	
	//Set events according to buffer status
	for (i = 0; i < 2; i++)
	{
		opposite = 1 - i;
		
		if (connection->lanes[i].end < CONNECTION_BUFFER)
			connection->lanes[i].in_fd.events |= G_IO_IN;
		if (connection->lanes[i].start < connection->lanes[i].end)
			connection->lanes[opposite].in_fd.events |= G_IO_OUT;
	}
	
	//Perform no IO on CONNECTION_REMOTE if not connected
	if (connection.state != CONNECTION_CONNECTED)
		connection->lanes[CONNECTION_REMOTE].in_fd.events = 0;
	
	*timeout = -1;
	return FALSE;
}

//GSourceFuncs::check
static gboolean connection_check(GSource *source)
{
	Connection *connection = (Connection *) source;
	int i;
	
	for (i = 0; i < 2; i++)
		if (connection->lanes[i].in_fd.revents)
			return TRUE;
	
	return FALSE;
}

//GSourceFuncs::dispatch
static gboolean connection_dispatch
	(GSource *source, GSourceFunc callback, gpointer user_data)
{
	Connection *connection = (Connection *) source;
	int i, opposite, io_res;
	
	//Perform no IO on CONNECTION_REMOTE if not connected
	if (connection.state != CONNECTION_CONNECTED)
		connection->lanes[CONNECTION_REMOTE].in_fd.revents = 0;
	
	//Check for errors
	for (i = 0; i < 2; i++)
		if (connection->lanes[i].in_fd.revents & (G_IO_HUP | G_IO_ERR))
			return G_SOURCE_REMOVE;
	
	for (i = 0; i < 2; i++)
	{
		opposite = 1 - i;
		
		//Read data into buffer
		if (connection->lanes[i].in_fd.revents & G_IO_IN)
		{
			io_res = read(connection->lanes[i].in_fd.fd,
				connection->lanes[i].buffer + connection->lanes[i].end,
				CONNECTION_BUFFER - connection->lanes[i].end);
			
			//Error handling
			if (io_res < 0)
			{
				if (! IO_TEMP_ERROR(errno))
					return G_SOURCE_REMOVE;
			}
			else if (io_res == 0)
				return G_SOURCE_REMOVE;
			
			connection->lanes[i].end += io_res;
		}
		
		//Write it to opposite lane
		if (connection->lanes[opposite].in_fd.revents & G_IO_OUT)
		{
			io_res = write(connection->lanes[opposite].in_fd.fd,
				connection->lanes[i].buffer + connection->lanes[i].start,
				connection->lanes[i].end - connection->lanes[i].start);
			
			//Error handling
			if (io_res < 0)
			{
				if (! IO_TEMP_ERROR(errno))
					return G_SOURCE_REMOVE;
			}
			else if (io_res == 0)
				return G_SOURCE_REMOVE;
			
			connection->lanes[i].start += io_res;
			
			//If start of buffer crosses middle,
			//shift the buffer contents to beginning
			if (connection->lanes[i].start >= (CONNECTION_BUFFER / 2))
			{
				int j, new_end;
				
				new_end = connection->lanes[i].end - connection->lanes[i].start;
				
				for (j = 0; j < new_end; j++)
					connection->lanes[i].buffer[j] 
						= connection->lanes[i].buffer
							[j + connection->lanes[i].start];
				
				connection->lanes[i].start = 0;
				connection->lanes[i].end = new_end;
			}
		}
	}
	
	//If status is at teardown,
	//remove the source if no data to write
	if (connection->status == CONNECTION_CLOSE
		&& (connection->lanes[CONNECTION_REMOTE].end 
			- connection->lanes[CONNECTION_REMOTE].start == 0))
	{
		return G_SOURCE_REMOVE;
	}
	
	connection_authenticator(connection);
	
	return G_SOURCE_CONTINUE;
}

//GSourceFuncs::finalize
static void connection_finalize(GSource *source)
{
	Connection *connection = (Connection *) source;
	int i;
	
	for (i = 0; i < 2; i++)
		close(connection->lanes[i].in_fd.fd);
	
	if (connection->iface)
		interface_close(connection->iface);
}

static GSourceFuncs connection_funcs =
{
	connection_prepare,
	connection_check,
	connection_dispatch,
	connection_finalize
};

//Add a connection to default context
guint connection_create(int fd, InterfaceManager *manager)
{
	Connection *connection;
	guint tag;
	int i;
	
	//Create GSource
	connection = (Connection *) g_source_new(&connection_funcs, sizeof(Connection));
	
	//Initialize lanes
	for (i = 0; i < 2; i++)
	{
		connection->lanes[i].in_fd.fd = fd;
		g_source_add_poll
			((GSource *) connection, &(connection->lanes[i].in_fd));
		connection->lanes[i].start + connection->lanes[i].end = 0;
	}
	
	//Add interface
	connection->state = CONNECTION_AUTH;
	connection->manager = manager;
	connection->iface = NULL;
	
	//Make fd nonbocking
	fd_set_blocking(fd, 0);
	
	//Add to default context
	tag = g_source_attach((GSource *) connection, NULL);
	g_source_unref((GSource *) connection);
	
	return tag;
}

