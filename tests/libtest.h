/* libtest.h
 * Common code for all tests
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

#include <src/incl.h>

//Runs a test function.
#define test_run(expr) \
	do { \
		if (!(expr)) \
		{ \
			fprintf(stderr, "[%d] Failed test: [%s]\n", __LINE__, #expr); \
			abort(); \
		} \
	} while (0)

//Evaluates expression and in case of error prints it.
#define test_error_handle(expr) \
	do { \
		const Error *e; \
		e = (expr); \
		if (e) \
		{ \
			fprintf(stderr, "[%d] Error: %s\n", __LINE__, error_desc(e)); \
			error_handle(e); \
			return 0; \
		} \
	} while (0)

//Aborts the program if there is error
#define abort_on_error(expr) \
	do { \
		const Error *e; \
		e = (expr); \
		if (e) \
		{ \
			abort_with_error("[%s:%d]: Expression [%s] failed: %s",\
					__FILE__, __LINE__, #expr, error_desc(e)); \
		} \
	} while (0)

//Script object
typedef struct
{
	size_t len;
	unsigned char *data;
	unsigned char *mask;
} ScriptElement;

/* Script as string
 * [0-9a-f][0-9a-f]: one byte
 * k:       Dont care
 * l:       Start new dialog
 * p-z:     Variables
 * All other characters are ignored
 */

void set_variable_loc(char id, void *data, size_t len);

ScriptElement *script_build(const char *src);

void script_free(ScriptElement *script);

//Actor
typedef struct _Actor Actor;

void actor_destroy(Actor *p);

Actor *actor_create_server(SocketHandle hd, ScriptElement *script);

Actor *actor_create_client(SocketAddress addr, ScriptElement *script);

void test_open_listener
	(const char *host, SocketHandle *hd_out, SocketAddress *addr_out);

void require_ipv6();
