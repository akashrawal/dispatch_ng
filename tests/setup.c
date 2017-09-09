#include "libtest.h"

char script_text[] = 
	"l 01"
	"l 01 02"
	"l 01 02 03"
	"l k  k  k "
	"l p"
	;

int counter = 2;

void finish_cb(Actor *p, void *data)
{
	counter--;
	if (counter == 0)
		event_base_loopbreak(evbase);
}

int main()
{
	Actor *client, *server;
	SocketAddress addr;
	ScriptElement *script;

	char var_contents[] = "test";
	set_variable_loc('p', var_contents, 4);
	

	utils_init();

	script = script_build(script_text);

	socket_address_from_str("127.0.0.1:7080", &addr);
	server = actor_create_server(addr, script);
	client = actor_create_client(addr, script);

	actor_set_callback(server, finish_cb, NULL);
	actor_set_callback(client, finish_cb, NULL);

	event_base_loop(evbase, 0);

	actor_destroy(server);
	actor_destroy(client);

	script_free(script);

	return 0;
}
