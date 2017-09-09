#include <src/incl.h>

//Runs a test function.
#define test_run(expr) \
	do { \
		if (!(expr)) \
		{ \
			fprintf(stderr, "[%d] Failed test: [%s]\n", __LINE__, #expr); \
			abort(); \
		} \
	} while (0)

//Evaluates expression and in case of error prints it.
#define test_error_handle(expr) \
	do { \
		const Error *e; \
		e = (expr); \
		if (e) \
		{ \
			fprintf(stderr, "[%d] Error: %s\n", __LINE__, error_desc(e)); \
			error_handle(e); \
			return 0; \
		} \
	} while (0)

//Aborts the program if there is error
#define abort_on_error(expr) \
	do { \
		const Error *e; \
		e = (expr); \
		if (e) \
		{ \
			abort_with_error("[%s:%d]: Expression [%s] failed: %s",\
					__FILE__, __LINE__, #expr, error_desc(e)); \
		} \
	} while (0)

//Script object
typedef struct
{
	size_t len;
	unsigned char *data;
	unsigned char *mask;
} ScriptElement;

/* Script as string
 * [0-9a-f][0-9a-f]: one byte
 * k:       Dont care
 * l:       Start new dialog
 * p-z:     Variables
 * All other characters are ignored
 */

void set_variable_loc(char id, void *data, size_t len);

ScriptElement *script_build(char *src);

void script_free(ScriptElement *script);

//Callback
typedef struct _Actor Actor;
typedef void (*ActorFinishCB)(Actor *p, void *data);


void actor_destroy(Actor *p);

Actor *actor_create_server(SocketAddress addr, ScriptElement *script);

Actor *actor_create_client(SocketAddress addr, ScriptElement *script);

void actor_set_callback(Actor *p, ActorFinishCB cb, void *cb_data);

