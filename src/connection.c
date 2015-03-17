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


#define CHANNEL_BUFFER (1024)

//'Joins' two sockets together
typedef struct
{
	GSource source;
	
	struct {
		GPollFD in_fd;
		char buffer[CHANNEL_BUFFER];
		int start, end;
	} lanes[2];
	
	Interface *interface;
	
} Channel;

//GSourceFuncs::prepare
static gboolean channel_prepare(GSource *source, gint *timeout)
{
	Channel *channel = (Channel *) source;
	int i, opposite;
	
	for (i = 0; i < 2; i++)
		channel->lanes[i].in_fd.events = G_IO_HUP | G_IO_ERR;
	
	for (i = 0; i < 2; i++)
	{
		opposite = 1 - i;
		
		if (channel->lanes[i].end < CHANNEL_BUFFER)
			channel->lanes[i].in_fd.events |= G_IO_IN;
		if (channel->lanes[i].start < channel->lanes[i].end)
			channel->lanes[opposite].in_fd.events |= G_IO_OUT;
	}
	
	*timeout = -1;
	return FALSE;
}

//GSourceFuncs::check
static gboolean channel_check(GSource *source)
{
	Channel *channel = (Channel *) source;
	int i;
	
	for (i = 0; i < 2; i++)
		if (channel->lanes[i].in_fd.revents)
			return TRUE;
	
	return FALSE;
}

//GSourceFuncs::dispatch
static gboolean channel_dispatch
	(GSource *source, GSourceFunc callback, gpointer user_data)
{
	Channel *channel = (Channel *) source;
	int i, opposite, io_res;
	
	//Check for errors
	for (i = 0; i < 2; i++)
		if (channel->lanes[i].in_fd.revents & (G_IO_HUP | G_IO_ERR))
			return G_SOURCE_REMOVE;
	
	for (i = 0; i < 2; i++)
	{
		opposite = 1 - i;
		
		//Read data into buffer
		if (channel->lanes[i].in_fd.revents & G_IO_IN)
		{
			io_res = read(channel->lanes[i].in_fd.fd,
				channel->lanes[i].buffer + channel->lanes[i].end,
				CHANNEL_BUFFER - channel->lanes[i].end);
			
			//Error handling
			if (io_res < 0)
			{
				if (! IO_TEMP_ERROR(errno))
					return G_SOURCE_REMOVE;
			}
			else if (io_res == 0)
				return G_SOURCE_REMOVE;
			
			channel->lanes[i].end += io_res;
		}
		
		//Write it to opposite lane
		if (channel->lanes[opposite].in_fd.revents & G_IO_OUT)
		{
			io_res = write(channel->lanes[opposite].in_fd.fd,
				channel->lanes[i].buffer + channel->lanes[i].start,
				channel->lanes[i].end - channel->lanes[i].start);
			
			//Error handling
			if (io_res < 0)
			{
				if (! IO_TEMP_ERROR(errno))
					return G_SOURCE_REMOVE;
			}
			else if (io_res == 0)
				return G_SOURCE_REMOVE;
			
			channel->lanes[i].start += io_res;
			
			//If start of buffer crosses middle,
			//shift the buffer contents to beginning
			if (channel->lanes[i].start >= (CHANNEL_BUFFER / 2))
			{
				int j, new_end;
				
				new_end = channel->lanes[i].end - channel->lanes[i].start;
				
				for (j = 0; j < new_end; j++)
					channel->lanes[i].buffer[j] 
						= channel->lanes[i].buffer
							[j + channel->lanes[i].start];
				
				channel->lanes[i].start = 0;
				channel->lanes[i].end = new_end;
			}
		}
	}
	
	return G_SOURCE_CONTINUE;
}

//GSourceFuncs::finalize
static void channel_finalize(GSource *source)
{
	Channel *channel = (Channel *) source;
	int i;
	
	for (i = 0; i < 2; i++)
		close(channel->lanes[i].in_fd.fd);
	
	interface_close(channel->interface);
}

static GSourceFuncs channel_funcs =
{
	channel_prepare,
	channel_check,
	channel_dispatch,
	channel_finalize
};

//Add a channel to default context
guint channel_add(int fd1, int fd2, Interface *interface)
{
	Channel *channel;
	guint tag;
	
	//Create GSource
	channel = (Channel *) g_source_new(&channel_funcs, sizeof(Channel));
	
	//Initialize lanes
	channel->lanes[0].in_fd.fd = fd1;
	g_source_add_poll((GSource *) channel, &(channel->lanes[0].in_fd));
	channel->lanes[1].in_fd.fd = fd2;
	g_source_add_poll((GSource *) channel, &(channel->lanes[1].in_fd));
	
	//Add interface
	channel->interface = interface;
	
	//Add to default context
	tag = g_source_attach((GSource *) channel, NULL);
	g_source_unref((GSource *) channel);
	
	return tag;
}


typedef struct
{
	int fd;
	InterfaceManager *manager;
} ConnectionData;



gpointer connection_main(gpointer thread_data)
{
	ConnectionData *data = (ConnectionData *) thread_data;
	int fd;
	InterfaceManager *manager;
	Interface *interface = NULL;
	int remote_fd;
	
	int io_res, i;
	guint8 buffer[257];
	
	fd = data->fd;
	manager = data->manager;
	g_slice_free(ConnectionData, data);
	
	//Macro to report messages
#define msg(...) \
	do {\
		printf("Connection %d: ", fd); \
		printf(__VA_ARGS__); \
		printf("\n"); \
	} while(0)
	
	//Read SOCKS5 handshake
	//Version, nmethods
	io_res = read(fd, buffer, 2);
	if (io_res < 2)
	{
		msg("IO error while reading SOCKS handshake");
		goto fail;
	}
	if (buffer[0] != 5)
	{
		msg("Unsupported SOCKS version %d", (int) buffer[0]);
		goto fail;
	}
	
	{
		//Methods
		int nmethods = buffer[1];
		guint8 selected;
		
		io_res = read(fd, buffer, nmethods);
		if (io_res < nmethods)
		{
			msg("IO error while reading SOCKS handshake");
			goto fail;
		}
		
		//Select method 0 only
		selected = 0xff;
		for (i = 0; i < nmethods; i++)
		{
			if (buffer[i] == 0)
			{
				selected = 0;
				break;
			}
		}
		
		//Send response
		buffer[0] = 5;
		buffer[1] = selected;
		io_res = write(fd, buffer, 2);
		if (io_res < 2)
		{
			msg("IO error while writing resonse");
			goto fail;
		}
		
		if (selected != 0)
		{
			msg("Client does not support method 0");
			goto fail;
		}
	}
	
	//Read request
	io_res = read(fd, buffer, 4);
	if (io_res < 4)
	{
		msg("IO error while reading request");
		goto fail;
	}
	if (buffer[3] == 2)
	{
		//Domain name
		
		//Read domain name
		io_res = read(fd, buffer + 4, 1);
		if (io_res < 1)
		{
			msg("IO error while reading domain name length");
			goto fail;
		}
		io_res = read(fd, buffer + 5, buffer[4]);
		if (io_res < buffer[4])
		{
			msg("IO error while reading domain name");
			goto fail;
		}
		
		//Read port
		io_res = read(fd, buffer + 5 + buffer[4], 2);
		if (io_res < 2)
		{
			msg("IO error while reading port");
			goto fail;
		}
	}
	else if (buffer[3] == 1)
	{
		//IPv4 address
		io_res = read(fd, buffer + 4, 1);
		if (io_res < 1)
		{
			msg("IO error while reading domain name length");
			goto fail;
		}
	}
	else if (buffer[3] == 3)
	{
		//IPv6 address
		addr_len = sizeof(struct sockaddr_in6);
		
		//Read address
		addr.inet6.sin6_family = AF_INET6;
		io_res = read(fd, (void *) &(addr.inet6.sin6_addr), 16);
		if (io_res < 16)
		{
			msg("IO error while reading IPv6 address");
			goto fail;
		}
		io_res = read(fd, (void *) &(addr.inet6.sin6_port), 2);
		if (io_res < 2)
		{
			msg("IO error while reading IPv6 port");
			goto fail;
		}
		
		//Get an interface
		interface = interface_manager_get(manager, INTERFACE_INET6);
	}
	
	remote_fd = interface_open(interface);
	if (connect(remote_fd, &(addr.addr), addr_len) < 0)
	{
		close(remote_fd);
		interface_close(interface);
		remote_fd = -1;
	}
	}
	
	
	if (buffer[0] != 5 || buffer[1] != 1 || buffer[2] != 0)
	{
		msg("Invalid/unsupported request (%d, %d, %d)",
			(int) buffer[0], (int) buffer[1], (int) buffer[2]);
		goto fail;
	}
	
	//Read address type
	if (buffer[3] == 2)
	{
		//Domain name
		int domain_len;
		int resolve_status;
		uint16_t port;
		char port_str[16];
		struct addrinfo hints, *addrs, *iter;
		int addr_type;
		
		//Read domain name
		io_res = read(fd, buffer, 1);
		if (io_res < 1)
		{
			msg("IO error while reading domain name length");
			goto fail;
		}
		domain_len = buffer[0];
		io_res = read(fd, buffer, domain_len);
		if (io_res < domain_len)
		{
			msg("IO error while reading domain name");
			goto fail;
		}
		buffer[domain_len] = 0;
		
		//Read port
		io_res = read(fd, &port, 2);
		if (io_res < 2)
		{
			msg("IO error while reading port");
			goto fail;
		}
		
		//Resolve it
		sprintf(port_str, "%d", (int) ntohs(port));
		hints.ai_flags = 0;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = 0;
		hints.ai_addrlen = 0;
		hints.ai_addr = NULL;
		hints.ai_canonname = NULL;
		hints.ai_next = NULL;
		
		resolve_status = getaddrinfo(buffer, port_str, &hints, &addrs);
		
		//Error checking
		if (resolve_status != 0)
		{
			msg("Could not resolve \"%s\": %s", 
				gai_strerror(resolve_status));
			goto fail;
		}
		
		//Get interface
		addr_type = 0;
		for (iter = addrs; iter; iter = iter->next)
		{
			if (iter->ai_socktype == AF_INET)
				addr_type |= INTERFACE_INET;
			else if (iter->ai_socktype == AF_INET6)
				addr_type |= INTERFACE_INET6;
		}
		interface = interface_manager_get(manager, addr_type);
		
		//Connect
		remote_fd = -1;
		for (iter = addrs; iter; iter = iter->next)
		{
			if (interface->addr.sa_family == iter->ai_socktype)
			{
				remote_fd = interface_open(interface);
				
				if (connect(remote_fd, iter->ai_addr, iter->ai_addrlen) >= 0)
					break;
				
				close(remote_fd);
				interface_close(interface);
				remote_fd = -1;
			}
		}
		
		//Free data
		freeaddrinfo(addrs);
	}
	else
	{
		//IP address
		union {
			struct sockaddr addr;
			struct sockaddr_in inet;
			struct sockaddr_in6 inet6;
		} addr;
		socklen_t addr_len;
		
		if (buffer[3] == 1)
		{
			//IPv4 address
			addr_len = sizeof(struct sockaddr_in);
			
			//Read address
			addr.inet.sin_family = AF_INET;
			io_res = read(fd, (void *) &(addr.inet.sin_addr), 4);
			if (io_res < 4)
			{
				msg("IO error while reading IPv4 address");
				goto fail;
			}
			io_res = read(fd, (void *) &(addr.inet.sin_port), 2);
			if (io_res < 2)
			{
				msg("IO error while reading IPv4 port");
				goto fail;
			}
			
			//Get an interface
			interface = interface_manager_get(manager, INTERFACE_INET);
		}
		else if (buffer[3] == 3)
		{
			//IPv6 address
			addr_len = sizeof(struct sockaddr_in6);
			
			//Read address
			addr.inet6.sin6_family = AF_INET6;
			io_res = read(fd, (void *) &(addr.inet6.sin6_addr), 16);
			if (io_res < 16)
			{
				msg("IO error while reading IPv6 address");
				goto fail;
			}
			io_res = read(fd, (void *) &(addr.inet6.sin6_port), 2);
			if (io_res < 2)
			{
				msg("IO error while reading IPv6 port");
				goto fail;
			}
			
			//Get an interface
			interface = interface_manager_get(manager, INTERFACE_INET6);
		}
		
		remote_fd = interface_open(interface);
		if (connect(remote_fd, &(addr.addr), addr_len) < 0)
		{
			close(remote_fd);
			interface_close(interface);
			remote_fd = -1;
		}
	}
	
	
		
fail:
	close(fd);
	return NULL;
}

void connection_run(int fd, Interface *interface)
{
	ConnectionData *data = g_slice_new(ConnectionData);
	
	data->fd = fd;
	data->interface = interface;
	
	g_thread_unref(g_thread_new
		(NULL, connection_main, (gpointer) data));
}
