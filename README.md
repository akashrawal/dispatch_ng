# dispatch-ng

dispatch-ng is an internet load balancer. You give it all the internet 
connections you have, and it creates a single load-balanced SOCKS5 proxy to use 
with, say, a multi-threaded download manager to get download speeds higher
than you may get from one internet connection.

## Usage

    dispatch-ng [--bind=address:port] <all IP addresses you have>

e.g.

    dispatch-ng 172.16.84.101 192.168.43.24

You can even provide metrics that decide the ratio by which your internet
connections are used.

    dispatch-ng 172.16.84.101@2 192.168.43.24@1

## Downloads

- [Source and Windows build](https://gitlab.com/akash_rawal/dispatch_ng/-/packages)


## Building from git

	git clone https://gitlab.com/akash_rawal/dispatch_ng.git
	cd dispatch_ng
	cargo build --release
