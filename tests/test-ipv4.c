#include "libtest.h"

char script_text[] = 
	"l 05 01 00" //SOCKS5 handshake
	"l 05 00" //Server selects 'no authentication'
	"l 05 01 00 01 7f 00 00 01     p" //Connect to 127.0.0.1:$p
	"l 05 00 00 01  k  k  k  k  k  k" //Connection successful
	"l 01" //Test traffic...
	"l 01 02"
;

int main()
{
	Actor *client, *server;
	Server *proxy;
	SocketAddress proxy_addr, server_addr;
	SocketHandle proxy_hd, server_hd;
	ScriptElement *script;

	utils_init();

	//Open sockets
	test_open_listener("127.0.0.1", &proxy_hd, &proxy_addr);
	test_open_listener("127.0.0.1", &server_hd, &server_addr);

	//Set variables
	set_variable_loc('p', &(server_addr.port), 2);

	//Build script
	script = script_build(script_text);

	//Start proxy
	balancer_add_from_string("0.0.0.0");
	proxy = server_create_test(proxy_hd);

	//Start server
	server = actor_create_server(server_hd, script + 4);

	//Start client
	client = actor_create_client(proxy_addr, script);

	//Start event loop
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
