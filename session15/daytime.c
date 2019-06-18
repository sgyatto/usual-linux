#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

static int open_connection(char *host, char *service);

/*
 * daytime クライアント
 *  
 */
int main(int args, char *argv[])
{
	int sock;
	FILE *f;
	char buf[1024];
	/* ここではホスト名とサービス名を渡す */
	sock = open_connection((args > 1 ? argv[1] : "localhost"), "daytime");
	
	f = fdopen(sock, "r");
	if (!f) {
		perror("fdopen(3)");
		exit(1);
	}
	fgets(buf, sizeof(buf), f);
	fclose(f);
	fputs(buf, stdout);
	exit(0);
}

/*
 * getaddrinfo(3)を使用してホスト名・サービス名からアドレス情報を取得した後、
 * socket(2), connect(2)で、IPv4 or IPv6の上手くいくほうで接続し、
 * socket記述子を返す。
 */
static int open_connection(char *host, char *service)
{
	int sock;
	struct addrinfo hints, *res, *ai;
	int err;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	/* 名前解決 */
	if ((err = getaddrinfo(host, service, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo(3): %s\n", gai_strerror(err));
		exit(1);
	}
	
	for (ai = res; ai; ai = ai->ai_next) {
		/* ソケット作成 */
		sock = socket(ai->ai_family, ai->ai_socktype, 0);
		if (sock < 0) {
			/* 取得失敗。次のアドレス情報へ */
			continue;
		}

		/* 接続 */
		if (connect(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
			/* 接続失敗。ソケットを閉じて次のアドレス情報へ */
			close(sock);
			continue;
		} 

		/* success */
		freeaddrinfo(res);
		return sock;
	}
	fprintf(stderr, "socket(2)/connect(2) failed\n");
	freeaddrinfo(res);
	exit(1);
}