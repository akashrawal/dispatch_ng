/* connector.c
 * Asynchronous connection manager
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

//Errors
define_static_error(connector_error_no_addrs, "No usable addresses");

typedef struct _AddrList AddrList;
struct _AddrList
{
	AddrList *next;
	SocketAddress addr;
};

struct _Connector
{
	//Connection subsystem
	AddrList *addrs;
	int final;
	SocketHandle hd;
	Interface *iface;
	struct event *event;
	const Error *last_error;

	//DNS subsystem
	uint16_t port;
	DnsRequest *dns_request;
	
	//Callback subsystem
	int returned;
	ConnectRes res;
	ConnectorCB cb;
	void *cb_data;
	struct event *cb_event;
	int idle_mode;
};

//Forward declarations
static void connector_return(Connector *connector, ConnectRes res);

//Connection subsystem
static void conn_init(Connector *connector)
{
	//Only nonzero members need to be set.
}

static void conn_free(Connector *connector)
{
	//Remove connection event
	if (connector->event)
	{
		event_del(connector->event);
		event_free(connector->event);
	}

	//Cancel ongoing connection if any
	if (connector->iface)
	{
		socket_handle_close(connector->hd);
		interface_close(connector->iface);
	}

	//Free all addresses
	AddrList *bak;
	while (connector->addrs)
	{
		bak = connector->addrs->next;
		free(connector->addrs);
		connector->addrs = bak;
	}
}

static void conn_set_last_error(Connector *connector, const Error *e)
{
	error_handle(connector->last_error);
	connector->last_error = e;
}

static void conn_start(Connector *connector);

static void conn_connect_cb(evutil_socket_t fd, short events, void *data)
{
	Connector *connector = (Connector *) data;
	ConnectRes res;

	//Remove event
	event_del(connector->event);
	event_free(connector->event);
	connector->event = NULL;

	//Get the connection information and vacate connector
	res.e = socket_handle_get_status(connector->hd);
	res.hd = connector->hd;
	res.iface = connector->iface;
	connector->iface = NULL;

	if (res.e)
	{
		conn_set_last_error(connector, res.e);

		//Close socket
		socket_handle_close(res.hd);
		interface_close(res.iface);

		//Connect to next address if available
		conn_start(connector);
	}
	else
	{
		connector_return(connector, res);
	}
}

//Start connecting to the next address on the list.
//Call this function after adding addressing or finalizing.
static void conn_start(Connector *connector)
{
	//If returned do nothing
	if (connector->returned)
		return;

	//If a connection is already in progress don't do anything.
	//Callback function will take care of this.
	if (connector->event)
		return;

	while (connector->addrs)
	{
		ConnectRes res;
		SocketAddress addr;
		const Error *e;

		abort_if_fail(!connector->event, "Assertion failure");

		//Pop an address
		{
			AddrList *list_ptr = connector->addrs;
			connector->addrs = list_ptr->next;
			addr = list_ptr->addr;
			free(list_ptr);
		}

		//Open a suitable interface
		e = balancer_open_iface(addr.host.type, &connector->iface, &connector->hd);
		if (e)
		{
			conn_set_last_error(connector, e);
			continue;
		}
		abort_if_fail(connector->iface, "Assertion failure");

		//Enable non-blocking
		e = socket_handle_set_blocking(connector->hd, 0);
		abort_if_fail(!e,
				"socket_handle_set_blocking(hd, 0): %s", error_desc(e));

		//Connect
		e = socket_handle_connect(connector->hd, addr);
		if (e)
		{
			if (e->type == socket_error_in_progress)
			{
				//In progress, add event and exit
				error_handle(e);
				connector->event = socket_handle_create_event
					(connector->hd, EV_WRITE, conn_connect_cb, connector);
				event_add(connector->event, NULL);
				break;
			}
			else
			{
				//Fail, try next address
				conn_set_last_error(connector, e);
				interface_close(connector->iface);
				connector->iface = NULL;
				continue;
			}
		}

		//Success, without even waiting
		//Get the connection information and vacate connector
		res.e = NULL;
		res.hd = connector->hd;
		res.iface = connector->iface;
		connector->iface = NULL;

		connector_return(connector, res);
		return;
	}

	abort_if_fail(connector->event || (! connector->addrs),
			"Assertion failure");

	//Return error if no addresses left and is final
	if (! connector->addrs && connector->final)
	{
		ConnectRes res;
		memset(&res, 0, sizeof(ConnectRes));
		res.e = connector->last_error;
		connector->last_error = NULL;

		if (! res.e)
			res.e = connector_error_no_addrs_instance;

		connector_return(connector, res);
		return;
	}

	abort_if_fail((connector->iface ? 1 : 0) == (connector->event ? 1 : 0),
			"Assertion failure");
}

static void conn_add_addr(Connector *connector, SocketAddress addr)
{
	AddrList *new_addr;

	abort_if_fail(! connector->final,
			"Assertion failure: "
			"No address can be added after calling conn_set_final()");

	//Add address to the list
	new_addr = fs_malloc(sizeof(AddrList));
	new_addr->addr = addr;
	new_addr->next = connector->addrs;
	connector->addrs = new_addr;

	//If not already connecting, add an event
	conn_start(connector);
}

static void conn_set_final(Connector *connector)
{
	connector->final = 1;

	//Return error if there are no more addresses to connect
	conn_start(connector);
}

////
//DNS subsystem
static void dns_init(Connector *connector)
{
	//Only nonzero members need to be set.
}

static void dns_free(Connector *connector)
{
	if (connector->dns_request)
	{
		dns_request_destroy(connector->dns_request);
		connector->dns_request = NULL;
	}
}

static void dns_cb
(const Error *e, size_t n_addrs, SocketAddress *addrs, void *data)
{
	size_t i;
	Connector *connector = (Connector *) data;

	if (e)
	{
		conn_set_last_error(connector, e);	
	}
	else
	{
		for (i = 0; i < n_addrs; i++)
			conn_add_addr(connector, addrs[i]);
	}
	if (addrs)
		free(addrs);
	conn_set_final(connector);
}

static void dns_start(Connector *connector, const char *addr, uint16_t port)
{
	//Set the port
	connector->port = port;

	connector->dns_request = dns_request_resolve
		(addr, port, balancer_get_available_types(), dns_cb, connector);
}

//Rest of the connector
void connector_return_idle(evutil_socket_t fd, short events, void *data)
{
	Connector *connector = (Connector *) data;

	event_free(connector->cb_event);
	connector->cb_event = NULL;
	(* connector->cb)(connector->res, connector->cb_data);
}
static void connector_return(Connector *connector, ConnectRes res)
{
	abort_if_fail(connector->returned == 0, "Assertion failure");
	connector->returned++;

	if (connector->idle_mode)
	{
		connector->res = res;
		abort_if_fail(! connector->cb_event, 
				"Assertion failure");
		connector->cb_event = event_new(evbase, -1, 0,
				connector_return_idle, connector);
		event_active(connector->cb_event, 0, 0);
	}
	else
	{
		(* connector->cb)(res, connector->cb_data);
	}
}

//Creates an idle connector
static Connector *connector_create(ConnectorCB cb, void *data)
{
	Connector *connector = (Connector *) fs_malloc(sizeof(Connector));

	memset(connector, 0, sizeof(Connector));

	connector->cb = cb;
	connector->cb_data = data;
	connector->idle_mode = 1;

	//Initialize all subsystems
	conn_init(connector);
	dns_init(connector);	

	return connector;
}

//Free's a connector and shuts down all associated events
void connector_destroy(Connector *connector)
{
	conn_free(connector);
	dns_free(connector);

	if (connector->cb_event)
		event_free(connector->cb_event);

	free(connector);
}

//Connect to a remote socket
Connector *connector_connect
	(SocketAddress addr, ConnectorCB cb, void *data)
{
	Connector *connector = connector_create(cb, data);

	conn_add_addr(connector, addr);
	conn_set_final(connector);

	connector->idle_mode = 0;

	return connector;
}

//Connect by resolving name by DNS
Connector *connector_connect_dns
	(const char *name, uint16_t port, ConnectorCB cb, void *data)
{
	Connector *connector = connector_create(cb, data);

	dns_start(connector, name, port);

	connector->idle_mode = 0;

	return connector;
}
	
