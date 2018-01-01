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

void callback()
{
	static int counter = 3;

	counter--;
	if (counter == 0)
		event_base_loopbreak(evbase);
}

void actor_cb(Actor *p, void *data)
{
	callback();
}

void server_event_cb(Server *server, ServerEvent event, void *data)
{
	if (event == SERVER_SESSION_CLOSE)
		callback();
}

int main()
{
	Actor *client, *server;
	Server *proxy;
	SocketAddress addr;
	ScriptElement *script;

	utils_init();

	//TODO: Allocate sockets so that we can remove
	//      the magic number 7080 and 7081

	//Build script
	script = script_build(script_text);

	//Start proxy
	balancer_add_from_string("0.0.0.0");
	proxy = server_create("127.0.0.1:7080");

	//Start client
	socket_address_from_str("127.0.0.1:7080", &addr);
	client = actor_create_client(addr, script);

	//Start server
	socket_address_from_str("127.0.0.1:7081", &addr);
	server = actor_create_server(addr, script + 4);

	//Set callbacks
	server_set_cb(proxy, server_event_cb, NULL);
	actor_set_callback(server, actor_cb, NULL);
	actor_set_callback(client, actor_cb, NULL);

	event_base_loop(evbase, 0);

	//Free everything
	actor_destroy(server);
	actor_destroy(client);
	server_destroy(proxy);
	script_free(script);
	balancer_shutdown();
	utils_shutdown();

	return 0;
}
