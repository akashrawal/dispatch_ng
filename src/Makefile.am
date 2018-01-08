noinst_LIBRARIES = libdispatch.a
bin_PROGRAMS = dispatch-ng

#Most of the sources are collected into a library for unit tests
libdispatch_a_SOURCES = incl.h \
		utils.c      utils.h            \
		network.c    network.h          \
		balancer.c   balancer.h         \
		socks.c      socks.h            \
		connector.c  connector.h        \
		session.c    session.h          \
		server.c     server.h 
libdispatch_a_CFLAGS = $(EVENT_CFLAGS)
#libdispatch_a_LIBADD = $(EVENT_LIBS)

 
#The program
dispatch_ng_SOURCES = main.c
dispatch_ng_CFLAGS = $(EVENT_CFLAGS)
dispatch_ng_LDADD = libdispatch.a $(EVENT_LIBS)
                      

