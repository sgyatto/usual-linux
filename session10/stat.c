#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

static char *filetype(mode_t mode);

int main(int argc, char *argv[])
{
	 struct stat st;
	
	if (argc != 2) {
		fprintf(stderr, "%s: wrong argument\n", argv[0]);
		exit(1);
	}
	if (lstat(argv[1], &st) < 0) {
		perror(argv[1]);
		exit(1);
	}
	/* type: st_mode から上位4bit取り出すとファイルタイプ*/
	printf("type\t%o (%s)\n", (st.st_mode & S_IFMT), filetype(st.st_mode));
	/* mode: st_mode から上位4bit落とすとfile permission bits (特殊モードビット含む)*/
	printf("mode\t%o\n", st.st_mode & ~S_IFMT);
	/* デバイス番号 */
	printf("dev\t%llu\n", (unsigned long long)st.st_dev);
	/* inode番号 */
	printf("ino\t%lu\n", (unsigned long)st.st_ino);
	/* デバイスファイル種別番号 */
	printf("rdev\t%llu\n", (unsigned long long)st.st_rdev);
	/* リンクカウント */
	printf("nlink\t%lu\n", (unsigned long)st.st_nlink);
	/* 所有ユーザID */
	printf("uid\t%d\n", st.st_uid);
	/* 所有グループID */
	printf("gid\t%d\n", st.st_gid);
	/* ファイルサイズ（バイト単位） */
	printf("size\t%ld\n", st.st_size);
	/* ファイのブロックサイズ */
	printf("blksize\t%lu\n", (unsigned long)st.st_blksize);
	/* ブロック数 */
	printf("blocks\t%lu\n", (unsigned long)st.st_blocks);
	/* 最終アクセス時刻 */
	printf("atime\t%s", ctime(&st.st_atime));
	/* 最終変更時刻 */
	printf("mtime\t%s", ctime(&st.st_mtime));
	/* 付帯情報の最終変更時刻 */
	printf("ctime\t%s", ctime(&st.st_ctime));
}

static char *filetype(mode_t mode) 
{
	if (S_ISREG(mode)) return "file";
	if (S_ISDIR(mode)) return "directory";
	if (S_ISCHR(mode)) return "chardev";
	if (S_ISBLK(mode)) return "blockdev";
	if (S_ISFIFO(mode)) return "fifo";
	if (S_ISLNK(mode)) return "symlink";
	if (S_ISSOCK(mode)) return "socket";
	return "unknown";
}