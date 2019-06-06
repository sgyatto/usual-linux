#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define INIT_BUFSIZE 1024

char *my_getcwd(void)
{
    char *buf, *tmp;
    size_t size = INIT_BUFSIZE;
    
    buf = malloc(size);
    if (!buf) return NULL;
    for (;;) {
        errno = 0;
        if (getcwd(buf, size)) return buf; /* 成功 */
		if (errno != ERANGE) break; /* bufサイズに無関係なエラー */
		
		/* 失敗したらNULLが返り、errnoにERANGEがセットされる */
		
		size *= 2;
		tmp = realloc(buf, size);
		if (!tmp) break; /* 戻り値がNULLなら失敗 */
		buf = tmp;
    }
	free(buf);
	return NULL; /* 失敗 */
}

int main(int argc, char *argv[])
{
	printf("current: %s\n", my_getcwd());
}