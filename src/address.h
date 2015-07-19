/* address.h
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

typedef struct
{
	int len;
	union
	{
		struct sockaddr x;
		struct sockaddr_in v4;
		struct sockaddr_in6 v6;
	} x;
} Sockaddr;

#define ADDRESS_INET  (1 << 0)
#define ADDRESS_OFF_INET  (0)
#define ADDRESS_INET6 (1 << 1)
#define ADDRESS_OFF_INET6  (1)
#define ADDRESS_N_TYPES (2)

typedef struct 
{
	int type;
	uint32_t ip[4];
} Address;

#define PORT ':'

#define METRIC '@'

void address_read(Address *addr, const char *str, int *suffix, char what);

void address_create_sockaddr(Address *addr, int port, Sockaddr *saddr);

int address_open_iface(Address *addr);

int address_open_svr(Address *addr, int port);

