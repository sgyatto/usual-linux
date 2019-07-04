#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netdb.h>
#include <grp.h>
#include <pwd.h>

/* Constants ************************/

#define SERVER_NAME "LittleHTTP"
#define SERVER_VERSION "1.0"
#define HTTP_MINOR_VERSION 0
#define TIME_BUF_SIZE 64
#define BLOCK_BUF_SIZE 1024
#define LINE_BUF_SIZE 4096
#define MAX_REQUEST_BODY_LENGTH (1024 * 1024)
#define MAX_BACKLOG 5

/* Data Type Definitions ************/

struct HTTPHeaderField {
	char *name;
	char *value;
	struct HTTPHeaderField *next;
};

struct HTTPRequest {
	int protocol_minor_version;
	char *method;
	char *path;
	struct HTTPHeaderField *header;
	char *body;
	long length;
};

struct FileInfo {
	char *path;
	long size;
	int ok;  /* ファイルの存在有無 */
};

/* Function Prototypes **************/

typedef void (*sighandler_t)(int);
static void setup_environment(char *root, char *user, char *group);
static void become_daemon(void);
static void install_signal_handlers(void);
static void trap_signal(int sig, sighandler_t handler);
static void detach_children(void);
static void signal_exit(int sig);
static void noop_handler(int sig);
static int listen_socket(char *port);
static void server_main(int server_fd, char *docroot);
static void service(FILE *in, FILE *out, char *docroot);
static struct HTTPRequest *read_request(FILE *in);
static void read_request_line(struct HTTPRequest *req, FILE *in);
static struct HTTPHeaderField *read_header_field(FILE *in);
static void upcase(char *str);
static void free_request(struct HTTPRequest *req);
static long content_length(struct HTTPRequest *req);
static char *lookup_header_field_value(struct HTTPRequest *req, char *name);
static void respond_to(struct HTTPRequest *req, FILE *out, char *docroot);
static void do_file_response(struct HTTPRequest *req, FILE *out, char *docroot);
static void method_not_allowed(struct HTTPRequest *req, FILE *out);
static void not_found(struct HTTPRequest *req, FILE *out);
static void not_implemented(struct HTTPRequest *req, FILE *out);
static void output_common_header_fields(struct HTTPRequest *req, FILE *out, char *status);
static struct FileInfo *get_fileinfo(char *docroot, char *urlpath);
static char *build_fspath(char *docroot, char *urlpath);
static void free_fileinfo(struct FileInfo *info);
static char *guess_content_type(struct FileInfo *info);
static void *xmalloc(size_t sz);
static void log_exit(const char *fmt, ...);

/* Functions ************************/

#define USAGE "Usage: %s [--port=n] [--chroot --user=u --group=g] [--debug] <docroot>\n"

static int debug_mode = 0;

static struct option longopts[] = {
	{"debug", no_argument, &debug_mode, 1},
	{"chroot", no_argument, NULL, 'c'},
	{"user", required_argument, NULL, 'u'},
	{"group", required_argument, NULL, 'g'},
	{"port", required_argument, NULL, 'p'},
	{"help", no_argument, NULL, 'h'},
	{0,0,0,0}
};

int main(int argc, char *argv[])
{
	int server_fd;
	char *port = NULL;
	char *docroot;
	int do_chroot = 0;
	char *user = NULL;
	char *group = NULL;
	int opt;

	while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
		switch (opt) {
		case 0: /* --debug のときは0が返る */
			break;
		case 'c':
			do_chroot = 1;
			break;
		case 'u':
			user = optarg;
			break;
		case 'g':
			group = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		case 'h':
			fprintf(stdout, USAGE, argv[0]);
			exit(0);
		case '?':
			fprintf(stderr, USAGE, argv[0]);
			exit(1);
		}
	}
	if (optind != argc - 1) {
		fprintf(stderr, USAGE, argv[0]);
		exit(1);
	}
	/* docroot setting */
	docroot = argv[optind];

	/* chroot setting */
	if (do_chroot) {
		setup_environment(docroot, user, group);
		docroot = "";
	}

	/* signal */
	install_signal_handlers();

	/* listen socket */
	server_fd = listen_socket(port);

	if (!debug_mode) {
		/* 非debug modeならdeamon化 */
		openlog(SERVER_NAME, LOG_PID|LOG_NDELAY, LOG_DAEMON);
		become_daemon();
	}

	server_main(server_fd, docroot);
	exit(0);
}

static void setup_environment(char *root, char *user, char *group)
{
	struct passwd *pw;
	struct group *gr;

	if (user == NULL || group == NULL) {
		fprintf(stderr, "use both of --user and --group\n");
		exit(1);
	}

	/* 実グループID */
	gr = getgrnam(group);
	if (gr == NULL) {
		fprintf(stderr, "no such group: %s\n", group);
		exit(1);
	}
	if (setgid(gr->gr_gid) < 0) {
		perror("setgid(2)");
		exit(1);
	}

	/* 補助グループID */
	if (initgroups(user, gr->gr_gid) < 0) {
		perror("initgroups(2)");
		exit(1);
	}

	/* 実ユーザID */
	pw = getpwnam(user);
	if (pw == NULL) {
		fprintf(stderr, "no such user: %s\n", user);
		exit(1);
	}
	chroot(root); /* root権限が必要 */
	if (setuid(pw->pw_uid) < 0) {
		perror("setuid(2)");
		exit(1);
	}
}

static void become_daemon(void)
{
	int n;

	/* root dir に移動 */
	if (chdir("/") < 0)
		log_exit("chdir(2) failed: %s", strerror(errno));

	/* 標準入出力を/dev/nullにつなぐ */
	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);

	/* 制御端末切り離し */
	n = fork(); /* setsidは自身がプロセスグループリーダーだと失敗するので子を作る */
	if (n < 0) log_exit("fork(2) failed: %s", strerror(errno));
	if (n != 0) _exit(0); /* 親は終了。_exit()を使うのはfflush()対策 */
	/* setsid()で生成するセッションは制御端末を持たない */
	if (setsid() < 0) log_exit("setsid(2) failed: %s", strerror(errno));
}

static void install_signal_handlers(void)
{
	trap_signal(SIGTERM, signal_exit);
	detach_children();
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

static void detach_children(void)
{
	struct sigaction act;

	act.sa_handler = noop_handler; /* no operation */
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART | SA_NOCLDWAIT;
	if (sigaction(SIGCHLD, &act, NULL) < 0) {
		log_exit("sigaction() failed: %s", strerror(errno));
	}
}

static void signal_exit(int sig)
{
	log_exit("exit by signal %d", sig);
}

static void noop_handler(int sig)
{
	;
}

static int listen_socket(char *port)
{
	struct addrinfo hints, *res, *ai;
	int err;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((err = getaddrinfo(NULL, port, &hints, &res) != 0))
		log_exit(gai_strerror(err));
	for (ai = res; ai; ai = ai->ai_next) {
		int sock;

		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock < 0) continue;
		if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
			close(sock);
			continue;
		}
		if (listen(sock, MAX_BACKLOG) < 0) {
			close(sock);
			continue;
		}
		freeaddrinfo(res);
		return sock;
	}
	log_exit("failed to listen socket");
	return -1; /* NOT REACH */
}

static void server_main(int server_fd, char *docroot)
{
	for (;;) {
		struct sockaddr_storage addr;
		socklen_t addrlen = sizeof addr;
		int sock;
		int pid;

		sock = accept(server_fd, (struct sockaddr*)&addr, &addrlen);
		if (sock < 0) log_exit("accept(2) failed: %s", strerror(errno));
		pid = fork();
		if (pid < 0) exit(3);
		if (pid == 0) { /* child */
			FILE *inf = fdopen(sock, "r");
			FILE *outf = fdopen(sock, "w");

			service(inf, outf, docroot);
			exit(0);
		}
		/* 親は接続済みソケットを閉じて次の接続へ */
		close(sock);
	}
}

static void service(FILE *in, FILE *out, char *docroot)
{
	struct HTTPRequest *req;

	req = read_request(in);
	respond_to(req, out, docroot);
	free_request(req);
}

static struct HTTPRequest *read_request(FILE *in)
{
	struct HTTPRequest *req;
	struct HTTPHeaderField *h;

	req = xmalloc(sizeof(struct HTTPRequest));
	read_request_line(req, in);
	req->header = NULL;
	while ((h = read_header_field(in)) != NULL) {
		h->next = req->header;
		req->header = h;
	}
	req->length = content_length(req);
	if (req->length != 0) {
		if (req->length > MAX_REQUEST_BODY_LENGTH)
			log_exit("request body too long");
		req->body = xmalloc(req->length);
		if (fread(req->body, req->length, 1, in) < 1)
			log_exit("failed to read request body");
	} else {
		req->body = NULL;
	}
	return req;
}

static void read_request_line(struct HTTPRequest *req, FILE *in)
{
	/* 無制限に読み込むべき箇所ではないので固定長 */
	char buf[LINE_BUF_SIZE];
	char *p, *path;

	if (fgets(buf, LINE_BUF_SIZE, in) == NULL)
		log_exit("no request line");

	/* method */
	p = strchr(buf, ' '); 	/* 初めて半角スペースが出現した箇所のポインタを返す */
	if (p == NULL) log_exit("parse error on request line (1): %s", buf);
	*p++ = '\0';
	req->method = xmalloc(p - buf);
	strcpy(req->method, buf); /* '\0'までをコピー */
	upcase(req->method);

	/* path */
	path = p;
	p = strchr(p, ' ');
	if (p == NULL) log_exit("parse error on request line (2): %s", buf);
	*p++ = '\0';
	req->path = xmalloc(p - path);
	strcpy(req->path, path);

	/* protocol_minor_version */
	if (strncasecmp(p, "HTTP/1.", strlen("HTTP/1.")) != 0)
		log_exit("parse error on request line (3): %s", buf);
	p += strlen("HTTP/1.");
	req->protocol_minor_version = atoi(p);
}

static struct HTTPHeaderField *read_header_field(FILE *in)
{
	struct HTTPHeaderField *h;
	char buf[LINE_BUF_SIZE];
	char *p;

	if (fgets(buf, LINE_BUF_SIZE, in) == NULL)
		log_exit("failed to read request header field: %s", strerror(errno));

	/* 改行判定 */
	if ((buf[0] == '\n') || (strcmp(buf, "\r\n") == 0))
		return NULL;

	/* name */
	p = strchr(buf, ':');
	if (p == NULL) log_exit("parse error on request field: %s", buf);
	*p++ = '\0';
	h = xmalloc(sizeof(struct HTTPHeaderField));
	h->name = xmalloc(p - buf);
	strcpy(h->name, buf);

	/* value */
	/* 先頭にある space or tab の文字だけポインタを進める */
	p += strspn(p, " \t");
	h->value = xmalloc(strlen(p) + 1);
	strcpy(h->value, p);

	return h;
}

static void upcase(char *str)
{
	char *p;

	for (p = str; *p; p++) {
		*p = (char)toupper((int)*p);
	}
}

static void free_request(struct HTTPRequest *req)
{
	struct HTTPHeaderField *h, *head;

	head = req->header;
	while (head) {
		h = head;
		head = h->next;
		free(h->name);
		free(h->value);
		free(h);
	}
	free(req->method);
	free(req->path);
	free(req->body);
	free(req);
}

static long content_length(struct HTTPRequest *req)
{
	char *val;
	long len;

	val = lookup_header_field_value(req, "Content-Length");
	if (val == NULL) return 0;
	len = atol(val);
	if (len < 0) log_exit("negative Content-Length value");
	return len;
}

static char *lookup_header_field_value(struct HTTPRequest *req, char *name)
{
	struct HTTPHeaderField *h;

	for (h = req->header; h; h = h->next) {
		if (strcasecmp(h->name, name) == 0)
			return h->value;
	}
	return NULL;
}

static void respond_to(struct HTTPRequest *req, FILE *out, char *docroot)
{
	if (strcmp(req->method, "GET") == 0) {
		do_file_response(req, out, docroot);
	} else if(strcmp(req->method, "HEAD") == 0) {
		do_file_response(req, out, docroot);
	} else if(strcmp(req->method, "POST") == 0) {
		method_not_allowed(req, out);
	} else {
		not_implemented(req, out);
	}
}

static void do_file_response(struct HTTPRequest *req, FILE *out, char *docroot)
{
	struct FileInfo *info;

	info = get_fileinfo(docroot, req->path);
	if (!info->ok) {
		free_fileinfo(info);
		not_found(req, out);
		return;
	}
	output_common_header_fields(req, out, "200 OK");
	fprintf(out, "Content-Length: %ld\r\n", info->size);
	fprintf(out, "Content-Type: %s\r\n", guess_content_type(info));
	fprintf(out, "\r\n");
	if (strcmp(req->method, "HEAD") != 0) {
		int fd;
		char buf[BLOCK_BUF_SIZE];
		ssize_t n;

		fd = open(info->path, O_RDONLY);
		if (fd < 0)
			log_exit("failed to open %s: %s", info->path, strerror(errno));
		for (;;) {
			n = read(fd, buf, BLOCK_BUF_SIZE);
			if (n < 0)
				log_exit("failed to read %s: %s", info->path, strerror(errno));
			if (n == 0)
				break;
			if (fwrite(buf, 1, n, out) < n)
				log_exit("failed to write to socket");
		}
		close(fd);
	}
	fflush(out);
	free_fileinfo(info);
}

static void method_not_allowed(struct HTTPRequest *req, FILE *out)
{
	output_common_header_fields(req, out, "405 Method Not Allowed");
	fprintf(out, "Content-Type: text/html\r\n");
	fprintf(out, "\r\n");
	fprintf(out, "<html>\r\n");
	fprintf(out, "<head>\r\n");
	fprintf(out, "<title>405 Method Not Allowed</title>\r\n");
	fprintf(out, "</head>\r\n");
	fprintf(out, "<body>\r\n");
	fprintf(out, "<p>The request method %s is not allowed</p>\r\n", req->method);
	fprintf(out, "</body>\r\n");
	fprintf(out, "</html>\r\n");
	fflush(out);
}

static void not_implemented(struct HTTPRequest *req, FILE *out)
{
	output_common_header_fields(req, out, "501 Not Implemented");
	fprintf(out, "Content-Type: text/html\r\n");
	fprintf(out, "\r\n");
	fprintf(out, "<html>\r\n");
	fprintf(out, "<head>\r\n");
	fprintf(out, "<title>501 Not Implemented</title>\r\n");
	fprintf(out, "</head>\r\n");
	fprintf(out, "<body>\r\n");
	fprintf(out, "<p>The request method %s is not implemented</p>\r\n", req->method);
	fprintf(out, "</body>\r\n");
	fprintf(out, "</html>\r\n");
	fflush(out);
}

static void not_found(struct HTTPRequest *req, FILE *out)
{
	output_common_header_fields(req, out, "404 Not Found");
	fprintf(out, "Content-Type: text/html\r\n");
	fprintf(out, "\r\n");
	if (strcmp(req->method, "HEAD") != 0) {
		fprintf(out, "<html>\r\n");
		fprintf(out, "<head><title>Not Found</title></head>\r\n");
		fprintf(out, "<body><p>File not found</p></body>\r\n");
		fprintf(out, "</html>\r\n");
	}
	fflush(out);
}

static void output_common_header_fields(struct HTTPRequest *req, FILE *out, char *status)
{
	time_t t;
	struct tm *tm;
	char buf[TIME_BUF_SIZE];

	t = time(NULL);
	tm = gmtime(&t);
	if (tm == NULL) log_exit("gmtime() failed: %s", strerror(errno));
	strftime(buf, TIME_BUF_SIZE, "%a, %d %b %Y %H:%M:%S GMT", tm);
	fprintf(out, "HTTP/1.%d %s\r\n", HTTP_MINOR_VERSION, status);
	fprintf(out, "Date: %s\r\n", buf);
	fprintf(out, "Server: %s/%s\r\n", SERVER_NAME, SERVER_VERSION);
	fprintf(out, "Connection: close\r\n");
}

static struct FileInfo *get_fileinfo(char *docroot, char *urlpath)
{
	struct FileInfo *info;
	struct stat st;

	info = xmalloc(sizeof(struct FileInfo));
	info->path = build_fspath(docroot, urlpath);
	info->ok = 0;
	if (lstat(info->path, &st) < 0) return info;
	if (!S_ISREG(st.st_mode)) return info;
	info->ok = 1;
	info->size = st.st_size;
	return info;
}

static char *build_fspath(char *docroot, char *urlpath)
{
	char *path;

	path = xmalloc(strlen(docroot) + 1 + strlen(urlpath) + 1);
	sprintf(path, "%s/%s", docroot, urlpath);
	return path;
}

static void free_fileinfo(struct FileInfo *info)
{
	free(info->path);
	free(info);
}

static char *guess_content_type(struct FileInfo *info)
{
	return "text/plain"; /* temporary */
}

static void *xmalloc(size_t sz)
{
	void *p;

	p = malloc(sz);
	if (p == NULL)
		log_exit("failed to allocate memory");
	return p;
}

static void log_exit(const char *fmt, ...)
{
	va_list ap; /* 可変長引数用 */

	va_start(ap, fmt);
	if (debug_mode) {
		/* debug_mode のときは標準エラー出力が使える*/
		vfprintf(stderr, fmt, ap);
		fputc('\n', stderr);
	} else {
		vsyslog(LOG_ERR, fmt, ap);
	}
	va_end(ap);
	exit(1);
}