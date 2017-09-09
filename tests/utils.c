#include "libtest.h"

int test_split_string(const char *str, char delim,
		const char *e_left, const char *e_right)
{
	char *left, *right = NULL;	

	left = split_string(str, delim, &right);

	if (e_left)
	{
		if (! left)
			return 0;
		if (! right)
			return 0;
		if (strcmp(left, e_left) != 0)
			return 0;
		if (strcmp(right, e_right) != 0)
			return 0;

		free(left);
	}
	else
	{
		if (left)
			return 0;
	}
	return 1;
}

int test_parse_long(const char *str, Status e_status, long e_out)
{
	long out;
	if (parse_long(str, &out) != e_status)
		return 0;
	if (e_status == STATUS_SUCCESS ? e_out != out : 0)
		return 0;
	return 1;
}

int main()
{
	test_run(test_split_string("xyz", 'y', "x", "z"));
	test_run(test_split_string("yz", 'y', "", "z"));
	test_run(test_split_string("xy", 'y', "x", ""));
	test_run(test_split_string("y", 'y', "", ""));
	test_run(test_split_string("xyz", 'w', NULL, NULL));
	test_run(test_split_string("", 'w', NULL, NULL));

	test_run(test_parse_long("1",  STATUS_SUCCESS, 1));
	test_run(test_parse_long("1L", STATUS_FAILURE, 0));

	return 0;
}
