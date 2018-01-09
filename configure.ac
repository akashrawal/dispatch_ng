#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([dispatch_ng], [0.1], [], [dispatch_ng], [https://github.com/akashrawal/dispatch_ng])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_AUX_DIR([auxdir])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_AR

# Checks for libraries.
PKG_CHECK_MODULES([EVENT], [libevent >= 2.1])
AC_SEARCH_LIBS([socket], [ws2_32])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 src/Makefile
				 tests/Makefile])
AC_OUTPUT
