/* socks.h
 * SOCKS numerical codes
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


static char *socks_error_strings[10] = {
	"0 (SOCKS_REPLY_SUCCESS, Succeded)",
	"1 (SOCKS_REPLY_GEN general, SOCKS server failure)",
	"2 (SOCKS_REPLY_CONN_NOT_ALLOWED, connection not allowed by ruleset)",
	"3 (SOCKS_REPLY_NETUNREACH, Network unreachable)",
	"4 (SOCKS_REPLY_HOSTUNREACH, Host unreachable)",
	"5 (SOCKS_REPLY_CONNREFUSED, Connection refused)",
	"6 (SOCKS_REPLY_TTLEXPIRED, TTL expired)",
	"7 (SOCKS_REPLY_CMD, Command not supported)",
	"8 (SOCKS_REPLY_ATYPE, Address type not supported)",
	"Invalid reply code"
};

const char *socks_reply_to_str(int code)
{
	if (code < 0 || code > 8)
		code = 9;
	
	return socks_error_strings[code];
}
