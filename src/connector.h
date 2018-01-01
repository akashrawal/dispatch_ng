/* connector.h
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

//Error messaages
extern const char connector_error_dns_fail[];

typedef struct 
{
	const Error *e;
	SocketHandle hd;
	Interface *iface;
} ConnectRes;

//Prototype for the callback function
typedef void (*ConnectorCB)(ConnectRes res, void *data);

//The connector
typedef struct _Connector Connector;

//Connect to a remote socket
Connector *connector_connect
	(SocketAddress addr, ConnectorCB cb, void *data);

//Connect by resolving name by DNS
Connector *connector_connect_dns
	(const char *name, uint16_t port, ConnectorCB cb, void *data);
	
//Destroys connector object.
//Connection, if in progress, is cancelled and no callback is called.
void connector_destroy(Connector *connector);

