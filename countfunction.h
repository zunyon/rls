extern size_t countstrlen(const char *str);
extern char * countstrstr(const char *haystack, const char *needle);
extern int countstrcmp(const char *s1, const char *s2);
extern int countstrncmp(const char *s1, const char *s2, size_t n);
extern char * countstrcat(char *dest, const char *src);
extern char * countstrcpy(char *dest, const char *src);
extern char * countstrncpy(char *dest, const char *src, size_t n);
extern char * countstrchr(const char *s, int c);

extern void * countmalloc(size_t size);
extern void * countcalloc(size_t nmemb, size_t size);
extern void countfree(void *ptr);

extern int countstat(const char *pathname, struct stat *statbuf);
extern int countlstat(const char *pathname, struct stat *statbuf);
extern ssize_t countreadlink(const char *pathname, char *buf, size_t bufsize);
extern void * countmemset(void *s, int c, size_t n);
extern int countprintf(const char *format, ...);

extern void showCountFunc(void);


// ================================================================================
// �u������
#define strlen countstrlen
#define strstr countstrstr
#define strcmp countstrcmp
#define strncmp countstrncmp
#define strcat countstrcat
#define strcpy countstrcpy
#define strncpy countstrncpy
#define strchr countstrchr

#define malloc countmalloc
#define calloc countcalloc
#define free countfree

// #define stat countstat			# struct stat ����������
#define lstat countlstat
#define readlink countreadlink
#define memset countmemset
#define printf countprintf
