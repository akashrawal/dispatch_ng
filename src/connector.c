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

#include "connector.h"

typedef struct
{
	Connector parent;
	int port;
	char name[1];
} ConnectorByName;

typedef struct
{
	Connector parent;
	struct sockaddr addr;
} ConnectorByAddr;

typedef struct
{
	Connector parent;
	struct sockaddr_in addr;
} ConnectorByAddr4;

typedef struct
{
	Connector parent;
	struct sockaddr_in6 addr;
} ConnectorByAddr6;

gboolean connector_idle(gpointer data)
{
	Connector *info = (Connector *) data;
		
	//Call callback and free structure
	if (! info->cancelled)
		(* info->cb)(info);
	
	//Free data
	g_free(data);
	
	return G_SOURCE_REMOVE;
}


gpointer connector_thread(gpointer data)
{
	Connector *info = (Connector *) data;
	
	if (info->source == CONNECTOR_NAME)
	{
		ConnectorByName *by_name = (ConnectorByName *) info;
		char port_str[16];
		struct addrinfo hints, *addrs, *iter;
		int resolve_status, addr_type, saved_errno, remote_fd;
		Interface *iface;
		
		//Resolve domain name
		sprintf(port_str, "%d", by_name->port);
		hints.ai_flags = 0;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = 0;
		hints.ai_addrlen = 0;
		hints.ai_addr = NULL;
		hints.ai_canonname = NULL;
		hints.ai_next = NULL;
		
		resolve_status = getaddrinfo
			(by_name->name, port_str, &hints, &addrs);
		
		//Error checking
		if (resolve_status != 0)
		{
			info->socks_status = 1;
			goto end;
		}
		
		//Get interface
		addr_type = 0;
		for (iter = addrs; iter; iter = iter->ai_next)
		{
			if (iter->ai_socktype == AF_INET)
				addr_type |= INTERFACE_INET;
			else if (iter->ai_socktype == AF_INET6)
				addr_type |= INTERFACE_INET6;
		}
		iface = interface_manager_get(info->manager, addr_type);
		
		//See if we got an interface
		if (! iface)
		{
			info->socks_status = 3;
			goto end;
		}
		
		//Connect
		remote_fd = -1;
		for (iter = addrs; iter; iter = iter->ai_next)
		{
			if (iface->addr.sa_family == iter->ai_socktype)
			{
				remote_fd = interface_open(iface);
				
				if (connect(remote_fd, iter->ai_addr, iter->ai_addrlen) >= 0)
					break;
				saved_errno = errno;
				
				close(remote_fd);
				interface_close(iface);
				remote_fd = -1;
			}
		}
		
		//Error checking
		if (remote_fd >= 0)
		{
			//success
			info->remote_fd = remote_fd;
			info->iface = iface;
		}
		else
		{
			if (saved_errno == ECONNREFUSED)
				info->socks_status = 5;
			else if (saved_errno == ENETUNREACH)
				info->socks_status = 3;
			else
				info->socks_status = 1;
		}
		
		freeaddrinfo(addrs);
	}
	else
	{
		ConnectorByAddr *by_addr = (ConnectorByAddr *) info;
		int addr_len;
		int remote_fd;
		Interface *iface;
		
		//Get address length and interface
		if (by_addr->addr.sa_family == AF_INET)
		{
			addr_len = sizeof(struct sockaddr_in);
			iface = interface_manager_get
				(info->manager, INTERFACE_INET);
		}
		else
		{
			addr_len = sizeof(struct sockaddr_in6);
			iface = interface_manager_get
				(info->manager, INTERFACE_INET6);
		}
		
		//See if we got an interface
		if (! iface)
		{
			info->socks_status = 3;
			goto end;
		}
		
		//Connect
		remote_fd = interface_open(iface);
		if (connect(remote_fd, &(by_addr->addr), addr_len) < 0)
		{
			if (errno == ECONNREFUSED)
				info->socks_status = 5;
			else if (errno == ENETUNREACH)
				info->socks_status = 3;
			else
				info->socks_status = 1;
			
			close(remote_fd);
			interface_close(iface);
		}
		else
		{
			//Success
			info->remote_fd = remote_fd;
			info->iface = iface;
		}
	}
	
end:

	//Add idle handler
	g_idle_add(connector_idle, data);
	
	return NULL;
}

Connector *connector_connect_by_name
	(const char *name, int port, InterfaceManager *manager,
	 ConnectorCB cb, gpointer data)
{
	ConnectorByName *info;
	
	info = (ConnectorByName *) g_malloc
		(sizeof(ConnectorByName) + strlen(name));
	
	info->parent.socks_status = 0;
	info->parent.remote_fd = 0;
	info->parent.iface = NULL;
	info->parent.manager = manager;
	info->parent.source = CONNECTOR_NAME;
	info->parent.cb = cb;
	info->parent.data = data;
	info->parent.cancelled = 0;
	info->port = port;
	strcpy(info->name, name);
	
	g_thread_unref(g_thread_new(NULL, connector_thread, info));
	
	return (Connector *) info;
}

Connector *connector_connect_by_addr
	(struct sockaddr *addr, InterfaceManager *manager,
	 ConnectorCB cb, gpointer data)
{
	int addr_len;
	ConnectorByAddr *info;
	
	if (addr->sa_family == AF_INET)
	{
		addr_len = sizeof(struct sockaddr_in);
		info = (ConnectorByAddr *) g_malloc(sizeof(ConnectorByAddr4));
	}
	else
	{
		addr_len = sizeof(struct sockaddr_in6);
		info = (ConnectorByAddr *) g_malloc(sizeof(ConnectorByAddr6));
	}
	
	info->parent.socks_status = 0;
	info->parent.remote_fd = -1;
	info->parent.iface = NULL;
	info->parent.manager = manager;
	info->parent.source = CONNECTOR_ADDR;
	info->parent.cb = cb;
	info->parent.data = data;
	info->parent.cancelled = 0;
	memcpy(&(info->addr), addr, addr_len);
	
	g_thread_unref(g_thread_new(NULL, connector_thread, info));
	
	return (Connector *) info;
}

void connector_cancel(Connector *connector)
{
	connector->cancelled = 1;
}
