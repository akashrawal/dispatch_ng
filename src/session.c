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

struct _Session
{
	struct {
		SocketHandle hd;
		int hd_valid;
		uint8_t buffer[BUFFER_SIZE];
		int start, end;
		struct event *evt;
	} lanes[2];
	
	int state, prev_state;
	unsigned int sid;
	Interface *iface;
	Connector *connector;

	SessionStateChangeCB cb;
	void *cb_data;
};

//Forward declarations
static void session_prepare(Session *session);
void session_authenticator(Session *session);

//Logging functions
static void session_log
	(Session *session, const char *format, ...)
{
	va_list args;
	
	va_start(args, format);
	
	printf("Session %u: ", session->sid); 
	
	vprintf(format, args);
	
	printf("\n"); 
	
	va_end(args);
}

//State change functions
//Does not call any callbacks
static void session_set_state(Session *session, SessionState state)
{
	session->state = state;

	struct
	{
		SessionState state;
		const char *str;
	} states[] = {
#define entry(x) {x, #x}
		entry(SESSION_AUTH),
		entry(SESSION_REQUEST),
		entry(SESSION_CONNECTING),
		entry(SESSION_CONNECTED),
		entry(SESSION_SHUTDOWN),
		entry(SESSION_CLOSED),
#undef entry
		{ 0, NULL }
	};

	int i;
	for (i = 0; states[i].str; i++)
	{
		if (states[i].state == state)
			break;
	}

	session_log(session, "Session entered state %s", states[i].str);
}

//Buffer management functions

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
	
	return res;
}

//Allocate space on buffer for writing
static uint8_t *session_write_alloc(Session *session, int len)
{
	uint8_t *res = session->lanes[SESSION_REMOTE].buffer 
		+ session->lanes[SESSION_REMOTE].end;
	session->lanes[SESSION_REMOTE].end += len;
	
	return res;
}

//Event management

//Responds to IO events
static void session_check(evutil_socket_t fd, short events, void *data)
{
	SocketHandle hd;
	Session *session = (Session *) data;
	int lane, opposite;
	size_t io_res;
	const Error *e;
	int shutdown_needed = 0;
	
	//Find the socket on which we received event
	for (lane = 0; lane < 2; lane++)
	{
		if (socket_handle_equal_with_native(session->lanes[lane].hd, fd))
			break;
	}

	hd = session->lanes[lane].hd;
	opposite = 1 - lane;
	
	//Assertions
	abort_if_fail(session->state != SESSION_CLOSED,
			"Assertion failed");
	abort_if_fail(session->state != SESSION_CONNECTED
			&& session->state != SESSION_SHUTDOWN
			? lane != SESSION_REMOTE : 1,
			"Assertion failed");
	abort_if_fail(session->state == SESSION_CONNECTED
			?  session->lanes[SESSION_REMOTE].hd_valid
			&& session->lanes[SESSION_REMOTE].hd_valid : 1,
			"Assertion failed");
	
		
	//Read data into buffer
	if (events & EV_READ)
	{
		e = socket_handle_read(hd, 
			session->lanes[lane].buffer + session->lanes[lane].end,
			BUFFER_SIZE - session->lanes[lane].end,
			&io_res);
		
		//Error handling
		if (e)
		{
			if (e->type != socket_error_again)
			{
				shutdown_needed = 1;
				session_log(session, 
						"Error %s", error_desc(e));
			}
			error_handle(e);
			e = NULL;
		}
		else if (io_res == 0)
		{
			session_log(session, "EOF encountered");
			shutdown_needed = 1;
		}
		else
			session->lanes[lane].end += io_res;
	}
	
	//Write from opposite buffer
	if (events & EV_WRITE)
	{
		e = socket_handle_write(hd, 
			session->lanes[opposite].buffer + session->lanes[opposite].start,
			session->lanes[opposite].end - session->lanes[opposite].start,
			&io_res);

		abort_if_fail(io_res != 0, "Assertion failure");
		
		//Error handling
		if (e)
		{
			if (e->type != socket_error_again)
			{
				shutdown_needed = 1;
				session_log(session, 
						"Error %s", error_desc(e));
			}
			error_handle(e);
			e = NULL;
		}
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

	//Close the socket handle if anything failed
	//If anything failed, then close socket handle and interface, if valid
	if (shutdown_needed)
	{
		socket_handle_close(hd);
		session->lanes[lane].hd_valid = 0;
		if (lane == SESSION_REMOTE)
		{
			interface_close(session->iface);
			session->iface = NULL;
		}
		if (session->state != SESSION_SHUTDOWN)
			session_set_state(session, SESSION_SHUTDOWN);
	}

	//If session is in shutdown state and all buffers are empty, then
	//enter closed state.
	if (session->state == SESSION_SHUTDOWN)
	{
		int cond = 0, i;
		for(i = 0; i < 2; i++)
		{
			cond += session->lanes[i].hd_valid
				? session->lanes[i].end - session->lanes[i].start
				: 0;
		}
		if (!cond)
			session_set_state(session, SESSION_CLOSED);
	}

	session_authenticator(session);

	//Call prepare function
	session_prepare(session);
}

//Prepares events for socket handles as per buffer state
static void session_prepare(Session *session)
{
	int lane;
	
	for (lane = 0; lane < 2; lane++)
	{
		int opposite = 1 - lane;
		short events = 0;
		
		//Create needed flags
		if (session->state != SESSION_CLOSED && session->lanes[lane].hd_valid)
		{
			if (session->state != SESSION_SHUTDOWN)
				if ((BUFFER_SIZE - session->lanes[lane].end) > 0)
					events |= EV_READ;
			
			if ((session->lanes[opposite].end
						- session->lanes[opposite].start) > 0)
				events |= EV_WRITE;
		}

		struct event *evt = session->lanes[lane].evt;

		//Delete event if not needed or change is needed
		if (evt && (!events || events != event_get_events(evt)))
		{
			event_del(evt);
			event_free(evt);
			evt = NULL;
		}

		//Create a new event if needed
		if (events && !evt)
		{
			evt = socket_handle_create_event(session->lanes[lane].hd,
					events | EV_PERSIST, session_check, session);
			event_add(evt, NULL);
		}

		session->lanes[lane].evt = evt;
	}

	//Assertion
	abort_if_fail(session->lanes[SESSION_CLIENT].evt
			|| session->lanes[SESSION_REMOTE].evt
			|| session->connector
			? session->state != SESSION_CLOSED
			: session->state == SESSION_CLOSED,
			"Assertion failure (session entered dead state)");

	abort_if_fail(session->state == SESSION_CONNECTED 
			? session->lanes[SESSION_CLIENT].evt
				|| session->lanes[SESSION_REMOTE].evt
			: 1,
			"Assertion failure (session entered semidead state)");

	//Call callbacks
	if (session->state != session->prev_state)
	{
		session->prev_state = session->state;
		
		if (session->cb)
			(* session->cb)(session, session->state, session->cb_data);
	}
}


//Protocol handling code here
static void session_write_socks_error_base(Session *session, int socks_errcode)
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
}

//This function calls user callback, so beware of reentrancy issues.
static void session_write_socks_error(Session *session, int socks_errcode)
{
	session_write_socks_error_base(session, socks_errcode);
	session_log(session, 
		"SOCKS error code %s sent", socks_reply_to_str(socks_errcode));
	session_set_state(session, SESSION_SHUTDOWN);
}

//This function calls user callback, so beware of reentrancy issues.
static void session_write_connect_error(Session *session, const Error *e)
{
	int socks_errcode = socks_reply_from_error(e);
	session_write_socks_error_base(session, socks_errcode);
	session_log(session, 
		"SOCKS error code %s sent because of error: %s",
		socks_reply_to_str(socks_errcode),
		error_desc(e));
	session_set_state(session, SESSION_SHUTDOWN);
}

void session_connect_cb(ConnectRes res, void *data)
{
	Session *session = (Session *) data;
	uint8_t *buffer;
	int reply_size;
	SocketAddress addr;
	char addr_tostring[ADDRESS_MAX_LEN];
	const Error *e;
	
	//Remove connector, no longer needed
	connector_destroy(session->connector);
	session->connector = NULL;
	
	//Handle errors
	if (res.e)
	{
		session_write_connect_error(session, res.e);
		error_handle(res.e);
	}
	else
	{

		//Read address bound
		e = socket_handle_getsockname(res.hd, &addr);
		if (e)
		{
			abort_with_error("socket_handle_getsockname(): %s",
					error_desc(e));
		}
		
		//Log message
		socket_address_to_str(addr, addr_tostring);
		session_log(session,
				"Connection established, bound address: %s", addr_tostring);
		
		//Add reply
		if (addr.host.type == NETWORK_INET)
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
		if (addr.host.type == NETWORK_INET)
		{
			buffer[3] = 1;
			memcpy(buffer + 4, addr.host.ip, 4);
			memcpy(buffer + 8, &(addr.port), 2);
		}
		else
		{
			buffer[3] = 4;
			memcpy(buffer + 4, addr.host.ip, 16);
			memcpy(buffer + 20, &(addr.port), 2);
		}
		
		//Setup session
		socket_handle_set_blocking(res.hd, 0);
		session->lanes[SESSION_REMOTE].hd = res.hd;
		session->lanes[SESSION_REMOTE].hd_valid = 1;
		session->iface = res.iface;
		session_set_state(session, SESSION_CONNECTED);
		
		//Assertions
		if (! session->iface)
			abort_with_error("Assertion failure");
	}
	session_prepare(session);
}

//TODO: Separate SOCKS protocol details from semantics

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
			session_set_state(session, SESSION_SHUTDOWN);
			return;
		}
		n_methods = buffer[1];
		buffer = session_peek(session, 2 + n_methods);
		if (! buffer)
			return;
		
		//Select method 0 only
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
			session_set_state(session, SESSION_SHUTDOWN);
			return;
		}
		
		session_read(session, 2 + n_methods);
		session_set_state(session, SESSION_REQUEST);
		session_log(session, "Authenticated");
	}
	
	//Session request
	if (session->state == SESSION_REQUEST)
	{
		int socks_errcode = 0;
		
		buffer = session_peek(session, 4);
		if (! buffer)
			return;
		
		//Verify protocol version and command
		if (buffer[0] != 5 || buffer[2] != 0)
			socks_errcode = SOCKS_REPLY_GEN;
		else if (buffer[1] != SOCKS_CMD_CONNECT)
			socks_errcode = SOCKS_REPLY_CMD;
		
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
			session->connector = connector_connect_dns
				(domain, port, session_connect_cb, session);
				
		}
		else if (buffer[3] == 1)
		{
			//IPv4 address
			SocketAddress addr;
			char addr_tostring[ADDRESS_MAX_LEN];

			buffer = session_read(session, 4 + 4 + 2);
			if (! buffer)
				return;

			addr.host.type = NETWORK_INET;
			memcpy(addr.host.ip, buffer + 4, 4);
			memcpy(&(addr.port), buffer + 8, 2);

			socket_address_to_str(addr, addr_tostring);
			session_log(session,
					"Received request to connect to ipv4 address %s",
					addr_tostring);
			
			//Connect
			session->connector = connector_connect
				(addr, session_connect_cb, session);
		}
		else if (buffer[3] == 4)
		{
			//IPv6 address
			SocketAddress addr;
			char addr_tostring[ADDRESS_MAX_LEN];
			
			buffer = session_read(session, 4 + 16 + 2);
			if (! buffer)
				return;
			
			addr.host.type = NETWORK_INET6;
			memcpy(addr.host.ip, buffer + 4, 16);
			memcpy(&(addr.port), buffer + 20, 2);
			
			socket_address_to_str(addr, addr_tostring);
			session_log(session,
					"Received request to connect to ipv6 address %s",
					addr_tostring);
			
			//Connect
			session->connector = connector_connect
				(addr, session_connect_cb, session);
		}
		else
		{
			session_write_socks_error(session, SOCKS_REPLY_ATYPE);
			return;
		}
		
		session_set_state(session, SESSION_CONNECTING);
	}
	
	
}

static unsigned int session_counter = 0;

//Create a session
Session *session_create(SocketHandle hd)
{
	Session *session;
	int i;
	
	session = (Session *) fs_malloc(sizeof(Session));
	
	//Initialize lanes
	for (i = 0; i < 2; i++)
	{
		session->lanes[i].hd_valid = 0;
		session->lanes[i].start = session->lanes[i].end = 0;
		session->lanes[i].evt = NULL;
	}
	
	session->lanes[SESSION_CLIENT].hd = hd;
	session->lanes[SESSION_CLIENT].hd_valid = 1;

	
	//Initialize others
	//TODO: session ID
	session->sid = session_counter++;
	session->iface = NULL;
	session->connector = NULL;
	session->cb = NULL;
	session->cb_data = NULL;
	session->prev_state = SESSION_CLOSED;
	session->state = SESSION_CLOSED;
	
	//Make fd nonbocking
	socket_handle_set_blocking(hd, 0);
	
	//Log message
	session_log(session, "Created");

	//Start session
	session_set_state(session, SESSION_AUTH);	
	session_prepare(session);

	return session;
}

//Gets current state
SessionState session_get_state(Session *session)
{
	return session->state;
}

//Shuts down session by trying to send all unsent data
void session_shutdown(Session *session)
{
	abort_if_fail(session->state != SESSION_SHUTDOWN
			&& session->state != SESSION_CLOSED,
			"Session already shutting down");
	
	session_log(session, "Closed");

	if (session->connector)
		connector_destroy(session->connector);

	session_set_state(session, SESSION_SHUTDOWN);	
}

void session_destroy(Session *session)
{
	int i;

	for (i = 0; i < 2; i++)
	{
		if (session->lanes[i].hd_valid)
			socket_handle_close(session->lanes[i].hd);
		if (session->lanes[i].evt)
			event_free(session->lanes[i].evt);
	}

	if (session->iface)
		interface_close(session->iface);
	if (session->connector)
		connector_destroy(session->connector);

	free(session);
}

void session_set_callback
	(Session *session, SessionStateChangeCB cb, void *data)
{
	session->cb = cb;
	session->cb_data = data;
}

