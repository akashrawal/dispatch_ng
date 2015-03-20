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

#ifndef CONNECTOR_H
#define CONNECTOR_H

#include "interface.h"

typedef enum
{
	CONNECTOR_NAME,
	CONNECTOR_ADDR
} ConnectorSource;

typedef struct _Connector Connector;

typedef void (*ConnectorCB)(Connector *info);

struct _Connector
{
	int socks_status;
	int remote_fd;
	Interface *iface;
	InterfaceManager *manager;
	
	//Private
	ConnectorSource source;
	ConnectorCB cb;
	gpointer data;
	int cancelled;
};


Connector *connector_connect_by_name
	(const char *name, int port, InterfaceManager *manager, 
	ConnectorCB cb, gpointer data);

Connector *connector_connect_by_addr
	(struct sockaddr *addr, InterfaceManager *manager,
	ConnectorCB cb, gpointer data);
	
void connector_cancel(Connector *connector);

#endif //CONNECTOR_H
