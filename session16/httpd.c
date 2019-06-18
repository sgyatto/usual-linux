#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

/* Function Prototypes **************/

typedef void (*sighandler_t)(int);
static void install_signal_handlers(void);
static void trap_signal(int sig, sighandler_t handler);
static void signal_exit(int sig);
static void *xmalloc(size_t sz);
static void log_exit(char *fmt, ...);

/* Functions ************************/

static void install_signal_handlers(void)
{
	trap_signal(SIGPIPE, signal_exit);
}

static void trap_signal(int sig, sighandler_t handler)
{
	struct sigaction act;

	act.sa_handler = handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	if (sigaction(sig, &act, NULL) < 0)
		log_exit("sigaction() failed: %s", strerror(errno));
}

static void signal_exit(int sig)
{
	log_exit("exit by signal %d", sig);
}

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