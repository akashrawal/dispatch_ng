#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([dispatch_ng], [0.1], [], [dispatch_ng], [https://github.com/akashrawal/dispatch_ng])
AC_CONFIG_SRCDIR([src/main.c])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_AR

# Checks for libraries.
AC_CHECK_LIB([event], [event_base_new], [], [AC_MSG_ERROR(["could not find required library libevent"])])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h string.h sys/socket.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([socket strerror])

AC_CONFIG_FILES([Makefile
                 src/Makefile
				 tests/Makefile])
AC_OUTPUT
