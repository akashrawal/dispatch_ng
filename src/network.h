/* network.h
 * Network API abstraction.
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

//Buffer size to use
#define BUFFER_SIZE (2048)

//Maximum size of address string representation
#define ADDRESS_MAX_LEN (50)

//Socket errors
extern const char socket_error_generic[];

extern const char socket_error_invalid_socket[];
extern const char socket_error_invalid_address[];
extern const char socket_error_again[];
extern const char socket_error_in_progress[];
extern const char socket_error_already[];
extern const char socket_error_timeout[];
extern const char socket_error_network_unreachable[];
extern const char socket_error_host_unreachable[];
extern const char socket_error_connection_refused[];
extern const char socket_error_dns_failure[];
extern const char socket_error_unsupported_backend_feature[];

//Type of network, IPV4 or IPV6
//Can be bitwised-or
typedef enum
{
	NETWORK_INET = 1,
	NETWORK_INET6 = 2
} NetworkType;

//Host / network interface address and functions
typedef struct
{
	NetworkType type;
	unsigned char ip[16]; //< network byte order
} HostAddress;

Status host_address_from_str(const char *str, HostAddress *addr_out);
void host_address_to_str(HostAddress addr, char *out);

//Socket address and functions
typedef struct
{
	HostAddress host;
	uint16_t port; //< network byte order
} SocketAddress;

void socket_address_to_str(SocketAddress addr, char *out);
Status socket_address_from_str(const char *str, SocketAddress *addr_out);
SocketAddress address_from_socks(NetworkType type, const void *data);

//Socket handle
typedef struct
{
	evutil_socket_t fd;
} SocketHandle;

void socket_handle_close(SocketHandle hd);

const Error *socket_handle_create_bound
	(SocketAddress addr, SocketHandle *hd_out);

const Error *socket_handle_create_listener
	(SocketAddress addr, SocketHandle *hd_out);

int socket_handle_equal_with_native(SocketHandle hd, evutil_socket_t socket);

struct event *socket_handle_create_event(SocketHandle hd, short events,
		event_callback_fn callback_fn, void *callback_arg);

const Error *socket_handle_connect(SocketHandle hd, SocketAddress addr);

const Error *socket_handle_accept(SocketHandle hd, SocketHandle *hd_out);

const Error *socket_handle_get_status(SocketHandle hd);

const Error *socket_handle_getsockname
	(SocketHandle hd, SocketAddress *addr_out);

const Error *socket_handle_set_blocking(SocketHandle hd, int val);

const Error *socket_handle_write
	(SocketHandle hd, const void *data, size_t len, size_t *out);

const Error *socket_handle_read
	(SocketHandle hd, void *data, size_t len, size_t *out);

//Asynchronous DNS
typedef struct _DnsRequest DnsRequest;

//TODO: Errors (DNS failure errors, backend failure)
typedef void (*DnsResponseCB)
	(const Error *e, size_t n_addrs, SocketAddress *addrs, void *data);

DnsRequest *dns_request_resolve
	(const char *hostname, uint16_t port, NetworkType types,
	 DnsResponseCB cb, void *cb_data);

void dns_request_destroy(DnsRequest *dns_ctx);

