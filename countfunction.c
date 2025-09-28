#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>

// #ifndef COUNTFUNC
// #include "countfunction.h"
// #endif


// ================================================================================
#ifndef COUNTFUNC
	#define debug
#else
	#define debug 1 ? (void) 0 :
#endif

// ================================================================================
// count: 標準関数
int count_strlen;
size_t
countstrlen(const char *str)
{
	debug printf("countstrlen:\n");
// 	debug printf(" %s\n", str);

	count_strlen++;
	return strlen(str);
}


int count_strstr;
char *
countstrstr(const char *haystack, const char *needle)
{
	debug printf("countstrstr:\n");
// 	debug printf(" %s, %s\n", haystack, needle);

	count_strstr++;
	return strstr(haystack, needle);
}


int count_strcmp;
int
countstrcmp(const char *s1, const char *s2)
{
	debug printf("countstrcmp:\n");
// 	debug printf(" %s, %s\n", s1, s2);

	count_strcmp++;
	return strcmp(s1, s2);
}


int count_strncmp;
int
countstrncmp(const char *s1, const char *s2, size_t n)
{
	debug printf("countstrncmp:\n");
// 	debug printf(" %s, %s, %ld\n", s1, s2, n);

	count_strncmp++;
	return strncmp(s1, s2, n);
}


int count_strcat;
char *
countstrcat(char *dest, const char *src)
{
	debug printf("countstrcat:\n");
// 	debug printf(" %s, %s\n", dest, src);

	count_strcat++;
	return strcat(dest, src);
}


int count_strcpy;
char *
countstrcpy(char *dest, const char *src)
{
	debug printf("countstrcpy:\n");
// 	debug printf(" %s, %s\n", dest, src);

	count_strcpy++;
	return strcpy(dest, src);
}


int count_strncpy;
char *
countstrncpy(char *dest, const char *src, size_t n)
{
	debug printf("countstrncpy:\n");
// 	debug printf(" %s, %s, %ld\n", dest, src, n);

	count_strncpy++;
	return strncpy(dest, src, n);
}


int count_strchr;
char *
countstrchr(const char *s, int c)
{
	debug printf("countstrchr:\n");
// 	debug printf(" %s, %d\n", s, c);

	count_strchr++;
	return strchr(s, c);
}


int count_malloc;
void *
countmalloc(size_t size)
{
	debug printf("countmalloc:\n");
// 	debug printf(" %ld\n", size);

	count_malloc++;
	return malloc(size);
}


int count_calloc;
void *
countcalloc(size_t nmemb, size_t size)
{
	debug printf("countcalloc:\n");
// 	debug printf(" %ld, %ld\n", nmemb, size);

	count_calloc++;
	return calloc(nmemb, size);
}


int count_free;
void
countfree(void *ptr)
{
// 	debug printf("countfree:\n");

	count_free++;
	free(ptr);
}


int count_stat;
int
countstat(const char *pathname, struct stat *statbuf)
{
// 	debug printf("countstat:\n");
// 	debug printf(" %s, [statbuf]\n", pathname);

	count_stat++;
	return stat(pathname, statbuf);
}


int count_lstat;
int
countlstat(const char *pathname, struct stat *statbuf)
{
// 	debug printf("countlstat:\n");
// 	debug printf(" %s, [statbuf]\n", pathname);

	count_lstat++;
	return lstat(pathname, statbuf);
}


int count_readlink;
ssize_t
countreadlink(const char *pathname, char *buf, size_t bufsize)
{
// 	debug printf("countreadlink:\n");
// 	debug printf(" %s, %s, %ld\n", pathname, buf, bufsize);

	count_readlink++;
	return readlink(pathname, buf, bufsize);
}


int count_memset;
void *
countmemset(void *s, int c, size_t n)
{
	debug printf("countmemset:\n");
// 	debug printf(" [vod *], %d, %ld\n", c, n);

	count_memset++;
	return memset(s, c, n);
}


int count_printf;
int
countprintf(const char *format, ...)
{
// 	debug printf("countprintf:\n");
// 	debug printf(" [format], ...\n");

	count_printf++;
	int ret = 0;

	va_list args;
	va_start(args, format);
	ret += vprintf(format, args);
	va_end(args);
	return ret;
}


// --------------------------------------------------------------------------------
// #define MYSCANDIR
#ifdef MYSCANDIR
#include <dirent.h>

int count_scandir;
int
myscandir(const char *dirp, struct dirent ***namelist, int (*filter)(const struct dirent *), int (*compar)(const struct dirent **, const struct dirent **))
{
	printf("myscandir:\n");

	DIR *dir;
	struct dirent *entry;
	struct dirent **list = NULL;
	int count = 0;
	int capacity = 10240; // 初期容量

	if ((dir = opendir(dirp)) == NULL) {
// 		perror("opendir");
// 		printf(" =>opendir: %s\n", dirp);
		return -1;
	}

	// !! 最後に free() する
	list = countmalloc(capacity * sizeof(struct dirent *));
	if (!list) {
		perror("malloc");
		closedir(dir);
		return -1;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (filter && !filter(entry)) {
			continue;
		}

		if (count >= capacity) {
			capacity *= 2;
			struct dirent **new_list = realloc(list, capacity * sizeof(struct dirent *));
			if (!new_list) {
				perror("realloc");
				countfree(list);
				closedir(dir);
				return -1;
			}
			list = new_list;
		}

		list[count] = countmalloc(sizeof(struct dirent));
		if (!list[count]) {
			perror("malloc");
			for (int i = 0; i < count; i++) {
				countfree(list[i]);
			}
			countfree(list);
			closedir(dir);
			return -1;
		}
		memcpy(list[count], entry, sizeof(struct dirent));
		count++;
	}

	closedir(dir);

	if (compar) {
		qsort(list, count, sizeof(struct dirent *), (int (*)(const void *, const void *))compar);
	}

	*namelist = list;
	count_scandir++;

	return count;
}
#define scandir myscandir
#endif


// --------------------------------------------------------------------------------
#define LABEL "\033[1m"
#define RESET "\033[0m"

// label 色 + 太字
void
printColor(char *str)
{
	countprintf(LABEL);
	countprintf(str);
	countprintf(RESET);
}


// --------------------------------------------------------------------------------
void
showCountFunc(void)
{
	printColor("count:\n");

#ifdef MYSCANDIR
	printf(" scandir: %7d\n", count_scandir);
#endif

	printf(" strlen:  %7d\n", count_strlen);
	printf(" strstr:  %7d\n", count_strstr);
	printf(" strcmp:  %7d\n", count_strcmp);
	printf(" strncmp: %7d\n", count_strncmp);
	printf(" strcat:  %7d\n", count_strcat);
	printf(" strcpy:  %7d\n", count_strcpy);
	printf(" strncpy: %7d\n", count_strncpy);
	printf(" strchr:  %7d\n", count_strchr);

	printf("\n");
	printf(" calloc:  %7d\n", count_calloc);
	printf(" malloc:  %7d\n", count_malloc);
	printf(" free:    %7d\n", count_free);
// 	printf(" malloc - free: %d\n", count_malloc - count_free);

	printf("\n");
	printf(" stat:    %7d\n", count_stat);
	printf(" lstat:   %7d\n", count_lstat);
	printf(" readlink:%7d\n", count_readlink);
	printf(" memset:  %7d\n", count_memset);
	printf(" printf:  %7d\n", count_printf);
}


// ================================================================================
// 単体の時のデバッグ用
#ifndef COUNTFUNC
int
main(void)
{
	countprintf("countfunction:\n");

	char str[] = "hello, world";
	int len;
	assert(countstrlen(str) == strlen(str));
	len = countstrlen(str);
	countprintf("len: %d\n", len);

	char s[] = "rl";
	assert(countstrstr(str, s) == strstr(str, s));

	char str2[] = "hello, world.";
	assert(countstrcmp(str, str2) == strcmp(str, str2));
	assert(countstrncmp(str, str2, 5) == strncmp(str, str2, 5));

	{
		char tmp1[32] = "tmp";
		char tmp2[32] = "tmp";

		assert(strlen(countstrcat(tmp1, s)) == strlen(strcat(tmp2, s)));
	}

	{
		char tmp1[32] = "";
		char tmp2[32] = "";

		assert(strlen(countstrcpy(tmp1, s)) == strlen(strcpy(tmp2, s)));
		assert(strlen(countstrncpy(tmp1, s, 1)) == strlen(strncpy(tmp2, s, 1)));
	}

	assert(countstrchr(str, ',') == strchr(str, ','));

	countprintf("\n");
	{
		char *arr;

		arr = countmalloc(10);
		for (int i=0; i<10; i++) {
			arr[i] = i;
		}
		for (int i=0; i<10; i++) {
			printf("%d, ", arr[i]);
		}
		printf("\n");

		// malloc() + memset() は calloc() で
		arr = countcalloc(20, sizeof(char));
		for (int i=0; i<20; i++) {
			arr[i] = i;
		}
		for (int i=0; i<20; i++) {
			printf("%d, ", arr[i]);
		}
		printf("\n");

		countmemset(arr, 100, sizeof(char) *20);
		for (int i=0; i<20; i++) {
			printf("%d, ", arr[i]);
		}
		printf("\n");

		countfree(arr);
	}


	countprintf("\n");
	{
		#include <dirent.h>
		#include <unistd.h>
		#include <sys/stat.h>
		struct dirent **namelist;
		int n;

		struct stat sb;

		n = scandir("./", &namelist, NULL, alphasort);
#ifndef MYSCANDIR
		count_malloc += n;
#endif
		if (n < 0) {
			perror("scandir");
			return -1;
		}

		while (n--) {
			countstat(namelist[n]->d_name, &sb);

			if (countlstat(namelist[n]->d_name, &sb) != -1) {
				switch (sb.st_mode & S_IFMT) {
				  case S_IFLNK: {
					  char linkname[256];
					  countreadlink(namelist[n]->d_name, linkname, sizeof(linkname));
					  countprintf(" ");
					  printColor(namelist[n]->d_name);
					  break;
				  }
				  default:
					countprintf(" %s", namelist[n]->d_name);
					break;
				}
			} else {
				countprintf(" error:%", namelist[n]->d_name);
			}
			countprintf("\n");

			countfree(namelist[n]);
		}
#ifdef MYSCANDIR
		countfree(namelist);
#endif
	}

	countprintf("\n");
	showCountFunc();

	return 0;
}
#endif
