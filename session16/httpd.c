#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* Function Prototypes **************/

static void *xmalloc(size_t sz);
static void log_exit(char *fmt, ...);

/* Functions ************************/

static void *xmalloc(size_t sz)
{
	void *p;

	p = malloc(sz);
	if (p == NULL)
		log_exit("failed to allocate memory");
	return p;
}

static void log_exit(char *fmt, ...)
{
	va_list ap; /* 可変長引数用 */

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	exit(1);
}