/* session.c
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

#include "incl.h"


typedef enum 
{
	SESSION_CLIENT = 0,
	SESSION_REMOTE = 1
} SessionLane;

typedef enum
{
	SESSION_AUTH,
	SESSION_REQUEST,
	SESSION_CONNECTING,
	SESSION_CONNECTED
} SessionState;

typedef struct
{
	struct {
		int fd;
		uint8_t buffer[BUFFER_SIZE];
		int start, end;
		struct event *evt;
	} lanes[2];
	
	int state, sid;
	Interface *iface;
	Connector *connector;
	DnsConnector *dns_connector;
	
} Session;

static void session_prepare(Session *session);
void session_authenticator(Session *session);

static void session_note(Session *session)
{
	printf("Session %d: ", session->sid); 
}

static void session_log
	(Session *session, const char *format, ...)
{
	va_list args;
	
	va_start(args, format);
	
	session_note(session);
	
	vprintf(format, args);
	
	printf("\n"); 
	
	va_end(args);
}

//Peeks on data received from client into buffer.
static uint8_t *session_peek(Session *session, int len)
{
	if (session->lanes[SESSION_CLIENT].start + len 
		> session->lanes[SESSION_CLIENT].end)
		return NULL;
	return session->lanes[SESSION_CLIENT].buffer 
		+ session->lanes[SESSION_CLIENT].start;
}

//Returns data received from client into buffer.
static uint8_t *session_read(Session *session, int len)
{
	uint8_t *res = session->lanes[SESSION_CLIENT].buffer 
		+ session->lanes[SESSION_CLIENT].start;
	if (session->lanes[SESSION_CLIENT].start + len 
		> session->lanes[SESSION_CLIENT].end)
		return NULL;
	session->lanes[SESSION_CLIENT].start += len;
	
	session_prepare(session);
	
	return res;
}

//Allocate space on buffer for writing
static uint8_t *session_write_alloc(Session *session, int len)
{
	uint8_t *res = session->lanes[SESSION_REMOTE].buffer 
		+ session->lanes[SESSION_REMOTE].end;
	session->lanes[SESSION_REMOTE].end += len;
	
	session_prepare(session);
	
	return res;
}

//Closes session and frees all resources and tries to flush data
static void session_close(Session *session)
{
	int i;
	
	session_log(session, "Closed");
	
	for (i = 0; i < 2; i++)
	{
		int opposite = 1 - i;
		if (session->lanes[i].evt)
		{
			event_del(session->lanes[i].evt);
			event_free(session->lanes[i].evt);
			session->lanes[i].evt = NULL;
		}
		if (session->lanes[i].fd >= 0)
		{
			flush_add(session->lanes[i].fd, 
	session->lanes[opposite].buffer + session->lanes[opposite].start,
	session->lanes[opposite].end - session->lanes[opposite].start);
		}
	}
	
	if (session->iface)
		interface_close(session->iface);
	if (session->connector)
		connector_cancel(session->connector);
	if (session->dns_connector)
		dns_connector_cancel(session->dns_connector);
	
	free(session);
}

void session_shutdown(Session *session, int side)
{
	if (session->lanes[side].evt)
	{
		event_del(session->lanes[side].evt);
		event_free(session->lanes[side].evt);
		session->lanes[side].evt = NULL;
	}
	if (session->lanes[side].fd >= 0)
	{
		close(session->lanes[side].fd);
		session->lanes[side].fd = -1;
	}
	
	session_close(session);
}

static void session_check(evutil_socket_t fd, short events, void *data)
{
	Session *session = (Session *) data;
	int lane, opposite, io_res;
	
	for (lane = 0; lane < 2; lane++)
	{
		if (session->lanes[lane].fd == fd)
			break;
	}
	
	/*
	session_log(session, "session_check: fd=%d, lane=%d, events=%d\n",
		(int) fd, lane, (int) events);
	*/
	
	opposite = 1 - lane;
		
	//Read data into buffer
	if (events & EV_READ)
	{
		io_res = read(fd,
			session->lanes[lane].buffer + session->lanes[lane].end,
			BUFFER_SIZE - session->lanes[lane].end);
		
		//Error handling
		if (io_res < 0)
		{
			if (! IO_TEMP_ERROR(errno))
				goto destroy;
		}
		else if (io_res == 0)
			goto destroy;
		else
			session->lanes[lane].end += io_res;
	}
	
	//Write from opposite buffer
	if (events & EV_WRITE)
	{
		io_res = write(fd,
			session->lanes[opposite].buffer + session->lanes[opposite].start,
			session->lanes[opposite].end - session->lanes[opposite].start);
		
		//Error handling
		if (io_res < 0)
		{
			if (! IO_TEMP_ERROR(errno))
				goto destroy;
		}
		else if (io_res == 0)
			goto destroy;
		else
			session->lanes[opposite].start += io_res;
		
		//If start of buffer crosses middle,
		//shift the buffer contents to beginning
		if (session->lanes[opposite].start >= (BUFFER_SIZE / 2))
		{
			int j, new_end;
			
			new_end = session->lanes[opposite].end - session->lanes[opposite].start;
			
			for (j = 0; j < new_end; j++)
				session->lanes[opposite].buffer[j] 
					= session->lanes[opposite].buffer
						[j + session->lanes[opposite].start];
			
			session->lanes[opposite].start = 0;
			session->lanes[opposite].end = new_end;
		}
	}
	
	session_prepare(session);
	session_authenticator(session);
	
	return;
destroy:
	session_shutdown(session, lane);
}

static void session_prepare(Session *session)
{
	int lane;
	
	for (lane = 0; lane < 2; lane++)
	{
		int opposite = 1 - lane, events = 0;
		
		if (session->lanes[lane].fd < 0)
			continue;
		
		if ((BUFFER_SIZE - session->lanes[lane].end) > 0)
			events |= EV_READ;
		
		if ((session->lanes[opposite].end - session->lanes[opposite].start) > 0)
			events |= EV_WRITE;
		
		if (session->lanes[lane].evt)
		{
			event_del(session->lanes[lane].evt);
			event_free(session->lanes[lane].evt);
			session->lanes[lane].evt = NULL;
		}
		
		/*
		session_log(session, "session_prepare(%d): events = (%d, %d), lane = (%d, %d), opposite = (%d, %d)",
			lane, events & EV_READ, events & EV_WRITE,
			session->lanes[lane].start, session->lanes[lane].end,
			session->lanes[opposite].start, session->lanes[opposite].end);
		
		if ((session->lanes[lane].start > (BUFFER_SIZE / 2)) || (session->lanes[opposite].start > (BUFFER_SIZE / 2)))
		{
			session_log(session, "Assertion failure");
			abort();
		}
		*/
		
		session->lanes[lane].evt = event_new
			(evbase, session->lanes[lane].fd, events, 
			session_check, session);
		event_add(session->lanes[lane].evt, NULL);
	}
}

//Add a session
void session_create(int fd)
{
	Session *session;
	int i;
	
	session = (Session *) fs_malloc(sizeof(Session));
	
	//Initialize lanes
	for (i = 0; i < 2; i++)
	{
		session->lanes[i].fd = -1;
		session->lanes[i].start = session->lanes[i].end = 0;
		session->lanes[i].evt = NULL;
	}
	
	session->lanes[SESSION_CLIENT].fd = fd;
	
	//Initialize others
	session->state = SESSION_AUTH;
	session->sid = fd;
	session->iface = NULL;
	session->connector = NULL;
	session->dns_connector = NULL;
	
	//Make fd nonbocking
	fd_set_blocking(fd, 0);
	
	//Prepare
	session_prepare(session);
	
	session_log(session, "Created");
}

//Protocol handling code here

static void session_write_socks_error
	(Session *session, int socks_errcode)
{
	uint8_t *buffer;
	
	buffer = session_write_alloc(session, 10);
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
	session_log(session, 
		"SOCKS error code %s sent", socks_reply_to_str(socks_errcode));
	session_close(session);
}

void session_connect_cb(ConnectRes res, void *data)
{
	Session *session = (Session *) data;
	uint8_t *buffer;
	int reply_size;
	Sockaddr saddr;
	
	
	//Remove connector, no longer needed
	session->connector = NULL;
	session->dns_connector = NULL;
	
	//Handle errors
	if (res.socks_status != 0)
	{
		session_write_socks_error(session, res.socks_status);
		return;
	}
	
	//Read address bound
	sockaddr_getsockname(&saddr, res.fd);
	
	//Log message
	session_note(session);
	printf("Connection established, bound address: ");
	sockaddr_write(&saddr, stdout);
	printf("\n");
	
	//Add reply
	if (saddr.x.x.sa_family == AF_INET)
	{
		reply_size = 10;
	}
	else
	{
		reply_size = 22;
	}
	
	buffer = session_write_alloc(session, reply_size);
	buffer[0] = 5;
	buffer[1] = 0;
	buffer[2] = 0;
	if (saddr.x.x.sa_family == AF_INET)
	{
		buffer[3] = 1;
		memcpy(buffer + 4, &(saddr.x.v4.sin_addr), 4);
		memcpy(buffer + 8, &(saddr.x.v4.sin_port), 2);
	}
	else
	{
		buffer[3] = 4;
		memcpy(buffer + 4, &(saddr.x.v6.sin6_addr), 16);
		memcpy(buffer + 20, &(saddr.x.v6.sin6_port), 2);
	}
	
	//Setup session
	session->lanes[SESSION_REMOTE].fd = res.fd;
	session->iface = res.iface;
	session->state = SESSION_CONNECTED;
	fd_set_blocking(res.fd, 0);
	session_prepare(session);
	
	//Assertions
	if (! session->iface)
		abort_with_error("Assertion failure");
}

//Manages all authentication
void session_authenticator(Session *session)
{
	int i;
	uint8_t *buffer;
	
	//SOCKS5 handshake
	if (session->state == SESSION_AUTH)
	{
		int n_methods;
		uint8_t selected = 0xff;
		
		buffer = session_peek(session, 2);
		if (! buffer)
			return;
		
		if (buffer[0] != 5)
		{
			session_log(session, 
				"Unsupported SOCKS version %d", (int) buffer[0]);
			session_close(session);
			return;
		}
		n_methods = buffer[1];
		buffer = session_peek(session, 2 + n_methods);
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
		buffer = session_write_alloc(session, 2);
		buffer[0] = 5;
		buffer[1] = selected;
	
		if (selected == 0xff)
		{
			session_close(session);
			return;
		}
		
		session_read(session, 2 + n_methods);
		session->state = SESSION_REQUEST;
		session_log(session, "Authenticated");
	}
	
	//Session request
	if (session->state == SESSION_REQUEST)
	{
		int socks_errcode = 0;
		
		buffer = session_peek(session, 4);
		if (! buffer)
			return;
		
		if (buffer[0] != 5 || buffer[2] != 0)
			socks_errcode = 1;
		else if (buffer[1] != 1)
			socks_errcode = 7;
		
		if (socks_errcode)
		{
			session_write_socks_error(session, socks_errcode);
			return;
		}
		
		//Read request
		if (buffer[3] == 3)
		{
			//Domain name
			int domain_len;
			char domain[257];
			int port;
			
			buffer = session_peek(session, 5);
			if (! buffer)
				return;
			domain_len = buffer[4];
			
			buffer = session_read
				(session, 4 + 1 + domain_len + 2);
			if (! buffer)
				return;
			
			//Copy out domain name
			memcpy(domain, buffer + 5, domain_len);
			domain[domain_len] = 0;
			
			//Copy out port
			port = ntohs(*((uint16_t *) (buffer + 5 + domain_len)));
			
			session_log(session, 
				"Received request to connect to domain name \"%s:%d\"", 
				domain, port);
			
			//Connect
			session->dns_connector = dns_connector_connect
				(domain, port, session_connect_cb, session);
		}
		else if (buffer[3] == 1)
		{
			//IPv4 address
			Sockaddr addr;
			
			buffer = session_read(session, 4 + 4 + 2);
			if (! buffer)
				return;
			
			memset(&(addr), 0, sizeof(addr));
			addr.x.v4.sin_family = AF_INET;
			memcpy(&(addr.x.v4.sin_addr), buffer + 4, 4);
			memcpy(&(addr.x.v4.sin_port), buffer + 8, 2);
			addr.len = sizeof(struct sockaddr_in);
			
			session_note(session);
			printf("Received request to connect to ipv4 address ");
			sockaddr_write(&addr, stdout);
			printf("\n");
			
			//Connect
			session->connector = connector_connect
				(addr, session_connect_cb, session);
		}
		else if (buffer[3] == 4)
		{
			//IPv6 address
			Sockaddr addr;
			
			buffer = session_read(session, 4 + 16 + 2);
			if (! buffer)
				return;
			
			memset(&(addr), 0, sizeof(addr));
			addr.x.v6.sin6_family = AF_INET6;
			memcpy(&(addr.x.v6.sin6_addr), buffer + 4, 16);
			memcpy(&(addr.x.v6.sin6_port), buffer + 20, 2);
			addr.len = sizeof(struct sockaddr_in);
			
			session_note(session);
			printf("Received request to connect to ipv6 address ");
			sockaddr_write(&addr, stdout);
			printf("\n");
			
			//Connect
			session->connector = connector_connect
				(addr, session_connect_cb, session);
		}
		else
		{
			session_write_socks_error(session, 8);
			return;
		}
		
		session->state = SESSION_CONNECTING;
	}
	
	
}




