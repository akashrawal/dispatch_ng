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
