/* connector.h
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


typedef struct 
{
	int socks_status;
	int fd;
	Interface *iface;
} ConnectRes;

typedef void (*ConnectorCB)(ConnectRes res, void *data);

typedef struct 
{
	int fd;
	Interface *iface;
	struct event *evt;
	
	ConnectorCB cb;
	void *data;
} Connector;

Connector *connector_connect
	(Sockaddr saddr, ConnectorCB cb, void *data);
	
void connector_cancel(Connector *connector);


extern struct evdns_base *dns_base;

typedef struct 
{
	struct evdns_getaddrinfo_request *dns_query;
	struct evutil_addrinfo *addrs;
	struct evutil_addrinfo *addrs_iter;
	Connector *connector;
	
	ConnectorCB cb;
	void *data;
} DnsConnector;

DnsConnector *dns_connector_connect
	(const char *name, int port, ConnectorCB cb, void *data);

void dns_connector_cancel(DnsConnector *dc);

void connector_init();

