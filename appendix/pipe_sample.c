#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/**
 * pipe_sample
 * command1 (No argument) | command2 (No argument)
 * command1、command2用にそれぞれforkしてプロセスを作って実行する例
 * arg1: command1 
 * arg2: command2
 */
int main(int argc, char const *argv[])
{
	int fd[2];
	pid_t pid;

	if (argc < 3) {
		fprintf(stderr, "%s command1 command2\n", argv[0]);
	}

	if (pipe(fd) < 0) {
		fprintf(stderr, "Can't create pipe\n");
		exit(1);
	}

	pid = fork();
	if (pid < 0) {
		perror("fork error");
		exit(1);
	} else if (pid == 0) { /* child */
		close(fd[0]);
		dup2(fd[1], 1);
		close(fd[1]);
		execlp(argv[1], argv[1], NULL);
		/* 処理が戻ったらエラー */
		perror(argv[1]);
		exit(1);
	} 

	/* parent */
	int status;
	waitpid(pid, &status, 0);
	
	/* 親のwrite用のfdを閉じないとパイプにEOFが伝わらず、
		readがブロックされてしまうので注意。 */
	close(fd[1]); 
	
	pid_t pid2 = fork();
	if (pid2 < 0) {
		perror("fork error");
		exit(1);
	} else if (pid2 == 0) { /* child */
		close(fd[1]);
		dup2(fd[0], 0);
		close(fd[0]);
		execlp(argv[2], argv[2], NULL);
		/* 処理が戻ったらエラー */
		perror(argv[2]);
		exit(1);
	} 

	/* parent */
	waitpid(pid2, &status, 0);
	close(fd[0]);
	exit(0);
}
