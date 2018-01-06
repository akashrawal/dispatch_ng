/* utils.h
 * Common utility functions
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


//Status reporting
typedef enum
{
	STATUS_SUCCESS = 1,
	STATUS_FAILURE = 0
} Status;

//Abort functions
void abort_with_error(const char *fmt, ...);
void abort_with_liberror(const char *fmt, ...);

//Assertions
#define abort_if_fail(expr, ...) \
	if (!(expr)) \
	{ \
		fprintf(stderr, "Failed assertion: [%s]\n", #expr); \
		abort_with_error(__VA_ARGS__); \
	}
#define abort_if_libfail(expr, ...) \
	if (!(expr)) \
	{ \
		fprintf(stderr, "Failed assertion: [%s]\n", #expr); \
		abort_with_liberror(__VA_ARGS__); \
	}

//Allocation
void *fs_malloc(size_t size);
void *fs_realloc(void *mem, size_t size);

//String functions
char *split_string(const char *str, char delim, char **right_out);
char *fs_strdup_vprintf(const char *fmt, va_list arglist);
char *fs_strdup_printf(const char *fmt, ...);
Status parse_long(const char *str, long *out);

//error reporting
typedef struct _Error Error;
struct _Error
{
	const char *type; //< Must be statically allocated
	const char *desc; //< NULL for static errors
};

//By memory allocation there are 2 kinds of errors,
// - static errors, Error struct is allocated statically, e->desc is NULL.
// - dynamically allocated errors, created by error_printf()

//Generic error
extern const char error_generic[];
extern const Error error_generic_instance[1];

//Define static error
#define define_static_error(_type, _string) \
	const char _type[] = _string; \
	const Error _type ## _instance[1] = {{ _type, NULL }}

//Create a dynamic error
const Error *error_printf(const char *type, const char *format, ...);

//Gets error description
static inline const char *error_desc(const Error *e)
{
	if (e)
	{
		if (e->desc)
			return e->desc;
		else
			return e->type;
	}
	else
	{
		return NULL;
	}
}

//If e is not null, frees the error if applicable and returns e->type
//else returns NULL
static inline const char *error_handle(const Error *e)
{
	if (e)
	{
		const char *type = e->type;
		if (e->desc)
		{
			free((void *) e->desc);
			free((void *) e);
		}
		return type;
	}
	else
	{
		return NULL;
	}
}


//Event loop (Must call utils_init() before using these)
extern struct event_base *evbase;
extern struct evdns_base *evdns_base;

void evloop_hold();
void evloop_release();

//Initializer and finalizer
void utils_init();
void utils_shutdown();
