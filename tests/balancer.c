/* balancer.c
 * Unit tests for src/balancer.c
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

typedef struct
{
	char istr[ADDRESS_MAX_LEN];
	int use_count;
} AddressList;

typedef struct
{
	Interface *iface;
	int use_count;
} CheckList;

int test_balancer(NetworkType network_types, const AddressList *addrs)
{
	int i, j;
	int n_addrs;
	int total_use_count = 0;

	for (n_addrs = 0; addrs[n_addrs].istr[0]; n_addrs++)
		;

	CheckList *check = fs_malloc(sizeof(CheckList) * n_addrs);

	for (i = 0; i < n_addrs; i++)
	{
		check[i].iface = balancer_add_from_string(addrs[i].istr);	
		if (check[i].iface == NULL)
			return 0;
		check[i].use_count = addrs[i].use_count;
		total_use_count += addrs[i].use_count;
	}

	for (j = 0; j < total_use_count; j++)
	{
		Interface *iface = NULL;
		SocketHandle hd;

		test_error_handle(balancer_open_iface(network_types, &iface, &hd));
		socket_handle_close(hd);

		for (i = 0; i < n_addrs; i++)
		{
			if (iface == check[i].iface)
				break;
		}
		if (i == n_addrs)
		{
			fprintf(stderr, "Unidentified interface %p\n", iface);
			return 0;
		}

		check[i].use_count--;
	}

	for (i = 0; i < n_addrs; i++)
	{
		if (check[i].use_count != 0)
		{
			fprintf(stderr, "check[%d].use_count != 0\n", i);
			return 0;
		}
	}

	balancer_shutdown();

	free(check);
	return 1;
}

int main()
{
	utils_init();
	
	test_run(test_balancer(NETWORK_INET, (const AddressList[]) {
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"[::]", 0},
			{"", 0}
		}));

	test_run(test_balancer(NETWORK_INET | NETWORK_INET6, (const AddressList[]) {
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"0.0.0.0", 1},
			{"[::]", 1},
			{"[::]", 1},
			{"[::]", 1},
			{"[::]", 1},
			{"[::]", 1},
			{"[::]", 1},
			{"[::]", 1},
			{"[::]", 1},
			{"", 0}
		}));

	test_run(test_balancer(NETWORK_INET, (const AddressList[]) {
			{"0.0.0.0@2", 2},
			{"0.0.0.0", 1},
			{"0.0.0.0@3", 3},
			{"0.0.0.0", 1},
			{"0.0.0.0@2", 2},
			{"0.0.0.0", 1},
			{"0.0.0.0@4", 4},
			{"0.0.0.0", 1},
			{"0.0.0.0@2", 2},
			{"0.0.0.0", 1},
			{"0.0.0.0@1", 1},
			{"0.0.0.0", 1},
			{"", 0}
		}));

	utils_shutdown();
	return 0;
}

