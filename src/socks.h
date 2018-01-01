/* socks.h
 * SOCKS numerical codes
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
 
//TODO: Decide whether to support all? (low priority)
typedef enum
{
	SOCKS_AUTH_NONE = 0, //< Only one that is supported
	SOCKS_AUTH_GSSAPI = 1,
	SOCKS_AUTH_USERPASS = 2,
	SOCKS_AUTH_NO_ACCEPTABLE_METHODS = 255
} SocksAuthCode;

typedef enum
{
	SOCKS_CMD_CONNECT = 1, //< Only one that works
	SOCKS_CMD_BIND = 2,
	SOCKS_CMD_UDP_ASSOCIATE = 3
} SocksRequestCommand;

typedef enum 
{
	SOCKS_ADDR_IPV4 = 1,
	SOCKS_ADDR_DOMAIN = 3,
	SOCKS_ADDR_IPV6 = 4
} SocksAddrtype;

typedef enum
{
	SOCKS_REPLY_SUCCESS = 0,
	SOCKS_REPLY_GEN = 1,
	SOCKS_REPLY_CONN_NOT_ALLOWED = 2,
	SOCKS_REPLY_NETUNREACH = 3,
	SOCKS_REPLY_HOSTUNREACH = 4,
	SOCKS_REPLY_CONNREFUSED = 5,
	SOCKS_REPLY_TTLEXPIRED = 6,
	SOCKS_REPLY_CMD = 7,
	SOCKS_REPLY_ATYPE = 8
} SocksReplyCode;

const char *socks_reply_to_str(int code);

int socks_reply_from_error(const Error *e);
