/* utils.c
 * Common utility functions
 * 
 * Copyright 2015 Akash Rawal
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

#include "utils.h"

//Error reporting

void abort_with_error(const char *fmt, ...)
{
	va_list arglist;
	
	fprintf(stderr, "ERROR: ");
	
	va_start(arglist, fmt);
	vfprintf(stderr, fmt, arglist);
	va_end(arglist);
	
	fprintf(stderr, "\n");
	
	abort();
}

void abort_with_liberror(const char *fmt, ...)
{
	va_list arglist;
	
	fprintf(stderr, "ERROR: ");
	
	va_start(arglist, fmt);
	vfprintf(stderr, fmt, arglist);
	va_end(arglist);
	
	fprintf(stderr, ": %s\n", strerror(errno));
	
	abort();
}	
