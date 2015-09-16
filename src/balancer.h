/* balancer.h
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


typedef struct _Interface Interface;
struct _Interface
{
	Interface *next;
	int metric;
	int use_count;
	Address addr;
};

int interface_open(Interface *iface);

void interface_close(Interface *iface);


Interface *balancer_add_from_string(const char *str);

void balancer_verify();

Interface *balancer_select(int addr_mask);


