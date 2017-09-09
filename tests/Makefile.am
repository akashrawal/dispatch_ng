
common_cflags = $(DISPATCH_NG_CFLAGS) -I$(top_srcdir) -DTESTDATA=$(srcdir)/data
common_libadd = $(DISPATCH_NG_LIBS) ../src/libdispatch.a


check_LIBRARIES = libtest.a
libtest_a_SOURCES = libtest.c libtest.h
libtest_a_CFLAGS = $(common_cflags)
libtest_a_LIBADD = $(common_libadd)

check_PROGRAMS = \
	utils \
	network \
	setup \
	test-ipv4

TESTS = $(check_PROGRAMS)
AM_CFLAGS = $(common_cflags)
LDADD = $(common_libadd) libtest.a

EXTRA_DIST = valgrind-suppressions

