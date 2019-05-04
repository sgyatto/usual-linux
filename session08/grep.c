#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>

static void do_grep(const regex_t *pat, FILE *src);

int main(int argc, char *argv[])
{
	regex_t pat;
	int err;
	int i;

	if (argc < 2) {
		fputs("no pattern\n", stderr);
		exit(1);
	}

	err = regcomp(&pat, argv[1], 
			REG_EXTENDED	// POSIX 拡張正規表現を使用する。
			| REG_NOSUB 	// パターンマッチの成功・失敗のみを返す(regexecでnmatch, pmatchを無視)。
			| REG_NEWLINE	// 全ての文字にマッチするオペレータに改行をマッチさせない。 
			);
	if (err != 0) {
		char buf[4096];
		regerror(err, &pat, buf, sizeof buf);
		puts(buf);
		exit(1);
	}

	if (argc == 2) {
		// 標準入力に対してgrep
		do_grep(&pat, stdin);
	} else {
		FILE *f;
		for (i = 2; i < argc; i++) {
			f = fopen(argv[i], "r");
			if (f == NULL) {
				perror(argv[i]);
				exit(1);
			}
			do_grep(&pat, f);
			fclose(f);
		}
	}
	regfree(&pat);
	exit(0);
}

static void do_grep(const regex_t *pat, FILE *src)
{
	char buf[4096];

	while (fgets(buf, sizeof buf, src) != NULL) {
		if (regexec(pat, buf, 0, NULL, 0) == 0) {
			fputs(buf, stdout);
		}
	}
}