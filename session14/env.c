#include <stdio.h>
#include <stdlib.h>

extern char **environ;

/**
 * env.c
 * 環境変数のセット(key=val)を表示
 */
int main(int argc, char *argv[])
{
	char **p;

	for (p = environ; *p; p++) {
		printf("%s\n", *p);
	}
	exit(0);
}