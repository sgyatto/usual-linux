#include <stdarg.h>
#include <stdio.h>


/* Function Prototypes **************/

static void log_exit(char *fmt, ...);



/* Functions ************************/

static void log_exit(char *fmt, ...)
{
	va_list ap; /* 可変長引数用 */

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	exit(1);
}