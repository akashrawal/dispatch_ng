/* setup.c
 * Tests for the testing library itself
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

const char script_text[] =
	"l 01"
	"l 01 02"
	"l 01 02 03"
	"l k  k  k "
	"l p"
;

int main()
{
	Actor *client, *server;
	SocketAddress addr;
	SocketHandle hd;
	ScriptElement *script;

	utils_init();

	char var_contents[] = "test";
	set_variable_loc('p', var_contents, 4);

	script = script_build(script_text);

	test_open_listener("127.0.0.1", &hd, &addr);
	server = actor_create_server(hd, script);
	client = actor_create_client(addr, script);

	event_base_loop(evbase, 0);

	actor_destroy(server);
	actor_destroy(client);

	script_free(script);
	utils_shutdown();

	return 0;
}
