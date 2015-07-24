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

int interface_open(Interface *iface)
{
	iface->use_count++;
	return address_open_iface(&(iface->addr));
}

void interface_close(Interface *iface)
{
	iface->use_count--;
}


static Interface *ifaces = NULL;

Interface *balancer_add_from_string(const char *str)
{
	Interface *iface;
	
	iface = (Interface *) fs_malloc(sizeof(Interface));
	
	address_read(&(iface->addr), str, &(iface->metric), METRIC); 
	if (iface->metric < 0)
		iface->metric = 1;
	iface->use_count = 0;
	
	iface->next = ifaces;
	ifaces = iface;
	
	return iface;
}

void balancer_dump()
{
	Interface *iter;
	
	for (iter = ifaces; iter; iter = iter->next)
	{
		address_write(&(iter->addr), stdout);
		printf("@%d\n",  iter->metric);
	}
}

Interface *balancer_select(int addr_mask)
{
	Interface *iter, *selected = NULL;
	double iter_cost, selected_cost;
	
	for (iter = ifaces; iter; iter = iter->next)
	{
		if (! (iter->addr.type & addr_mask))
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
	
	return selected;
}
