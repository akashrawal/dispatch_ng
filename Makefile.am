
SUBDIRS = src tests

check-valgrind:
	make LOG_COMPILER="valgrind --suppressions=$(abs_srcdir)/tests/valgrind-suppressions --error-exitcode=2 --leak-check=full" check
