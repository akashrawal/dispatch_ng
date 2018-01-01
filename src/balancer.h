/* balancer.h
 * Outgoing interface abstraction and load balancing algorithm
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


typedef struct _Interface Interface;
struct _Interface
{
	Interface *next;
	int metric;
	int use_count;
	HostAddress addr;
};

void interface_close(Interface *iface);

Interface *balancer_add(HostAddress addr, int metric);

Interface *balancer_add_from_string(const char *addr_with_metric);

//This function might no longer be needed
//void balancer_verify();

extern const char balancer_error_no_iface[];

const Error *balancer_open_iface(NetworkType types,
		Interface **iface_out, SocketHandle *hd_out);

NetworkType balancer_get_available_types();

void balancer_shutdown();
