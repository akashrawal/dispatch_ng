/* balancer.c
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

#include "incl.h"

//Interface data structure
struct _Interface
{
	int index;
	int metric;
	int use_count;
	HostAddress addr;
};
NetworkType types = 0;

//Min-heap data structure
typedef struct
{
	Interface **data;
	size_t len, alloc_len;
} Heap;

Heap ip4[1] = {0}, ip6[1] = {0};

static double value(Heap *heap, int idx)
{
	return ((double) heap->data[idx]->use_count / heap->data[idx]->metric);
}

static void assign(Heap *heap, int idx, Interface *value)
{
	heap->data[idx] = value;
	if (value)
		value->index = idx;
}

#if 0
static void assert_heap(Heap *heap)
{
	int i, parent;

	for (i = 1; i < heap->len; i++)
	{
		parent = (i - 1) / 2;
		abort_if_fail(value(heap, parent) <= value(heap, i),
				"Assertion failure");
	}
}
#endif

static void shift_up(Heap *heap, int idx)
{
	Interface *tmp = heap->data[idx];
	double tmp_value = value(heap, idx);
	while (idx > 0)
	{
		int parent = (idx - 1) / 2;
		if (value(heap, parent) <= tmp_value)
			break;
		assign(heap, idx, heap->data[parent]);
		idx = parent;
	}
	assign(heap, idx, tmp);

	//assert_heap(heap);
}

static void shift_down(Heap *heap, int idx)
{
	Interface *tmp = heap->data[idx];
	double tmp_value = value(heap, idx);

	while (idx < heap->len)
	{
		int left = idx * 2 + 1;
		int right = idx * 2 + 2;
		int sel = -1;

		if (left < heap->len)
			sel = left;

		if (right < heap->len)
		{
			if (value(heap, right) < value(heap, left))
				sel = right;
		}

		if (sel >= 0)
		{
			if (value(heap, sel) < tmp_value)
			{
				assign(heap, idx, heap->data[sel]);
				idx = sel;
				continue;
			}
		}
		break;
	}

	abort_if_fail(idx < heap->len, "Assertion failure");

	assign(heap, idx, tmp);

	//assert_heap(heap);
}

static Heap *select_heap(Interface *iface)
{
	if (iface->addr.type == NETWORK_INET)
		return ip4;
	else
		return ip6;
}

static void heap_insert(Heap *heap, Interface *iface)
{
	if (heap->alloc_len == 0)
	{
		heap->alloc_len = 8;
		heap->data = malloc(sizeof(void *) * heap->alloc_len);
	}
	else if (heap->alloc_len >= heap->len)
	{
		heap->alloc_len *= 2;
		heap->data = realloc(heap->data, sizeof(void *) * heap->alloc_len);
	}
	heap->len++;
	assign(heap, heap->len - 1, iface);
	shift_up(heap, heap->len - 1);
}

void interface_close(Interface *iface)
{
	iface->use_count--;
	shift_up(select_heap(iface), iface->index);
}

HostAddress interface_get_addr(Interface *iface)
{
	return iface->addr;
}

Interface *balancer_add(HostAddress addr, int metric)
{
	Interface *iface;

	iface = (Interface *) fs_malloc(sizeof(Interface));
	
	iface->addr = addr;
	iface->metric = metric;
	if (iface->metric < 0)
		iface->metric = 1;
	iface->use_count = 0;
	
	heap_insert(select_heap(iface), iface);
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
		char *endptr;
		s = host_address_from_str(addr_str, &addr);	
		metric = strtol(metric_str, &endptr, 10);
		if (*endptr)
			s = STATUS_FAILURE;
		free(addr_str);
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
	Heap *heaps[2] = { NULL, NULL };
	Interface *selected = NULL;
	Heap *selected_heap = NULL;
	SocketHandle hd;
	const Error *e = NULL;
	int i;

	if (types & NETWORK_INET)
		heaps[0] = ip4;
	if (types & NETWORK_INET6)
		heaps[1] = ip6;

	for (i = 0; i < 2; i++)
	{
		if (!heaps[i])
			continue;

		if (heaps[i]->len == 0)
			continue;


		if (selected_heap)
		{
			if (value(heaps[i], 0) >= value(selected_heap, 0))
				continue;
		}

		selected_heap = heaps[i];
	}

	if (! selected_heap)
		return error_struct_no_iface;

	selected = selected_heap->data[0];

	{
		SocketAddress addr;
		addr.host = selected->addr;
		addr.port = 0;
		e = socket_handle_create_bound(addr, &hd);
	}
	if (e)
		return e;

	selected->use_count++;
	shift_down(selected_heap, selected->index);
	*iface_out = selected;
	*hd_out = hd;
	return NULL;
}

NetworkType balancer_get_available_types()
{
	return types;
}

void balancer_shutdown()
{
	int i, j;
	Heap *heaps[2] = { ip4, ip6 };

	for (i = 0; i < 2; i++)
	{
		if (! heaps[i]->data)
			break;

		for (j = 0; j < heaps[i]->len; j++)
			free(heaps[i]->data[j]);
		free(heaps[i]->data);

		memset(heaps[i], 0, sizeof(Heap));
	}

	types = 0;
}

