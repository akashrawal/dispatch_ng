
SUBDIRS = src tests
EXTRA_DIST = README.md

check-valgrind:
	make LOG_COMPILER="valgrind --suppressions=$(abs_srcdir)/tests/valgrind-suppressions --error-exitcode=2 --track-origins=yes --leak-check=full --show-leak-kinds=all" check
