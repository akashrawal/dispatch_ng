/* main.c 
 * The main program
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

#include "server.h"

int main(int argc, char *argv[])
{
	int i, len;
	Interface *listen = NULL, *dispatch = NULL, *iter, *next;
	InterfaceManager *manager;
	GMainLoop *loop;
	
	//Read arguments
	for (i = 1; i < argc; i++)
	{
		if ((strcmp(argv[i], "-h") == 0)
			|| (strcmp(argv[i], "--help") == 0))
		{
			printf("Usage: $0 [--bind=addr:port] "
				"addr1:metric1 addr2:metric2 ...\n");
			exit(1);
		}
		len = strlen("--bind=")
		if (strncmp(argv[i], "--bind=", len) == 0)
		{
			iter = interface_new_from_string(argv[i] + len, 1080);
			iter->next = listen;
			listen = iter;
		}
		else
		{
			iter = interface_new_from_string(argv[i] + len, 1);
			iter->next = dispatch;
			dispatch = iter;
		}
	}
	
	//Default listening addresses
	if (! listen)
	{
		iter = interface_new_from_string("127.0.0.1", 1080);
		iter->next = listen;
		listen = iter;
		
		iter = interface_new_from_string("[::1]", 1080);
		iter->next = listen;
		listen = iter;
	}
	
	if (! dispatch)
	{
		abort_with_error("No addresses to dispatch.");
	}
	
	//Print details
	printf("SOCKS server listening at");
	for (iter = listen; iter; iter = iter->next)
	{
		interface_write(iter);
		printf(" ");
	}
	
	printf("Dispatching to addresses");
	for (iter = dispatch; iter; iter = iter->next)
	{
		interface_write(iter);
		printf(" ");
	}
	
	//Start dispatch
	manager = interface_manager_new();
	for (iter = dispatch; iter; iter = iter->next)
	{
		interface_manager_add(manager, iter);
	}
	for (iter = listen; iter; iter = iter->next)
	{
		server_create(iter, manager);
	}
	loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);
	
	return 0;
}
