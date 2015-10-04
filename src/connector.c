/* connector.c
 * Asynchronous connection manager
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

void connector_cancel(Connector *connector)
{
	event_del(connector->evt);
	event_free(connector->evt);
	if (connector->iface)
	{
		close(connector->fd);
		interface_close(connector->iface);
	}
	free(connector);
}

static void connector_check(evutil_socket_t fd, short events, void *data)
{
	Connector *connector = (Connector *) data;
	ConnectRes cres;
	
	if (events & (EV_READ | EV_WRITE))
	{
		int res;
		socklen_t optlen = sizeof(res);
		
		if (getsockopt(connector->fd, SOL_SOCKET, SO_ERROR, 
			&res, &optlen) < 0)
		{
			abort_with_liberror("getsockopt()");
		}
		
		if (res == 0)
		{
			//Success
			cres.socks_status = 0;
			cres.fd = connector->fd;
			cres.iface = connector->iface;
			connector->iface = NULL;
		}
		else
		{
			//Failed
			if (res == ENETUNREACH)
				cres.socks_status = 3;
			else if (res == ECONNREFUSED)
				cres.socks_status = 5;
			else if (res == ETIMEDOUT)
				cres.socks_status = 6;
			else 
				cres.socks_status = 1;
			
			cres.fd = -1;
			cres.iface = NULL;
		}
	}
	else
	{
		abort_with_error("Unforseen circumstances!");
	}
	
	//Call the callback
	(* connector->cb) (cres, connector->data);
	
	//Cleanup
	connector_cancel(connector);
}

static void connector_no_iface_delayed
	(evutil_socket_t fd, short events, void *data)
{
	Connector *connector = (Connector *) data;
	ConnectRes cres;
	
	cres.socks_status = 3;
	cres.fd = -1;
	cres.iface = NULL;
	
	//Call the callback
	(* connector->cb) (cres, connector->data);
	
	//Cleanup
	connector_cancel(connector);
}


Connector *connector_connect
	(Sockaddr saddr, ConnectorCB cb, void *data)
{
	Connector *connector = (Connector *) fs_malloc(sizeof(Connector));
	int mask;
	
	//Create socket
	switch (saddr.x.x.sa_family)
	{
		case AF_INET: mask = ADDRESS_INET; break;
		default: mask = ADDRESS_INET6;
	}
	connector->iface = balancer_select(mask);
	connector->fd = -1;
	if (connector->iface)
	{
		connector->fd = interface_open(connector->iface);
	}
	if (connector->fd >= 0)
	{
		//Connect
		if (connect(connector->fd, &(saddr.x.x), saddr.len) < 0)
		{
			if (errno != EINPROGRESS)
				abort_with_liberror("connect()");
		}
		
		//Setup events
		connector->evt = event_new(evbase, connector->fd, 
			EV_READ | EV_WRITE, connector_check, connector);
		event_add(connector->evt, NULL);
	}
	else
	{
		connector->fd = -1;
		connector->evt = event_new(evbase, -1, 0, 
			connector_no_iface_delayed, connector);
		event_active(connector->evt, 0, 0);
	}
	
	connector->cb = cb;
	connector->data = data;
	
	return connector;
}
	


struct evdns_base *dns_base = NULL;

void dns_connector_cancel(DnsConnector *dc)
{
	if (dc->addrs)
		evutil_freeaddrinfo(dc->addrs);
	if (dc->dns_query)
		evdns_getaddrinfo_cancel(dc->dns_query);
	if (dc->connector)
		connector_cancel(dc->connector);
	
	free(dc);
}

void dns_connector_return_fail(DnsConnector *dc)
{
	ConnectRes cres;
	cres.socks_status = 1;
	cres.fd = -1;
	cres.iface = NULL;
	(* dc->cb)(cres, dc->data);
	dns_connector_cancel(dc);
}

static void dns_connector_connect_prepare(DnsConnector *dc);

static void dns_connector_connect_check(ConnectRes res, void *data)
{
	DnsConnector *dc = (DnsConnector *) data;
	
	dc->connector = NULL;
	
	if (res.socks_status == 0)
	{
		//success
		(* dc->cb)(res, dc->data);
		dns_connector_cancel(dc);
	}
	else
	{
		//Failed, try next
		dc->addrs_iter = dc->addrs_iter->ai_next;
		dns_connector_connect_prepare(dc);
	}
}

static void dns_connector_connect_prepare(DnsConnector *dc)
{
	Sockaddr conn_addr;
	
	if (! dc->addrs_iter)
	{
		//No more addresses to try, return unsuccessfully
		dns_connector_return_fail(dc);
		return;
	}
	
	sockaddr_copy(&conn_addr, 
		dc->addrs_iter->ai_addr, dc->addrs_iter->ai_addrlen); 
	
	dc->connector = connector_connect(conn_addr, 
		dns_connector_connect_check, dc);
}

static void dns_connector_dns_query_cb
	(int result, struct evutil_addrinfo *res, void *arg)
{
	DnsConnector *dc = (DnsConnector *) arg;
	
	//If cancelled... Don't touch me, you'll be dead too
	if (result == EVUTIL_EAI_CANCEL)
		return;
	
	dc->dns_query = NULL;
	
	if (result != 0 || ! res)
	{
		//failed
		dns_connector_return_fail(dc);
		return;
	}
	
	dc->addrs = res;
	dc->addrs_iter = res;
	
	dns_connector_connect_prepare(dc);
}

DnsConnector *dns_connector_connect
	(const char *name, int port, ConnectorCB cb, void *data)
{
	DnsConnector *dc = (DnsConnector *) fs_malloc(sizeof(DnsConnector));
	char servname[16];
	struct evutil_addrinfo hints;
	
	//Init
	dc->dns_query = NULL;
	dc->connector = NULL;
	dc->addrs = NULL;
	dc->addrs_iter = NULL;
	dc->cb = cb;
	dc->data = data;
	
	//Start a dns lookup
	snprintf(servname, 16, "%d", port);
	hints.ai_flags = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_addrlen = 0;
	hints.ai_addr = NULL;
	hints.ai_canonname = NULL;
	hints.ai_next = NULL;
	dc->dns_query = evdns_getaddrinfo(dns_base, name, servname, &hints, 
		dns_connector_dns_query_cb, dc);
	
	return dc;
}

//Module initializer
void connector_init()
{
	dns_base = evdns_base_new(evbase, 1);
}
