/* address.c
 * Internet address abstraction
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


void address_read
	(Address *addr, const char *str, int *suffix, int what)
{
	char *str_cp;
	int addr_end = 0;
	int s_data = -1;
	size_t str_len;
	
	str_len = strlen(str);
	str_cp = fs_malloc(str_len);
	strcpy(str_cp, str);
	
	//First character will tell address type
	if (str_cp[0] != '[')
	{
		//IPv4
		
		//Remove ':'
		for (addr_end = 0; str_cp[addr_end]; addr_end++)
			if (str_cp[addr_end] == what)
				break;
		str_cp[addr_end] = 0;
		
		//Read address
		if (inet_pton(AF_INET, str_cp, 
		              (void *) &(addr->ip) != 1)
		{
			abort_with_error("Invalid address %s", str);
		}
		addr->type = ADDRESS_INET;
	}
	else
	{
		//IPv6
		InterfaceInet6 *inet6_iface;
		
		//Remove ending ']'
		for (addr_end = 0; str_cp[addr_end]; addr_end++)
			if (str_cp[addr_end] == ']')
				break;
		if (! str_cp[addr_end])
			abort_with_error("Invalid address %s", str);
		str_cp[addr_end] = 0;
		
		//Read address
		if (inet_pton(AF_INET6, str_cp + 1, 
		              (void *) &(addr->ip)) != 1)
		{
			abort_with_error("Invalid address %s", str);
		}
		addr->type = ADDRESS_INET6;
	}
	
	//Put back ':' or ']'
	str_cp[addr_end] = str[addr_end];
	
	//Find suffix
	for (; str_cp[addr_end]; addr_end++)
		if (str_cp[addr_end] == what)
		{
			addr_end++;
			s_data = atoi(str_cp + addr_end);
			break;
		}
	if (s_data < 0)
		abort_with_error("Invalid address %s", str);
	
	free(str_cp);
	
	(* suffix) = s_data;
}

void address_create_sockaddr(Address *addr, int port, Sockaddr *saddr);
{
	memset(saddr, 0, sizeof(SockaddrAll));
	if (addr->type == ADDRESS_INET)
	{
		saddr->x.v4.sin_family = AF_INET;
		memcpy(&(saddr->x.v4.sin_addr), addr->ip, 4);
		saddr->x.v4.sin_port = htons(port);
		saddr->len = sizeof(struct sockaddr_in);
	}
	else
	{
		saddr->x.v6.sin6_family = AF_INET6;
		memcpy(&(saddr->x.v6.sin6_addr), addr->ip, 16);
		saddr->x.v6.sin6_port = htons(port);
		saddr->len = sizeof(struct sockaddr_in6);
	}
}

int address_open_bound_socket(Address *addr, int port)
{
	int fd;
	SockaddrAll saddr;
	
	if (addr->type == ADDRESS_INET)
	{
		fd = socket(PF_INET, SOCK_STREAM, 0);
	}
	else
	{
		fd = socket(PF_INET6, SOCK_STREAM, 0);
	}
	
	if (fd < 0)
		abort_with_liberror("socket()");
		
	address_create_sockaddr(addr, port, &saddr);
	if (bind(fd, &(saddr.x.x), saddr.len) < 0)
		abort_with_liberror("bind()");
	
	fd_set_blocking(fd, 0);
	
	return fd;
}

int address_open_iface(Address *addr)
{
	return address_open_bound_socket(addr, 0);
}

int address_open_svr(Address *addr, int port)
{
	int fd = address_open_bound_socket(addr, port);
	
	if (listen(fd, 10) < 0)
		abort_with_liberror("listen()");
	
	return fd;
}

void address_write(Address *addr, FILE *file)
{
	void *ap;
	int af;
	char buf[64];
	
	if (addr->type == ADDRESS_INET)
	{
		ap = &(addr->x.v4.sin_addr);
		af = AF_INET;
	}
	else 
	{
		ap = &(addr->x.v6.sin6_addr);
		af = AF_INET6;
	}
	
	if (! inet_ntop(af, ap, buf, 64))
		abort_with_liberror("inet_ntop()");
	
	fprintf(file, "%s", buf);
}

