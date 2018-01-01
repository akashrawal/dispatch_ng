/* network.c
 * Unit tests for src/network.c
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

#include "libtest.h"

const char *same = "e_str is same as i_str";

int test_socket_address(const char *i_str, const char *e_str)
{
	SocketAddress addr;
	Status status;
	char str[ADDRESS_MAX_LEN];

	status = socket_address_from_str(i_str, &addr);
	if (status != (e_str ? STATUS_SUCCESS : STATUS_FAILURE))
		return 0;
	if (! e_str)
		return 1;
	if (e_str == same)
		e_str = i_str;

	socket_address_to_str(addr, str);

	return strcmp(str, e_str) == 0 ? 1 : 0;
}

int test_getsockname(const char *i_str)
{
	SocketHandle hd;
	char str[ADDRESS_MAX_LEN];
	SocketAddress i_addr, addr;

	if (socket_address_from_str(i_str, &i_addr) != STATUS_SUCCESS)
		return 0;

	test_error_handle(socket_handle_create_bound(i_addr, &hd));

	test_error_handle(socket_handle_getsockname(hd, &addr));

	socket_address_to_str(addr, str);
	
	if (strcmp(i_str, str) != 0)
		return 0;

	socket_handle_close(hd);

	return 1;
}

int main()
{
	test_run(test_socket_address("192.168.56.101:7080", same));
	test_run(test_socket_address("[1:2:3:4:5:6:7:8]:7080", same));
	//TODO: Add test for '::' feature for ipv6

	test_run(test_getsockname("127.0.0.1:7080"));
}
