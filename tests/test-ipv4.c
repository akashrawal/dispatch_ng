#include "libtest.h"

char script_text[] = 
	"l 05 01 00" //SOCKS5 handshake
	"l 05 00" //Server selects 'no authentication'
	"l 05 01 00 01 7f 00 00 01 1b a9" //Connect to 127.0.0.1:7081
	"l 05 00 00 01  k  k  k  k  k  k" //Connection successful
	"l 01" //Test traffic...
	"l 01 02"
	;

/*
void finish_cb(Actor *p, void *data)
{
	event_base_loopbreak(evbase);
}
*/

int main()
{
	Actor *client, *server;
	SocketAddress addr;
	ScriptElement *script;

	utils_init();

	//TODO: Allocate sockets so that we can remove
	//      the magic number 7080 and 7081

	//Build script
	script = script_build(script_text);

	//Start proxy
	balancer_add_from_string("0.0.0.0");
	server_create("127.0.0.1:7080", 1);

	//Start client
	socket_address_from_str("127.0.0.1:7080", &addr);
	client = actor_create_client(addr, script);

	//Start server
	socket_address_from_str("127.0.0.1:7081", &addr);
	server = actor_create_server(addr, script + 4);

	//Set callbacks
	actor_set_callback(server, NULL, NULL);
	actor_set_callback(client, NULL, NULL);

	event_base_loop(evbase, 0);

	actor_destroy(server);
	actor_destroy(client);

	script_free(script);

	return 0;
}
