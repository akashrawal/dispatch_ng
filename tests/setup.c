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
