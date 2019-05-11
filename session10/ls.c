#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

static void do_ls(char *path);

int main(int argc, char *argv[])
{
	int i;
	
	if (argc < 2) {
		fprintf(stderr, "%s: no arguments\n", argv[0]);
		exit(1);
	}
	for (i = 1; i < argc; i++) {
		do_ls(argv[i]);
	}
	exit(0);
}

static void do_ls(char *path)
{
	DIR *dir;
	struct dirent *ent;

	dir = opendir(path);
	if (dir == NULL) {
		perror(path);
		exit(1);
	}
	while ((ent = readdir(dir)) != NULL) {
		printf("%s\n", ent->d_name);
	}
	closedir(dir);
}
