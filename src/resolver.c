/* resolver.c
 * Asynchronously resolves domain names
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

#include "resolver.h"

gboolean resolver_idle(gpointer data)
{
	ResolverData *info = (ResolverData *) data;
	
	//Call callback and free structure
	(* info->cb)(info);
	
	//Free data
	freeaddrinfo(info->addrs);
	g_free(data);
	
	return G_SOURCE_REMOVE;
}


gpointer resolver_thread(gpointer data)
{
	ResolverData *info = (ResolverData *) data;
	char port_str[16];
	struct addrinfo hints;
	
	//Resolve it
	sprintf(port_str, "%d", info->port);
	hints.ai_flags = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_addrlen = 0;
	hints.ai_addr = NULL;
	hints.ai_canonname = NULL;
	hints.ai_next = NULL;
	
	info->status = getaddrinfo
		(info->name, port_str, &hints, &(info->addrs));
	
	//Add idle handler
	g_idle_add(resolver_idle, data);
	
	return NULL;
}

void resolver_resolve
	(const char *name, int port, ResolverCB cb, gpointer data)
{
	ResolverData *info;
	
	info = (ResolverData *) g_malloc
		(sizeof(ResolverData) + strlen(name));
	
	info->status = 0;
	info->addrs = NULL;
	info->cb = cb;
	info->data = NULL;
	info->port = port;
	strcpy(info->name, name);
	
	g_thread_unref(g_thread_new(NULL, resolver_thread, info));
}
