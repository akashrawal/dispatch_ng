/* balancer.c
 * Outgoing interface abstraction and load balancing algorithm
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

void interface_close(Interface *iface)
{
	iface->use_count--;
}

static Interface *ifaces = NULL;
NetworkType types;

Interface *balancer_add(HostAddress addr, int metric)
{
	Interface *iface;

	iface = (Interface *) fs_malloc(sizeof(Interface));
	
	iface->addr = addr;
	iface->metric = metric;
	if (iface->metric < 0)
		iface->metric = 1;
	iface->use_count = 0;
	
	iface->next = ifaces;
	ifaces = iface;
	types |= addr.type;
	
	return iface;
}

Interface *balancer_add_from_string(const char *addr_with_metric)
{
	char *addr_str, *metric_str;
	HostAddress addr;
	int metric;
	Status s;

	addr_str = split_string(addr_with_metric, '@', &metric_str);

	if (addr_str)
	{
		s = host_address_from_str(addr_str, &addr);	
		//TODO: Detect errors here
		metric = atoi(metric_str);
	}
	else
	{
		s = host_address_from_str(addr_with_metric, &addr);	
		metric = -1;
	}

	abort_if_fail(s == STATUS_SUCCESS,
			"Failed to parse address %s", addr_with_metric);

	return balancer_add(addr, metric);
}

const char balancer_error_no_iface[] = "No suitable interface available";
static const Error error_struct_no_iface[] = {{balancer_error_no_iface, NULL}};

const Error *balancer_open_iface(NetworkType types,
		Interface **iface_out, SocketHandle *hd_out)
{
	Interface *iter, *selected = NULL;
	double iter_cost, selected_cost;
	SocketHandle hd;
	const Error *e;
	
	//TODO: Improve algorithm for O(log(n)) time complexity
	for (iter = ifaces; iter; iter = iter->next)
	{
		if (! (iter->addr.type & types))
			continue;
		
		iter_cost = (double) iter->use_count / (double) iter->metric;
		
		if (selected)
		{
			if (iter_cost < selected_cost)
			{
				selected = iter;
				selected_cost = iter_cost;
			}
		}
		else
		{
			selected = iter;
			selected_cost = iter_cost;
		}
	}


	if (! selected)
		return error_struct_no_iface;

	{
		SocketAddress addr;
		addr.host = selected->addr;
		addr.port = 0;
		e = socket_handle_create_bound(addr, &hd);
	}
	if (e)
		return e;

	selected->use_count++;
	*iface_out = selected;
	*hd_out = hd;
	return NULL;
}

NetworkType balancer_get_available_types()
{
	return types;
}
