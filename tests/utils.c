/* utils.c
 * Unit tests for src/utils.c
 * 
 * Copyright 2015-2018 Akash Rawal
 * This file is part of dispatch_ng.
 * 
 * dispatch_ng is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * dispatch_ng is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with dispatch_ng.  If not, see <http://www.gnu.org/licenses/>.
 */

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
