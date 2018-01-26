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

- [Source](https://bintray.com/akashrawal/dispatch_ng/source)
- [Windows build](https://bintray.com/akashrawal/dispatch_ng/mingw-w64)


## Building from git

	git clone https://gitlab.com/akash_rawal/dispatch_ng.git
	cd dispatch_ng
	libtoolize
	aclocal
	automake --add-missing
	autoconf
	mkdir build
	cd build
	../configure
	make -j`nproc` check
	sudo make install
