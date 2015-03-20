/* interface.h
 * Handles outgoing addresses
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

#ifndef INTERFACE_H
#define INTERFACE_H

#include "utils.h"

#define INTERFACE_INET  (1 << 0)
#define INTERFACE_OFF_INET  (0)
#define INTERFACE_INET6 (1 << 1)
#define INTERFACE_OFF_INET6  (1)
#define INTERFACE_N_TYPES (2)

//Structure representing a dispatch address
typedef struct _Interface Interface;
struct _Interface
{
	Interface *next;
	int use_count;
	int metric;
	socklen_t len;
	struct sockaddr addr;
};

typedef struct
{
	Interface *next;
	int use_count;
	int metric;
	socklen_t len;
	struct sockaddr_in addr;
} InterfaceInet4;

typedef struct
{
	Interface *next;
	int use_count;
	int metric;
	socklen_t len;
	struct sockaddr_in6 addr;
} InterfaceInet6;

//Creates a new interface from string description
//Use g_free() to destroy.
Interface *interface_new_from_string(const char *desc, int metric);

//Opens a socket bound to the interface.
int interface_open(Interface *interface);

//Opens a server socket
int interface_open_server(Interface *interface);

//Closes socket bound to the interface.
void interface_close(Interface *interface);

//Writes textual description of an interface
void interface_write(Interface *interface);

//Interface manager
typedef struct
{
	Interface *ifaces[INTERFACE_N_TYPES];
} InterfaceManager;

//Creates a new blank interface manager
InterfaceManager *interface_manager_new();

//Gets a suitable interface for address families
Interface *interface_manager_get
	(InterfaceManager *manager, int addr_type);

//Adds a dispatch address
void interface_manager_add
	(InterfaceManager *manager, Interface *interface);

//Frees interface manager
void interface_manager_free(InterfaceManager *manager);

#endif //INTERFACE_H
