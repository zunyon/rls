/*
	The MIT License (MIT)

	Copyright(c) 2025 zunyon

	Permission is hereby granted, free of charge, to any person obtaining a copy of
	this software and associated documentation files (the “Software”), to deal in
	the Software without restriction, including without limitation the rights to use,
	copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
	Software, and to permit persons to whom the Software is furnished to do so,
	subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
	FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
	ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
	DEALINGS IN THE SOFTWARE.
*/

// ================================================================================
// version.h
#define VERSION "0.2.0"

#define INCDATE
// make release で verion を固定した時間
#define BYEAR "2025"
#define BDATE "04/19"
#define BTIME "17:58:59"

// --------------------------------------------------------------------------------
// Last Update:
// my-last-update-time "2025, 04/29 11:34"
// 2024, 09/01 Ver. 0.1.0
// 2025, 01/13 Ver. 0.2.0

// 一覧リスト表示
//   ファイル名のユニークな部分の識別表示
//   指定ファイル情報を含むファイルの選別表示

// --------------------------------------------------------------------------------
// 20250429 から追加変更
// free() の場所変更
// initColor() の変更、color_txt[] のデフォルト値変更
// -tb, -te を大文字に変更
// lstat() 時に inode, nlink を追加
// kind の追加
// label の追加


// ================================================================================
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <grp.h>
#include <pwd.h>
#include <locale.h>
#include <errno.h>


// ================================================================================
// #define DEBUG
#ifdef DEBUG
 #define debug
 #else
 #define debug 1 ? (void) 0 :
#endif


// ================================================================================
#define FNAME_LENGTH 256		// ファイル/ディレクトリ名
#define DATALEN 32				// mode, date, owner, group など
#define UNIQUE_LENGTH 32		// unique かどうか、最長連続 32 文字までカウント
int *count_chklen;


// ================================================================================
// 代替実装
#define MYTOLOWER
#define MYSTRCASESTR	// 標準に無い、GNU strcasestr() の代わりに実装
#define MYROUND			// round() の -lm が不要になるように実装


#ifdef MYTOLOWER
#include <limits.h>
// - を _ に変更する
const char *upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-";
const char *lower = "abcdefghijklmnopqrstuvwxyz_";
int map[UCHAR_MAX + 1];

#define myTolower(i) map[(unsigned char)i]
#else
	#define myTolower tolower
#endif


// -P で使用、needle はすでに小文字化済み
#ifdef MYSTRCASESTR
char *
myStrcasestr(const char *haystack, const char *needle)
{
	for (; *haystack; haystack++) {
		if (myTolower((unsigned char) *haystack) == (unsigned char) *needle) {
			const char *h, *n;

			for (h = haystack, n = needle; *h && *n; h++, n++) {
				if (myTolower((unsigned char) *h) != (unsigned char) *n) {
					break;
				}
			}

			if (!*n) {
				return (char *) haystack;
			}
		}
	}

	return NULL;
}
#else
	#define myStrcasestr strcasestr
#endif


// -r で使用
#ifdef MYROUND
double
myRound(double d)
{
	double ret = 0.0;

	ret = (int) ((d + 0.05) * 10.0) / 10.0;
// 	printf("d:%f, ret:%f\n", d, ret);

	return ret;
}
#else
	#include <math.h>
	#define myRound round
#endif


// あとは、myStrlen()


// ================================================================================
// 重複文字列リスト、、、unique 文字列をファイル名から検索しないように
struct DLIST {
	char dupword[UNIQUE_LENGTH];		// 最大 32 文字
	struct DLIST *left;
	struct DLIST *right;
};


void
freeDuplist(struct DLIST *node)
{
	if (node == NULL) {
		return;
	}

	freeDuplist(node->left);
	freeDuplist(node->right);
	free(node);
}


#ifdef DEBUG
void
displayDuplist(struct DLIST *node)
{
	if (node == NULL) {
		return;
	}

	// 左から順番に表示
	displayDuplist(node->left);
	printf("dup: [%s]\n", node->dupword);
	displayDuplist(node->right);
}
#endif


struct DLIST *
mallocDuplist(char *word, int len)
{
	struct DLIST *new;

	new = malloc(sizeof(struct DLIST));
	if (new == NULL) {
		perror("malloc");
		printf(" =>You have no memory. %zu\n", sizeof(struct DLIST));

		exit(EXIT_FAILURE);
	}

	strncpy(new->dupword, word, len);
	new->dupword[len] = '\0';

	new->left = NULL;
	new->right = NULL;

	return new;
}


// 必ず文字列を登録するし、多重で登録しないので、チェックは行わない
void
addDuplist(struct DLIST *p, char *word, int len)
{
	struct DLIST *prev = NULL;
	int ret = 0;

	// 該当箇所まで移動
	while (p) {
		prev = p;			// 一つ上を覚えておく

		ret = strncmp(word, p->dupword, len);
		if (ret < 0) {
			p = p->left;
		} else {
			p = p->right;
		}
		// cc -S で見ると、if の方が 1 命令多くなっている (速度は同じ、.o の量が増える)
// 		p = (ret < 0) ? p->left : p->right;
	}

	// 無かった方に新規登録
	struct DLIST *new_node = mallocDuplist(word, len);
	if (ret < 0) {
		prev->left = new_node;
	} else {
		prev->right = new_node;
	}
}


#ifdef DEBUG
int *count_searchDuplist;
#endif


// 検索履歴から、重複しているか確認する
int
searchDuplist(struct DLIST *p, char *word, int len)
{
	while (p) {

#ifdef DEBUG
		*count_searchDuplist += 1;
#endif

		int ret = strncmp(word, p->dupword, len);
		// 見つかった
		if (ret == 0) {
			return 1;
		}

		if (ret < 0) {
			p = p->left;
		} else {
			p = p->right;
		}
	}

	return 0;
}


// ================================================================================
typedef enum {
	base,
	normal,
	dir,
	fifo,
	socket,
	device,

	error,
	paint,			// printUnique(), printMatchedString() で使用

	label,
	reset,

	ListCount
} CLIST;

// カラーリスト
#define COLOR_TEXT 64
char colorlist[ListCount][COLOR_TEXT];


// --------------------------------------------------------------------------------
// ファイルの種類による色分け
// RLS_COLORS の仕切り
#define DELIMITER ":"
#define ENVNAME "RLS_COLORS"

char default_color_txt[ListCount * COLOR_TEXT];


#define printEscapeColor(color) printf("%s", colorlist[color]);

void
printStr(CLIST color, const char *str)
{
	// 色付け後にリセット、、、背景 or 文字色だけの設定が引き継がれないように
	printEscapeColor(color);
	printf("%s", str);
	printEscapeColor(reset);
}


void
colorUsage(void)
{
	printStr(base,   "base");   printf(", ");
	printStr(normal, "normal"); printf(", ");
	printStr(dir,    "dir");    printf(", ");
	printStr(fifo,   "fifo");   printf(", ");
	printStr(socket, "socket"); printf(", ");
	printStr(device, "device"); printf(", ");
	printStr(error,  "error");  printf(", ");
	printStr(paint,  "paint");  printf(", ");
	printStr(label,  "label");

	printf("\n");
}


// 変数名を文字列にする
#define toStr(n) #n

void
initColor(int default_color, char *argcolor)
{
	char env[COLOR_TEXT * ListCount];

	char *cname[ListCount] = {
		toStr(base),
		toStr(normal),
		toStr(dir),
		toStr(fifo),
		toStr(socket),
		toStr(device),
		toStr(error),
		toStr(paint),
		toStr(label),
		toStr(reset)
	};

	debug printf("initColor:\n");
	debug printf(" ");
	debug colorUsage();

	// --------------------------------------------------------------------------------
	// default color 表示、getenv() しない
	if (default_color) {
		return;
	}

	// --------------------------------------------------------------------------------
	char *from;
	if (argcolor[0] == '\0') {
		// 引数が無ければ、getenv() で色を設定
		if ((from = getenv(ENVNAME)) == NULL) {
			debug printf(" =>envname:[%s]\n", ENVNAME);

			return;
		}
		debug printf("getenv(\"%s\"): %s\n", ENVNAME, from);
	} else {
		// 引数の色指定
		from = argcolor;
		debug printf(" argv: %s\n", from);
	}
	strcpy(env, from);

	char *p;
	int usage = 0;

	int overwritelist[ListCount];
	memset(overwritelist, -1, sizeof(overwritelist));

	p = strtok(env, DELIMITER);
	while (p) {
		char name[COLOR_TEXT];
		char valuechar[COLOR_TEXT];
		// strchr() の失敗チェックを先に行う
// 		if (sscanf(p, "%[^=]=%63s", name, valuechar) != 2) {
		if (sscanf(p, "%[^=]=%s", name, valuechar) != 2) {
			usage++;
			break;
		}

		debug printf(" name:[%s],\tvalue:[%s],\t%s\n", name, valuechar, p);

		int found = 0;
		for (int i=0; i<(int) (sizeof(cname) / sizeof(cname[0])); i++) {
			if (strcmp(name, cname[i]) == 0) {
				char ctxt[128];

				// 指定された項目は、1 回目は上書き、それ以降は追記
				if (overwritelist[i] == -1) {
					overwritelist[i] = 0;
					colorlist[i][0] = '\0';
				}

				// 256 色: 3000 が fore、4000 が back
				if (atoi(valuechar) >= 3000) {
					if (colorlist[i][0] == '\0') {
						sprintf(ctxt, "\033[%c8;5;%sm", valuechar[0], valuechar+1);
					} else {
						// 追記する記載
						sprintf(ctxt, ";%c8;5;%sm", valuechar[0], valuechar+1);
						colorlist[i][strlen(colorlist[i])-1] = '\0';		// 最後の 'm' を削除
					}
				} else {
					// 8 色や、追記の属性
					if (colorlist[i][0] == '\0') {
						sprintf(ctxt, "\033[%sm", valuechar);
					} else {
						// 追記する記載
						sprintf(ctxt, ";%sm", valuechar);
						colorlist[i][strlen(colorlist[i])-1] = '\0';		// 最後の 'm' を削除
					}
				}

				strcat(colorlist[i], ctxt);
				found = 1;
				break;
			}
		}

		if (found == 0) {
			// 知らない変数名
			usage++;
			break;
		}

		p = strtok(NULL, DELIMITER);
	}

#ifdef DEBUG
	// どこを書き換えたか確認
	for (int i=0; i<ListCount; i++) {
		printf(" %s: %d, len:%ld\n", cname[i], overwritelist[i], strlen(colorlist[i]));
	}
#endif

	if (usage) {
		// 変な name
		printf("initColor:\n");
		printf("  bad color setting: [%s]\n", p);
		printf("  %s\n", from);
		printf("  ");
		colorUsage();
		exit(EXIT_FAILURE);
	}

#ifdef DEBUG
	printf("initColor end:\n");
	printf(" ");
	colorUsage();
#endif
}


// ================================================================================
// 表示するファイルの情報
#define ListCountd FNAME_LENGTH		// info に属させる項目数、formatListString に合わせる

struct FNAME {
	struct stat sb;					// st_mode, st_mtime, st_size, st_uid, st_gid を使用

	char *info[ListCountd];			// この構造体の文字列要素の先頭
		char inode[DATALEN];			// inode
		char nlink[DATALEN];			// hard links
		char mode[DATALEN];				// mode bits
		char owner[DATALEN];			// owner
		char group[DATALEN];			// group
		char size[DATALEN];				// size の文字列
		char count[DATALEN];			// ディレクトリに含まれているファイル数と、size の混合
		char date[DATALEN];				// mtime 日付
		char time[DATALEN];				// 日時
		char week[DATALEN];				// 曜日
		char path[FNAME_LENGTH];		// 絶対パスで指定されたパス名
		char *name;						// 表示用ファイル名
		char kind[2];					// 種類
		char linkname[FNAME_LENGTH];	// link 名
		char errnostr[FNAME_LENGTH +8];	// lstat() のエラー

	int print_length;				// 表示時のファイル名の長さ、全角考慮 myStrlen()
	int length;						// ファイル名の長さ strlen()
	char *lowername;				// 比較用
	int uniquebegin;				// unique の開始
	int uniqueend;					// unique の終了、paint_string で該当した場合 1 を代入、aggregate でカウント
	CLIST color;					// 表示色

	int sourcelist;					// check の対象にするか/しないか
	int targetlist;					// uniqueCheck(), uniqueCheckFirstWord() の対象にするか/しないか
	int showlist;					// 表示するか/しないか、下記 #define

	// linkedlist
	int prevn;						// skip 対象を飛ばした、前の FNAME の配列番号
	int nextn;						// skip 対象を飛ばした、次の FNAME の配列番号
};

#define SHOW_NONE   0	// 表示しない
#define SHOW_SHORT  1	// printShort()
#define SHOW_LONG   2	// printLong()


// 対象をディレクトリとして扱うかどうかのチェック
#define IS_DIRECTORY(data) ((data).mode[0] == 'd' || (data).linkname[strlen((data).linkname) -1] == '/')


// --------------------------------------------------------------------------------
void
makeMode(struct FNAME *p)
{
	char c;
	int st_mode = p->sb.st_mode;

	switch (st_mode & S_IFMT) {
		case S_IFDIR:  c = 'd'; p->color = dir;    p->kind[0] = '/'; break;	// dir
		case S_IFBLK:  c = 'b'; p->color = device;                   break;	// block device      /dev/
		case S_IFCHR:  c = 'c'; p->color = device;                   break;	// character device  /dev/
		case S_IFIFO:  c = '|'; p->color = fifo;   p->kind[0] = '|'; break;	// FIFO/pipe         /tmp/fish.ryoma/
		case S_IFSOCK: c = 's'; p->color = socket; p->kind[0] = '='; break;	// socket            /tmp/
		case S_IFLNK: {										// symlink
			struct stat sb;

			c = 'l'; p->kind[0] = '@';
			// symlink 先のファイル名
			if (readlink(p->name, p->linkname, sizeof(p->linkname)) == -1) {
				perror("readlink");
				printf(" =>p->name:[%s]\n", p->name);
				printf(" =>p->linkname:[%s]\n", p->linkname);

				exit(EXIT_FAILURE);
			}

			// link 先の sb を取得、link 先がディレクトリか
			if (lstat(p->linkname, &sb) == -1) {
				// データが取れなかったから異常
				sprintf(p->errnostr, "-> %s", strerror(errno));
				p->color = error;
			} else {
				// ディレクトリだったら / を追記
				if ((sb.st_mode & S_IFMT) == S_IFDIR) {
					strcat(p->linkname, "/");
					p->color = dir;
					p->kind[0] = '/';
				}
			}
		}
		break;

		default: c = '-'; break;		// ls は '-', exa は '.'
	}

	// --------------------------------------------------------------------------------
	const char *modetxt[] = {
		"---",
		"--x",
		"-w-",
		"-wx",
		"r--",
		"r-x",
		"rw-",
		"rwx"
	};

	sprintf(p->mode, "%c%s%s%s",
			c,
			modetxt[(st_mode & 0700) >> 6],
			modetxt[(st_mode & 0070) >> 3],
			modetxt[ st_mode & 0007      ]);

	// --------------------------------------------------------------------------------
	// setuid, setgid, sticky bit の対応
	// /user/include/linux/stat.h
	switch (st_mode & 0007000) {
		case S_ISUID: p->mode[3] = 's'; break;		// /bin/umount
		case S_ISGID: p->mode[6] = 's'; break;		// /bin/write.ul
		case S_ISVTX: p->mode[9] = 't'; break;		// /tmp/
	}
}


// --------------------------------------------------------------------------------
void
makeDate(struct FNAME *p, time_t lt)
{
	struct tm *t = localtime(&(p->sb.st_mtime));

	// 現時刻から半年前かチェック (秒でチェック、60 * 60 * 24 * 365 / 2 = 15768000)
	if (difftime(lt, p->sb.st_mtime) < 15768000) {
		strftime(p->date, DATALEN, "%b %e %H:%M", t);
	} else {
		strftime(p->date, DATALEN, "%b %e  %Y", t);
	}

	// 日時
	strftime(p->time, DATALEN, "%Y, %m/%d %H:%M:%S", t);
	// 曜日
	strftime(p->week, DATALEN, "%a", t);
}


void
makeReadableDate(struct FNAME *p, time_t lt)
{
	double delta = difftime(lt, p->sb.st_mtime);
	// /proc や /sys が未来
	int later = 0;
	if (delta < 0) {
		delta *= -1.0;
		later = 1;
	}

	do {
// 		if (delta < 30) {          strcpy( p->date,    "just now"); break; }
		if (delta < 60) {          sprintf(p->date,   "%dsec ago", (int) delta); break; }
		if (delta < 3600) {        sprintf(p->date,   "%dmin ago", (int) delta / 60); break; }
		if (delta < 3600*24) {     sprintf(p->date,  "%dhour ago", (int) delta / 3600); break; }
// 		if (delta < 3600*24*2) {   strcpy( p->date,   "yesterday"); break; }
		if (delta < 3600*24*7) {   sprintf(p->date,   "%dday ago", (int) delta / (3600*24)); break; }
		if (delta < 3600*24*31) {  sprintf(p->date,  "%dweek ago", (int) delta / (3600*24*7)); break; }
		if (delta < 3600*24*365) { sprintf(p->date, "%dmonth ago", (int) delta / (3600*24*31)); break; }
		sprintf(p->date,  "%dyear ago", (int) delta / (3600*24*365));
	} while (0);

	if (later) {
		char *found = strchr(p->date, 'g');
		found[0] = 'f';		// after に変更
		found[1] = 't';
	}
}


// --------------------------------------------------------------------------------
#define SEP ','

// addComma
void
makeSize(long int num, char *digits)
{
	if (num < 1000) {
		sprintf(digits, "%ld", num);
	} else {
		char tmp[DATALEN];

		makeSize(num / 1000, digits);
// 		sprintf(tmp, ",%03ld", num % 1000);
		sprintf(tmp, "%c%03ld", SEP, num % 1000);
		strcat(digits, tmp);
	}
}


// size, count で使用
void
makeReadableSize(char *numstr)
{
	// 1,234,567,890,123,456,789
	int len = strlen(numstr);

	if (len < 4) {			// 1K 以下
		return;
	}

	int unit = 'K';
	do {
		if (len > 24) { unit = 'E'; break; }
		if (len > 20) { unit = 'P'; break; }
		if (len > 16) { unit = 'T'; break; }
		if (len > 12) { unit = 'G'; break; }
		if (len >  8) { unit = 'M'; break; }
	} while (0);

	numstr = strchr(numstr, SEP);
	numstr[0] = '.';		// ',' を '.' に置換
	numstr[2] = unit;		// 小数点以下 1 桁は表示
	numstr[3] = '\0';
}


// --------------------------------------------------------------------------------
// 指定ディレクトリに含まれているエントリー数を返す、stat() をしないから、chdir() は不要
int
countEntry(char *dname, char *path)
{
	struct dirent **namelist;
	char fullpath[FNAME_LENGTH];
	char *tmppath;

	debug printf("countEntry:\n");

	if (dname[0] ==  '/') {
		tmppath = dname;
	} else {
		sprintf(fullpath, "%s%s/", path, dname);
		tmppath = fullpath;
	}

	int file_count = scandir(tmppath, &namelist, NULL, NULL);	// sort 不要

	if (file_count == -1) {
		debug printf(" scandir: %s :-1.\n", tmppath);
		return -1;
	}
	debug printf(" %d, %s\n", file_count, tmppath);

	for (int i=0; i<file_count; i++) {
		free(namelist[i]);
	}

	// "." と ".." を除いた数を返す
	return file_count -2;
}


// ================================================================================
// text で囲む場合の文字列
struct ENCLOSING {
	char textbegin[DATALEN];
	char textend[DATALEN];
	int lbegin;
	int lend;
	int tlen;
};


// ファイルの種類の文字を表示
int
printKind(struct FNAME p)
{
	// 'c', 'b' は追加表示する文字なし
	switch (p.mode[0]) {
		case 'd': printStr(base, "/"); break;
		case '|': printStr(base, "|"); break;		// FIFO/pipe  /tmp/fish.ryoma/
		case 's': printStr(base, "="); break;		// socket     /tmp/
		case 'l': {
			if (IS_DIRECTORY(p)) {
				printStr(base, "/");
			} else {
				printStr(base, "@");
			}
		} break;
		// 次が無ければ表示しない
		default: return 0; break;
	}

	return 1;
}


void (*printName)(struct FNAME, const char *, struct ENCLOSING);
char paintString[FNAME_LENGTH];
int paintStringLen;

void
printUnique(struct FNAME p, const char *dummy, struct ENCLOSING enc)
{
	// unique が無いので、そのまま表示
	if (p.uniquebegin == -1) {
		printStr(base, p.name);
		return;
	}

	printEscapeColor(base);
	// --------------------------------------------------------------------------------
	// あれば uniquebegin までを表示
	if (p.uniquebegin > 0) {
		printf("%.*s", p.uniquebegin, p.name);
	}

	// --------------------------------------------------------------------------------
	// 色付け開始
	printf("%s", enc.textbegin);
	printEscapeColor(p.color);

	// --------------------------------------------------------------------------------
	// unique 部分は 1 文字ずつ表示
	for (int i=p.uniquebegin; i<=p.uniqueend; i++) {
		// 以下、表示を変更しているので、paint カラー
		// 先頭が - の場合、\ でエスケープできないから変更、、、/bin/x86_64_linux_gnu[_go]ld
		if (p.name[i] == '-' && i == p.uniquebegin) {
			printEscapeColor(paint);
			printf("_");
			printEscapeColor(p.color);
			continue;
		}

		// 途中の _ の場合、Shift を押さなくてよくなる対応
		if (p.name[i] == '_' && i != p.uniquebegin) {
			printEscapeColor(paint);
			printf("-");
			printEscapeColor(p.color);
			continue;
		}

		// エスケープ対象の文字、printLength: と合わせる
		if (strchr(" ~#()\\$", p.name[i])) {
			printf("\\");
		}

		// 普通の文字表示
		printf("%c", p.name[i]);
	}
	printEscapeColor(reset);

	// --------------------------------------------------------------------------------
	printEscapeColor(base);
	printf("%s", enc.textend);
	// あれば uniqueend 後を表示
	printf("%s", p.name + p.uniqueend +1);
	printEscapeColor(reset);

	// dummy
	if (dummy == NULL) {
	}
}


int
countMatchedString(const char *str)
{
	int ret = 0;

	if (paintStringLen == 0) {
		return ret;
	}

	int length = strlen(str);

	// 比較用に小文字化
	char name[length];
	for (int i=0; i<length; i++) {
		name[i] = myTolower(str[i]);
	}
	name[length] = '\0';

	if (strstr(name, paintString) == NULL) {
		return ret;
	}

	for (int i=0; i<length; i++) {
		if (strncmp(paintString, name+i, paintStringLen) == 0) {
			i += paintStringLen - 1;

			ret++;
		}
	}

	return ret;
}


void
printMatchedString(struct FNAME p, const char *str, struct ENCLOSING enc)
{
	// このファイル情報のどれにも該当していない pickupString() されていない
	if (p.uniqueend == -1) {
		printStr(base, str);
		return;
	}

	if (paintStringLen == 0) {		// 0 だとstrncmp() で無限ループ
		printStr(base, str);
		return;
	}

	// --------------------------------------------------------------------------------
	int length = strlen(str);

	// 比較用に小文字化
	char name[length];
	for (int i=0; i<length; i++) {
		name[i] = myTolower(str[i]);
	}
	name[length] = '\0';

	if (strstr(name, paintString) == NULL) {
		printStr(base, str);
		return;
	}

	printEscapeColor(base);
	for (int i=0; i<length; i++) {
		if (strncmp(paintString, name+i, paintStringLen) == 0) {
			printf("%s", enc.textbegin);

			printEscapeColor(paint);
			printf("%.*s", paintStringLen, str+i);			// マッチした文字列を、元のままで表示
// 			printf("%s", paintString);
			printEscapeColor(reset);
			printEscapeColor(base);

			printf("%s", enc.textend);
			i += paintStringLen - 1;
		} else {
			printf("%c", str[i]);
		}
	}
	printEscapeColor(reset);
}


// ================================================================================
#ifdef DEBUG
void
displayAllQsortdata(struct FNAME *data, int n, struct ENCLOSING enc)
{
	printf("No:len:[unibegin,uniend] show: source: target: name/  lower:[name]\n");

	for (int i=0; i<n; i++) {
		printf("%2d:%2d:", i+1, data[i].length);		// 数、長さ
		printf("[%2d,%2d] ", data[i].uniquebegin, data[i].uniqueend);
		printf("sh:%2d ", data[i].showlist);			// -1(error), 0(非表示), 1(short), 2(long)
		printf("sr:%2d ", data[i].sourcelist);
		printf("tr:%2d ", data[i].targetlist);
		printName(data[i], data[i].name, enc);			// name
		if (strcmp(data[i].name, data[i].lowername)) {
			printf("  lower:[%s]", data[i].lowername);
		}

		printf("\n");
	}
}
#endif


// ================================================================================
#ifdef DEBUG
int count_deletelist = 0;
#endif

int
deleteList(struct FNAME list[], int i, int *start)
{
	if (i == -1) {
		return *start;
	}

	int pr = list[i].prevn;
	int ne = list[i].nextn;

	if (i == *start) {
		*start = ne;
	}

	if (pr != -1) {
		list[pr].nextn = ne;
	}
	if (ne != -1) {
		list[ne].prevn = pr;
	}
	list[i].nextn = -1;
	list[i].prevn = -1;

#ifdef DEBUG
	count_deletelist++;
#endif

	return ne;
}


#ifdef DEBUG
void
printallList(struct FNAME list[], int nth)
{
	for (int i=0; i<nth; i++) {
		if (list[i].nextn == -1 && list[i].prevn == -1) {
			printf("%2d, (next:%2d, prev:%2d, name:%s)\n", i, list[i].nextn, list[i].prevn, list[i].name);
		} else {
			printf("%2d,  next:%2d, prev:%2d, name:%s\n", i, list[i].nextn, list[i].prevn, list[i].name);
		}
	}
}


void
printlinkedList(struct FNAME list[], int start)
{
	int i=start;

	printf("start:%d\n", start);
	while (i != -1) {
		printf("%2d, next:%2d, prev:%2d, name:%s\n", i, list[i].nextn, list[i].prevn, list[i].name);
		i = list[i].nextn;
	}
}
#endif


void
addFNamelist(struct FNAME *p, char *name)
{
	p->size[0] = '\0';
	p->count[0] = '\0';
	p->path[0] = '\0';
	p->name = name;		// namelist[i] をそのまま使用
	p->kind[0] = '\0'; p->kind[1] = '\0';
	p->linkname[0] = '\0';
	p->errnostr[0] = '\0';

	p->length = strlen(p->name);
// 	p->lowername = strdup(p->name);
	p->lowername = (char *) malloc(sizeof(char) * (p->length+1));
	if (p->lowername == NULL) {
		perror("malloc");
		printf(" =>size:%zu\n", sizeof(char) * (p->length+1));

		exit(EXIT_FAILURE);
	}
	// 比較用に小文字化
	for (int i=0; i<p->length; i++) {
#ifndef MYTOLOWER
		if (p->name[i] == '-') {			// '-' と '_' が同じ扱いなので、'_' に統一
			p->lowername[i] = '_';
			continue;
		}
#endif
		p->lowername[i] = myTolower(p->name[i]);
	}
	p->lowername[p->length] = '\0';
// 	debug printf("addFNamelist: %s\n", p->lowername);

	p->uniquebegin = -1;
	p->uniqueend   = -1;
	p->color = normal;			// -s の時の色でもある

	// uniqueCheck(), uniqueCheckFirstWord() 候補
	p->sourcelist = 1;
	p->targetlist = 1;
	// 全表示候補
	p->showlist = SHOW_SHORT;	// デフォルトは printShort()

	// linkedlist
	p->prevn = -1;
	p->nextn = -1;
}


// ================================================================================
// ファイル名の長い順にする
int
sortNameLength(const struct dirent **s1, const struct dirent **s2)
{
	// duplist search count: ./a.out -al /usr/bin/
	// if あり: 464696, 無し: 491029

	int len1 = strlen((*s1)->d_name);
	int len2 = strlen((*s2)->d_name);

	if (len1 == len2) {
		return strcmp((*s1)->d_name, (*s2)->d_name);
	}

	return (len1 < len2) ? 1 : -1;
}


// アルファベット順
// myAlphaSort() と同様に、大文字/小文字を分けないために、strcasecmp()
int
sortNameAlphabet(const struct dirent **s1, const struct dirent **s2)
{
	return strcasecmp((*s1)->d_name, (*s2)->d_name);
}


// --------------------------------------------------------------------------------
// qsort() 時のソート
// 大文字/小文字を分けない
int
myAlphaSort(const void *a, const void *b)
{
	struct FNAME *s1 = (struct FNAME *)a;
	struct FNAME *s2 = (struct FNAME *)b;

// 	return strcmp(s1->lowername, s2->lowername);
	return strcasecmp(s1->name, s2->name);
// 	return strcmp(s1->name, s2->name);				// ls と同じソート
}


int
myAlphaSortRev(const void *a, const void *b)
{
	struct FNAME *s1 = (struct FNAME *)a;
	struct FNAME *s2 = (struct FNAME *)b;

// 	return strcmp(s2->lowername, s1->lowername);
	return strcasecmp(s2->name, s1->name);
// 	return strcmp(s2->name, s1->name);				// ls と同じソート
}


int
sizeSort(struct FNAME *s1, struct FNAME *s2)
{
	int len1 = strlen(s1->size);
	int len2 = strlen(s2->size);

	if (len1 == len2) {
		// 同じファイルサイズの時、ファイル名をアルファベット順でソート
		if (strcmp(s1->size, s2->size) == 0) {
// 			return strcmp(s1->lowername, s2->lowername);
			return strcasecmp(s1->name, s2->name);
		}
		return strcmp(s1->size, s2->size);
	}

	return (len1 < len2) ? -1 : 1;
}

// size の小さい順
int
mySizeSort(const void *a, const void *b)
{
	struct FNAME *s1 = (struct FNAME *)a;
	struct FNAME *s2 = (struct FNAME *)b;

	return sizeSort(s1, s2);
}

// size の大きい順
int
mySizeSortRev(const void *a, const void *b)
{
	struct FNAME *s1 = (struct FNAME *)a;
	struct FNAME *s2 = (struct FNAME *)b;

	return sizeSort(s2, s1);	// 逆順
}


// mtime の新しい順
int
myMtimeSort(const void *a, const void *b)
{
	struct FNAME *s1 = (struct FNAME *)a;
	struct FNAME *s2 = (struct FNAME *)b;

	return ((double) s1->sb.st_mtime - s2->sb.st_mtime < 1) ? 1 : -1;
}

// mtime の古い順
int
myMtimeSortRev(const void *a, const void *b)
{
	struct FNAME *s1 = (struct FNAME *)a;
	struct FNAME *s2 = (struct FNAME *)b;

	return ((double) s2->sb.st_mtime - s1->sb.st_mtime < 1) ? 1 : -1;	// 逆順
}


// ================================================================================
#define SKIP_LIST "()"			// unique な文字列に含まれていると厄介な文字列、SKIP 対象

void
runUniqueCheck(struct FNAME *fnamelist, int nth, int *startn, struct DLIST *duplist, void (*chkfunc)(struct FNAME *top, int chklen, int n, int *startn, struct DLIST *duplist), int deep_unique)
{
	int sum = 0;
	for (int j=1; j<UNIQUE_LENGTH; j++) {
		chkfunc(fnamelist, j, nth, startn, duplist);
		sum += count_chklen[j];

		// 最後までチェックする
		if (deep_unique) {
			continue;
		}

		// --------------------------------------------------------------------------------
		// 途中でやめる
		// unique の数が前回の半分以下、0 になった、これ以上続けても非効率、、、ただし、最低 3 文字までは行う
		if ((j>2) && (count_chklen[j-1]/2 > count_chklen[j] || count_chklen[j] == 0)) {
			debug printf(" done, %d\n", j);
			break;
		}

		// 9 割りを超えていたらやめる
		if (sum > nth * 0.9) {
			debug printf(" done %d/%d.\n", sum, nth);
			break;
		}
	}
}


void
uniqueCheck(struct FNAME *top, int chklen, int n, int *startn, struct DLIST *duplist)
{
	int now = *startn;

	// now 比較元
	while (now != -1) {
		if (top[now].sourcelist == -1) {
			now = top[now].nextn;
			continue;
		}

		// ファイル名を全部入力する必要があるものは、unique 表示にしない
		if (chklen >= top[now].length) {
			now = deleteList(top, now, startn);
			continue;
		}

		// --------------------------------------------------------------------------------
		// チェック先のファイル名の長さまで、1 文字ずつずらして比較する
		int nth = top[now].length - chklen +1;
		int brk2 = 0;

		for (int i=0; i<nth; i++) {
			char *tmpname2;
			tmpname2 = top[now].lowername + i;

			// tmpname は重複している
			if (searchDuplist(duplist, tmpname2, chklen)) {
				continue;
			}

			int brk = 0;
			for (int j=0; j<chklen; j++) {
				// 漢字が含まれている
				if (isprint((int) tmpname2[j]) == 0) {
					brk = 1;
					break;
				}
				// 先頭が '()' だとエスケープできないから飛ばす、myTolower() 後の文字列で
				if (strchr(SKIP_LIST, tmpname2[j])) {
					brk = 1;
					break;
				}
			}
			// 漢字や SKIP_LIST を飛ばす
			if (brk) {
				addDuplist(duplist, tmpname2, chklen);
				continue;
			}

			// --------------------------------------------------------------------------------
			// strstr は n が無いから、コピーしないとダメ
			char tmpname[chklen];
			strncpy(tmpname, top[now].lowername +i, chklen);
			tmpname[chklen] = '\0';

			int uq = 1;
			// p 比較先
			for (int p=0; p!=n; p++) {
				// emacs list 対象外はチェックしない
				if (top[p].targetlist == -1) {
					continue;
				}

				// 自分だから、飛ばす
				if (now == p) {
					continue;
				}

				// 同じ first_word の場合でも、d と d 以外なら重複しても問題なし
// 				if (top[p].mode[0] == top[now].mode[0]) {
					if (strstr(top[p].lowername, tmpname)) {
						uq = 0;
						// 重複していたので登録
						addDuplist(duplist, tmpname, chklen);
						break;
					}
// 				}
			}

			if (uq) {
				top[now].uniquebegin = i;
				top[now].uniqueend = i+chklen-1;
				now = deleteList(top, now, startn);
				brk2 = 1;
				// それぞれの cheklen と合致個数を記録する
				count_chklen[chklen]++;
				break;
			}
		}

		if (brk2) {
			continue;
		}

		now = top[now].nextn;
	}
}


void
uniqueCheckFirstWord(struct FNAME *top, int chklen, int n, int *startn, struct DLIST *duplist)
{
	int now = *startn;

	// now 比較元
	while (now != -1) {
		if (top[now].sourcelist == -1) {
			now = top[now].nextn;
			continue;
		}

		// ファイル名を全部入力する必要があるものは、unique 表示にしない
		if (chklen >= top[now].length) {
			now = deleteList(top, now, startn);
			continue;
		}

		// --------------------------------------------------------------------------------
		// tmpname は重複している
		if (searchDuplist(duplist, top[now].lowername, chklen)) {
			now = top[now].nextn;
			continue;
		}

		int brk = 0;
		for (int i=0; i<chklen; i++) {
			// 漢字が含まれている
			if (isprint((int) top[now].lowername[i]) == 0) {
				brk = 1;
				break;
			}
			if (strchr(SKIP_LIST, top[now].lowername[i])) {
				brk = 1;
				break;
			}
		}
		// 漢字や SKIP_LIST を飛ばす
		if (brk) {
			addDuplist(duplist, top[now].lowername, chklen);
			now = top[now].nextn;
			continue;
		}

		// --------------------------------------------------------------------------------
		int uq = 1;

		// p 比較先
		for (int p=0; p!=n; p++) {
			// emacs list 対象外はチェックしない
			if (top[p].targetlist == -1) {
				continue;
			}

			// 自分だから、飛ばす
			if (now == p) {
				continue;
			}

// 			// 同じ first_word の場合でも、d と d 以外なら重複しても問題なし
// 			if (top[p].mode[0] == top[now].mode[0]) {
				if (strncmp(top[p].lowername, top[now].lowername, chklen) == 0) {
					uq = 0;
					// 重複していたので登録
					addDuplist(duplist, top[now].lowername, chklen);
					now = top[now].nextn;
					break;
				}
// 			}
		}

		if (uq) {
			top[now].uniquebegin = 0;
			top[now].uniqueend = chklen-1;
			now = deleteList(top, now, startn);

			// それぞれの cheklen と合致個数を記録する
			count_chklen[chklen]++;
		}
	}
}


// use_emacs で使用
// p2 に対して、どれぐらいのマッチング率かを返す
float
matchPercent(struct FNAME p1, struct FNAME p2)
{
	int i = 0;

	// 二つの文字列を比較
	while (p1.name[i] == p2.name[i]) {
		if (p1.name[i] == '\0') {
			return 100.0;				// 完全一致
		}
		i++;
	}

	// どちらかが部分文字列である場合、もしくは一致しない
	return (float) i / p2.length;
}


// ================================================================================
// 表示用漢字対策
#include <wchar.h>
int wcwidth(wchar_t c);

int
myStrlen(struct FNAME p)
{
	// ワイド文字列に変換
	wchar_t wstr[p.length + 1];
	mbstowcs(wstr, p.name, p.length + 1);

	// 各文字の表示幅を計算
	long unsigned int len = wcslen(wstr);
	int total = 0;
	for (size_t i=0; i<len; i++) {
		int width = wcwidth(wstr[i]);
		if (width > 0) {
			total += width;
		}
	}

	return total;
}


// --------------------------------------------------------------------------------
// 表示する -f 情報の中から、該当文字列を探す
int
pickupString(struct FNAME p, char *string, char orderlist[], char *(*func)(const char *, const char *))
{
	for (int i=0; orderlist[i] != '\0'; i++) {
		switch (orderlist[i]) {
		  case 'i': case 'I': if (func(p.inode, string) != NULL) { return 1; } break;
		  case 'h': case 'H': if (func(p.nlink, string) != NULL) { return 1; } break;
		  case 'm': case 'M': if (func(p.mode,  string) != NULL) { return 1; } break;
		  case 'o': case 'O': if (func(p.owner, string) != NULL) { return 1; } break;
		  case 'g': case 'G': if (func(p.group, string) != NULL) { return 1; } break;
		  case 's': case 'S': if (func(p.size,  string) != NULL) { return 1; } break;
		  case 'c': case 'C': if (func(p.count, string) != NULL) { return 1; } break;
		  case 'd': case 'D': if (func(p.date,  string) != NULL) { return 1; } break;
		  case 't': case 'T': if (func(p.time,  string) != NULL) { return 1; } break;
		  case 'w': case 'W': if (func(p.week,  string) != NULL) { return 1; } break;
		  case 'p': case 'P': if (func(p.path,  string) != NULL) { return 1; } break;
		  case 'n': case 'N': if (func(p.name,  string) != NULL) { return 1; } break;
		  case 'k': case 'K': if (func(p.kind,  string) != NULL) { return 1; } break;
		  case 'l': case 'L': if (myStrcasestr(p.linkname, string) != NULL) { return 1; } break;
		  case 'e': case 'E': if (myStrcasestr(p.errnostr, string) != NULL) { return 1; } break;
		}
	}

	return 0;
}


// ================================================================================
// リダイレクトでもレイアウトを維持するようにスペースを使用
// リダイレクト時の ls と同じ出力結果を得るのは ./a.out -fNk か
void
printShort(struct FNAME *data, int n, struct ENCLOSING enc, unsigned short int termlen, unsigned short int termhei)
{
	debug printf("printShort:\n");
	int printshort_count = 0;
	int attempts = 0;

	int sep = 2;		// ファイル名同士の最低空白数 2
	int nth = 0;

	// 表示数をカウント
	for (int i=0; i<n; i++) {
		if (data[i].showlist != SHOW_SHORT) {
			continue;
		}

		nth++;
	}
	debug printf(" nth:%d\n", nth);

	// 表示対象が無かった
	if (nth == 0) {
		return;
	}

#ifdef DEBUG
	printf("No:len:[unibegin,uniend] name/  lower:[name]\n");

	for (int i=0; i<n; i++) {
		if (data[i].showlist != SHOW_SHORT) {
			printf("%2d:%2d( -):", i+1, data[i].length);						// 数
		} else {
			printf("%2d:%2d(%2d):", i+1, data[i].length, data[i].print_length);	// 数、長さ
		}
		printf("[%2d,%2d] ", data[i].uniquebegin, data[i].uniqueend);
		printName(data[i], data[i].name, enc);									// name
		printKind(data[i]);
		if (strcmp(data[i].name, data[i].lowername)) {
			printf("  lower:[%s]", data[i].lowername);
		}

		printf("\n");
	}
#endif

	// repeat: 最大行数、増やしていくと、column が入るようになる
	// 出来るようになった最初ではなく、出来る最後を選択するので、row が変わった瞬間ではない (もっと row は少ない)
	for (int repeat=1; repeat!=n; repeat++) {
		// 初期化
		int col = repeat;
		int row = nth/col +1;

		// row の数が、termlen/3 以下になるように
		if (row > termlen/3) {
			continue;
		}

		// 2 次元配列を確保
		int rowcolumnlist[row][col];
		memset(rowcolumnlist, -1, sizeof(rowcolumnlist));

		// 1 行のレイアウト+番兵
		int columnlist[row +1];
		memset(columnlist, -1, sizeof(columnlist));

// 		// --------------------------------------------------------------------------------
		// termlen に達するか、全部配置していく
		int count = 0;
		int maxlength = 0;

		for (int i=0; i<row && count<n; i++) {
			for (int j=0; j<col && count<n; j++) {
				// 非表示のファイル
				if (data[count].showlist != SHOW_SHORT) {
					j--;
					count++;
					continue;
				}

				// 列で、一番長い column に更新
				if (columnlist[i] < data[count].print_length) {
					columnlist[i] = data[count].print_length;

					// --------------------------------------------------------------------------------
					// 更新した結果、termlen に収まっているかチェック
					int colcount = 0;
					maxlength = 0;
					for (int k=0; columnlist[k] != -1; k++) {
						// 1 行の合計
						maxlength += columnlist[k];
						colcount++;
					}
					// 最後の 1 個の右側にセパレーターは不要
					maxlength += sep * (colcount - 1);

					// はみ出ている
					if (maxlength >= termlen) {
						break;
					}
				}

				rowcolumnlist[i][j] = count;
				count++;

				debug attempts++;
			}
			// はみ出ている
			if (maxlength >= termlen) {
				break;
			}
		}

		// --------------------------------------------------------------------------------
		// termlen に収まったか
		if (maxlength >= termlen) {
			continue;
		}

#ifdef DEBUG
		// columnlist 表示
		printf("columnlist: ");
		for (int i=0; columnlist[i] != -1; i++) {
			printf("%d, ", columnlist[i]);
		}
		printf("\n");

		// rowcolumnlist 表示
		printf("layout: [%d, %d]\n", row, col);
		for (int i=0; i<col; i++) {
			for (int j=0; j<row; j++) {
				printf("%4d ", rowcolumnlist[j][i]);
			}
			printf("\n");
		}
		printf("\n");

		// 最大長さを視覚的に表示
		printf("termlen:%d, maxlength:%d\n", termlen, maxlength);
		for (int i=0; i<maxlength; i++) {
			printf("+");
		}
		printf("\n");
#endif

		// --------------------------------------------------------------------------------
		// 表示、ここから先は Y,X
		for (int i=0; i<col; i++) {
			for (int j=0; j<row && rowcolumnlist[j][i] != -1; j++) {
				int index = rowcolumnlist[j][i];

				// リダイレクトしてもレイアウトを保つように
				printName(data[index], data[index].name, enc);
				int ret = printKind(data[index]);
				// 右が -1 なら出力しない
				if (j<row-1 && rowcolumnlist[j+1][i] != -1) {
					printf("%*s", columnlist[j] - data[index].print_length + sep - ret, "");
				}

				debug printshort_count++;
			}

			printf("\n");
		}
		// ガイダンス非表示
		if (columnlist[1] == -1) {
			repeat = 0;
		}

		// + ガイダンス行表示を行う (DEBUG 時は必ず表示)
#ifdef DEBUG
		repeat = termhei+1;
#endif
		// スクロールするなら + ガイダンス行表示
		if (repeat > termhei) {
			for (int j=0; columnlist[j] != -1; j++) {
				printStr(base, "+");
				if (columnlist[j+1] != -1) {
					printf("%*s", columnlist[j] - 1 + sep, "");
				}
			}
			printf("\n");
		}

		debug printf("printShort: %d/%d\n", printshort_count, nth);
		debug printf(" attempts count: %d\n", attempts);

		return;
	}


	// --------------------------------------------------------------------------------
	// termlen より長いファイル名がある
	for (int i=0; i<n; i++) {
		if (data[i].showlist != SHOW_SHORT) {
			continue;
		}
		printName(data[i], data[i].name, enc);
		printKind(data[i]);
		printf("\n");

		debug printshort_count++;
	}

	debug printf("printShort: %d/%d\n", printshort_count, nth);
	debug printf(" attempts count: %d\n", attempts);
}


// -f 文字列で、今より後ろに表示する文字列があるか
int
haveAfterdata(struct FNAME *p, const char orderlist[], int i)
{
	if (orderlist[i+1] == '\0') {
		return 0;
	}

	for (int j=i+1; orderlist[j] != '\0'; j++) {
		if (p->info[j] == NULL) {
			continue;
		}
		if (p->info[j][0] == '\0') {
			continue;
		}

		// 表示する文字列があった
		return 1;
	}

	return 0;
}


void
printLong(struct FNAME *data, int n, struct ENCLOSING enc, int digits[], char orderlist[])
{
	int printlong_count = 0;
	debug printf("printLong:\n");

	for (int i=0; i<n; i++) {
		// 非表示設定のファイルは表示しない
		if (data[i].showlist != SHOW_LONG) {
			continue;
		}

#ifdef DEBUG
		printlong_count++;
		printf("%3d.", printlong_count);

		// unique 文字数の表示
		if (data[i].uniquebegin != -1) {
			printf("%2d: ", data[i].uniqueend - data[i].uniquebegin + 1);
		} else {
			printf("    ");
		}

		printf("%3ld: ", data[i].sb.st_nlink);
#endif

		// --------------------------------------------------------------------------------
		for (int j=0; orderlist[j] != '\0'; j++) {
			int count = 0;
			int len = 0;

			count = countMatchedString(data[i].info[j]);

			// printMatchedString() 対応、左寄せ項目
			if (strchr("mMoOgGtTwW", (unsigned char) orderlist[j])) {
				// --------------------------------------------------------------------------------
				if ((unsigned char) orderlist[j] == 'm' || (unsigned char) orderlist[j] == 'M') {
					len = (int) strlen(data[i].mode) + count * enc.tlen;
				}

				if ((unsigned char) orderlist[j] == 'o' || (unsigned char) orderlist[j] == 'O') {
					len = (int) strlen(data[i].owner) + count * enc.tlen;
				}

				if ((unsigned char) orderlist[j] == 'g' || (unsigned char) orderlist[j] == 'G') {
					len = (int) strlen(data[i].group) + count * enc.tlen;
				}

				if ((unsigned char) orderlist[j] == 't' || (unsigned char) orderlist[j] == 'T') {
					len = (int) strlen(data[i].time) + count * enc.tlen;
				}

				if ((unsigned char) orderlist[j] == 'w' || (unsigned char) orderlist[j] == 'W') {
					len = (int) strlen(data[i].week) + count * enc.tlen;
				}

				debug printf("%c:", orderlist[j]);
				printMatchedString(data[i], data[i].info[j], enc);
				if (haveAfterdata(&data[i], orderlist, j) == 0) {
					continue;
				}

				if (digits[(unsigned char) orderlist[j]]) {
					printf("%*s", digits[(unsigned char) orderlist[j]] - len, "");
				}
				printf(" ");

				continue;
			}

			// --------------------------------------------------------------------------------
			// printMatchedString() 対応、右寄せ・特殊項目
			switch (orderlist[j]) {
			  case 'i':
			  case 'I':
				len = (int) strlen(data[i].inode) + count * enc.tlen;
				if (digits[(unsigned char) orderlist[j]]) {
					printf("%*s", digits[(unsigned char) orderlist[j]] - len, "");
				}
				debug printf("%c:", orderlist[j]);
				printMatchedString(data[i], data[i].info[j], enc);
				if (haveAfterdata(&data[i], orderlist, j) == 0) {
					break;
				}
				printf(" ");
				break;

			  case 'h':
			  case 'H':
				len = (int) strlen(data[i].nlink) + count * enc.tlen;
				if (digits[(unsigned char) orderlist[j]]) {
					printf("%*s", digits[(unsigned char) orderlist[j]] - len, "");
				}
				debug printf("%c:", orderlist[j]);
				printMatchedString(data[i], data[i].info[j], enc);
				if (haveAfterdata(&data[i], orderlist, j) == 0) {
					break;
				}
				printf(" ");
				break;

			  case 'd':
			  case 'D':
				len = (int) strlen(data[i].date) + count * enc.tlen;
				// -t の時の見栄え、右寄せ表示
				if (digits[(unsigned char) orderlist[j]]) {
					printf("%*s", digits[(unsigned char) orderlist[j]] - len, "");
				}
				debug printf("%c:", orderlist[j]);
				printMatchedString(data[i], data[i].info[j], enc);
				if (haveAfterdata(&data[i], orderlist, j) == 0) {
					break;
				}
				printf(" ");
				break;

			  case 's':
			  case 'S':
				len = (int) strlen(data[i].size) + count * enc.tlen;
				// 数値なので、右寄せ表示
				if (digits[(unsigned char) orderlist[j]]) {
					printf("%*s", digits[(unsigned char) orderlist[j]] - len, "");
				}
				debug printf("%c:", orderlist[j]);
				printMatchedString(data[i], data[i].info[j], enc);				// size
				if (haveAfterdata(&data[i], orderlist, j) == 0) {
					break;
				}
				printf(" ");
				break;

			  case 'c':
			  case 'C':
				len = (int) strlen(data[i].count) + count * enc.tlen;
				// 数値なので、右寄せ表示
				if (digits[(unsigned char) orderlist[j]]) {
					printf("%*s", digits[(unsigned char) orderlist[j]] - len, "");
				}
				// エントリー数は dir 色で表示 ? -n の時は分かりづらいけど、如何する ?
				if (data[i].color == dir && paintStringLen == 0) {
					// -n の時は分かりづらいけど、如何する ?
					debug printf("c:");
					printStr(dir, data[i].info[j]);
				} else {
					debug printf("s:");
					printMatchedString(data[i], data[i].info[j], enc);				// size
				}
				if (haveAfterdata(&data[i], orderlist, j) == 0) {
					break;
				}
				printf(" ");
				break;

				// --------------------------------------------------------------------------------
			  case '[':
			  case ']':
				printf("%s", data[i].info[j]);
				if (haveAfterdata(&data[i], orderlist, j) == 0) {
					break;
				}
				printf(" ");
				break;

				// --------------------------------------------------------------------------------
			  case 'p':
			  case 'P':
				if (data[i].info[j][0] == '\0') {
					break;
				}
				len = (int) strlen(data[i].path) + count * enc.tlen;
				debug printf("%c:", orderlist[j]);
				printMatchedString(data[i], data[i].info[j], enc);
				if (haveAfterdata(&data[i], orderlist, j) == 0) {
					break;
				}
				if (digits[(unsigned char) orderlist[j]]) {
					printf("%*s", digits[(unsigned char) orderlist[j]] - len, "");
				}
				// 特殊対応
				if (!(orderlist[j] == 'P' && (orderlist[j+1] == 'n' || orderlist[j+1] == 'N'))) {
					printf(" ");
				}
				break;

			  case 'n':
			  case 'N':
				len = data[i].print_length + count * enc.tlen;
				debug printf("%c:", orderlist[j]);
				printName(data[i], data[i].info[j], enc);					// name (printUnique(), printMatchedString() の切替)
				if (haveAfterdata(&data[i], orderlist, j) == 0) {
					break;
				}
				if (digits[(unsigned char) orderlist[j]]) {
					printf("%*s", digits[(unsigned char) orderlist[j]] - len, "");
				}
				// 特殊対応
				if (orderlist[j+1] != 'k' && orderlist[j+1] != 'K') {
					printf(" ");
				}
				break;

			  case 'k':
			  case 'K':
// 				int ret = printKind(data[i]);
				int ret = 0;
				if (data[i].kind[0] != '\0') {
					printMatchedString(data[i], data[i].info[j], enc);
					ret = 1;
				}
				if (haveAfterdata(&data[i], orderlist, j) == 0) {
					break;
				}
				if (ret == 0) {
					printf(" ");
				}
				break;

			  case 'l':
			  case 'L':
				len = (int) strlen(data[i].linkname) + count * enc.tlen;
				// symlink 先を表示
				if (data[i].mode[0] == 'l') {
					printf(" ");
					printStr(data[i].color, "->");							// link 先と同じ色
					printf(" ");
					debug printf("%c:", orderlist[j]);
					printMatchedString(data[i], data[i].info[j], enc);
					if (digits[(unsigned char) orderlist[j]]) {
						printf("%*s", digits[(unsigned char) orderlist[j]] - len, "");
					}
				} else {
					// -> の分を表示する
					if (orderlist[j] == 'l') {
						printf("    ");			// " -> " の分
						if (digits[(unsigned char) orderlist[j]]) {
							printf("%*s", digits[(unsigned char) orderlist[j]] - len, "");
						}
					}
				}
				if (haveAfterdata(&data[i], orderlist, j) == 0) {
					break;
				}
				printf(" ");
				break;

			  case 'e':
			  case 'E':
				len = (int) strlen(data[i].errnostr) + count * enc.tlen;
				// エラー表示
				if (data[i].errnostr[0] != '\0') {
					debug printf("%c:", orderlist[j]);
					printStr(data[i].color, "[");
					printMatchedString(data[i], data[i].info[j], enc);
					printStr(data[i].color, "]");
					if (digits[(unsigned char) orderlist[j]]) {
						printf("%*s", digits[(unsigned char) orderlist[j]] - len, "");
					}
				} else {
					// [] の分を表示する
					if (orderlist[j] == 'e') {
						printf("  ");			// "[]" の分
						if (digits[(unsigned char) orderlist[j]]) {
							printf("%*s", digits[(unsigned char) orderlist[j]] - len, "");
						}
					}
				}
				if (haveAfterdata(&data[i], orderlist, j) == 0) {
					break;
				}
				printf(" ");
				break;

			  default:
				// 指定文字列に無いものを指摘
				// -fz などの指定では NULL (無視)、改行だけ表示されることになる
				printEscapeColor(reset);
				printEscapeColor(error);
				printf("%c ", orderlist[j]);
				printEscapeColor(reset);
				break;
			}
		}

		// --------------------------------------------------------------------------------
		printEscapeColor(reset);
		printf("\n");
	}
	debug printf("printLong:  %d/%d\n", printlong_count, n);
}


// ================================================================================
// どの程度の文字列の長さで unique になっているか表示
void
printAggregate(int hitcount, int displaycount, int nth, int *count_chklen)
{
	int count = 0;
	double percent = 0.0;

	debug printf("printAggregate:\n");

	for (int i=1; i<UNIQUE_LENGTH; i++) {
		count += count_chklen[i];
		percent += myRound( count_chklen[i] * 100.0 / nth );
	}

	// 表示
	debug printf(" ");
	printf("aggregate results:");

	// -P の時、表示数が変化するから
	if (hitcount != count || displaycount != nth) {
		printf(" display:%d/%d(%.1f%%)", hitcount, displaycount, myRound( hitcount * 100.0 / displaycount));
	}
	// 全体のデータ表示
	printf(" all:%d/%d(%.1f%%) ", count, nth, percent);
	for (int i=1; i<UNIQUE_LENGTH; i++) {
		if (count_chklen[i] != 0) {
			printf("[%d:%d(%.1f%%)] ", i, count_chklen[i], myRound(count_chklen[i] * 100.0 / nth));
		}
	}
	printf("\n");
}


// ================================================================================
// 256 color、|less すると、表示できる色が変わる
void
showEscapeList(void)
{
	printf("Default Colors:\n");
	printf(" ");
	colorUsage();
	printf(" set -x %s '%s'\n", ENVNAME, default_color_txt);
	printf("\n");

	printf("8 color:\n");
	for (int i=30; i<38; i++) {
		printf("\033[%dm", i);
		printf("%5d", i);
		printf("\033[0m");

		printf("\033[%dm", i+10);
		printf("%5d", i+10);
		printf("\033[0m");
	}
	printf("\n");

	for (int i=90; i<98; i++) {
		printf("\033[%dm", i);
		printf("%5d", i);
		printf("\033[0m");

		printf("\033[%dm", i+10);
		printf("%5d", i+10);
		printf("\033[0m");
	}
	printf("\n\n");

	// --------------------------------------------------------------------------------
	int j;

	printf("256 color: fg:3xxx, bg:4xxx\n");
	j=1;
	printf(" standard colors/high-intensity colors\n");
	for (int i=0; i<16; i++) {
		printf("\033[38;5;%dm", i);
		printf("%5d", 3000 + i);
		printf("\033[0m");

		printf("\033[48;5;%dm", i);
		printf("%5d", 4000 + i);
		printf("\033[0m");

		if (j++ % 8 == 0) {
			printf("\n");
		}
	}
	printf("\n");

	// --------------------------------------------------------------------------------
	// ここの fg/bg は上と混ぜられない
	printf("216 colors:\n");
	j=1;
	for (int i=16; i<232; i++) {
		printf("\033[38;5;%dm", i);
		printf("%5d", 3000 + i);
		printf("\033[0m");

		printf("\033[48;5;%dm", i);
		printf("%5d", 4000 + i);
		printf("\033[0m");

		if (j++ % 6 == 0) {
			printf("\n");
		}
	}
	printf("\n");

	// --------------------------------------------------------------------------------
	// fg/bg では、bg が優先される
	printf("grayscale colors:\n");
	j=1;
	for (int i=232; i<256; i++) {
		printf("\033[38;5;%dm", i);
		printf("%5d", 3000 + i);
		printf("\033[0m");

		printf("\033[48;5;%dm", i);
		printf("%5d", 4000 + i);
		printf("\033[0m");

		if (j++ % 6 == 0) {
			printf("\n");
		}
	}
}


#ifndef VERSION
#include "version.h"
#endif

void
showVersion(char **argv)
{
	printf("%s:\n", argv[0]);
	printf(" Displays a list of directories and files.\n");
	printf(" Displays %sUnique Parts%s of directory and file names in %sColor%s for shell filename completion.\n", colorlist[normal], colorlist[reset], colorlist[normal], colorlist[reset]);
	printf("  default unique word:  fish shell. (default)\n");
	printf("  beginning of word:    like tcsh shell. (-b)\n");
	printf("  Elisp like file name: same file name and differet file extension. (-e)\n");
	printf("  enclosing string:     without Color. (-TB\\[ -TE\\] -n)\n");
	printf(" Displays only files for which the specified string matches. (-P, all strings displayed)\n");

#ifdef VERSION
	printf("\n");
	printf(" Version: %s\n", VERSION);
#endif
	printf(" Build: ");
#ifdef INCDATE
	printf("%s, %s %s [%s]\n", BYEAR, BDATE, BTIME, __FILE__);
#else
	printf("%s, %s\n", __DATE__, __TIME__);
#endif
}


// --------------------------------------------------------------------------------
void
argvUsage(char **argv)
{
	printf("Usage:\n");
	printf(" %s [OPTION]... [DIR/|FILE]...\n", argv[0]);
	printf("\n");
	printf(" List of directories and files. if nothing is listed, the current directory is used.\n");
	printf("  Long listing format:  /bin is FILE, /bin/ is DIR.\n");
	printf("  Short listing format: /bin is DIR.\n");

	printf("\n");
	printf(" Options may be specified individually, as in -l -a -u, or collectively, as in -alu. (In no particular order)\n");
	printf(" Color %s-xxx%s option is specified separately from the other options.\n", colorlist[normal], colorlist[reset]);

	printf("\n");
	printf(" If multiple options are specified:\n");
	printf("  The last option is overwritten:   -c, -p, -P, -f, -TB, -TE.\n");
	printf("  All options are merged:           -m, -z, -N.\n");

	printf("\n");
	printf("Listing format options:\n");
	printf(" -s: Short listing format. (no kind, one color)\n");
	printf(" -l: Long listing format.\n");
	printf(" %s-f%s: with -l, change Format orders (default: -fmogcdPNkLE)\n", colorlist[normal], colorlist[reset]);
	printf("     (Char:  m:mode, o:owner, g:group, c:count, d:date, p:path, n:name, k:kind, l:linkname, e:errno,\n");
	printf("             i:inode, h:hardlinks, s:size, t:time, w:week.\n");
	printf("             Upper case is padding off. (Same length: paint with enclosing string)\n");
	printf("      Other: p:with -l, argv is \"PATH/FILE\" format,\n");
	printf("             [, ]:enclosing string with \'[\', \']\',\n");
	printf("             s:size of DIRECTORY and FILE, c:count entry of DIRECTORY and size of FILE.)\n");
	printf("     -s > -l = -f > default short listing (include file status)\n");

	printf("\n");
	printf("Coloring algorithm options:\n");
	printf(" -u: deep Unique word check. (default check -> deep check)\n");
	printf(" -b: Beginning of a word. (default check -> beginning word check)\n");
	printf(" %s-p%s: Paint the matched string (without kind). (-pstring, case insensitive) \n", colorlist[normal], colorlist[reset]);
	printf(" -e: paint Elisp like file unique group name word check.\n");
	printf("     -p > -u > -b = -e > default unique word check algorithm\n");

	printf("\n");
	printf("Output data options:\n");
	printf(" -a: show All dot files.\n");
	printf(" -o: with -a, show Only directories. (-s > -o > [FILE])\n");
	printf(" %s-P%s: like -p, Pickup only the string matched. (-Pstring, case sensitive) \n", colorlist[normal], colorlist[reset]);
	printf("     -P = -o > -a\n");

	printf("\n");
	printf("Color options:\n");
	printf(" -n: No colors.\n");
	printf(" %s-c%s: set Colors. (8 colors: -cbase=37:normal=34:normal=1:..., 256 colors: -cbase=3007:normal=3012:normal=1:...)\n", colorlist[normal], colorlist[reset]);
	printf(" -d: use Default colors.\n");
	printf("     -n > -c > -d > %s env color > default color\n", ENVNAME);

	printf("\n");
	printf("Sort options:\n");
	printf(" -m: Mtime sort. (alphabet sort -> mtime sort, -mm: reverse mtime sort)\n");
	printf(" -z: siZe sort (without count).  (alphabet sort -> size sort, -zz: reverse size sort)\n");
	printf(" -N: reverse alphabet (Name) sort. (alphabet sort -> reverse alphabet sort, -NN: alphabet sort)\n");
	printf(" -O: no sort Order.\n");
	printf("     O > N(N) > -z(z) > -m(m)\n");

	printf("\n");
	printf("Additional options:\n");
	printf(" -t: with -l, human-readable daTe.\n");
	printf(" -i: with -l, human-readable sIze. (count, size, hardlinks, whithout inode)\n");
	printf(" -r: show aggregate Results.\n");
	printf("     -t = -i != -r\n");

	printf("\n");
	printf("Other options:\n");
	printf(" -h, %s-help%s:    show this message.\n", colorlist[normal], colorlist[reset]);
	printf(" -v, %s-version%s: show Version.\n", colorlist[normal], colorlist[reset]);
	printf(" %s-TB%s, %s-TE%s:     enclosing string. (-TB\\[ -TE\\])\n", colorlist[normal], colorlist[reset], colorlist[normal], colorlist[reset]);
	printf(" %s-always%s:      output the escape sequence characters.\n", colorlist[normal], colorlist[reset]);
	printf(" %s-256%s:         color text output.\n", colorlist[normal], colorlist[reset]);

	printf("\n");
	printf("Default Colors:\n");
	printf(" ");
	colorUsage();
	printf(" set -x %s '%s'\n", ENVNAME, default_color_txt);
}


// ================================================================================
// terminal のサイズを取得、printShort() で使用するだけ
int
getTerminalSize(unsigned short int *x, unsigned short int *y)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
		debug printf("getTerminalSizeE: width:%d, height:%d\n", ws.ws_col, ws.ws_row);
// 		perror("ioctl");
// 		exit(EXIT_FAILURE);
		return -1;
	}

	debug printf("getTerminalSize: width:%d, height:%d\n", ws.ws_col, ws.ws_row);
	*x = ws.ws_col;
	*y = ws.ws_row;

	return 0;
}


// --------------------------------------------------------------------------------
// 複数のディレクトリ/ファイル引数対応の構造体、1 引数毎に管理する
struct DENT {
	// データ
	char *path;							// 引数の指定ディレクトリ
	struct dirent **direntlist;			// ディレクトリの全エントリ
	int nth;							// エントリ数
	struct FNAME *fnamelist;			// 全エントリのファイル情報
	int startn;							// sourlist の先頭

	// unique かどうか、最長連続 32 文字までカウント
	int count_chklen[UNIQUE_LENGTH];

	// 引数の処理
	int is_file;						// その引数はファイル
	char is_filename[FNAME_LENGTH];		// パスから切り離したファイル名

	// インデント
	int inode_digits;
	int nlink_digits;
	int mode_digits;
	int owner_digits;					// owner 文字列の最大桁数
	int group_digits;					// group 文字列の最大桁数
	int size_digits;					// ファイルサイズの最大桁数
	int count_digits;					// ファイル数とファイルサイズの混合、の最大桁数
	int date_digits;					// 日付文字列の最大桁数
	int time_digits;					// 日時
	int week_digits;					// 曜日
	int path_digits;
	int name_digits;
	int kind_digits;
	int linkname_digits;
	int errnostr_digits;

	// 重複リスト
	struct DLIST *duplist;
#ifdef DEBUG
	int countSearchDlist;				// DEBUG のみ、不要にする
#endif
};


// 引数名と値を表示する
typedef enum {
	show_simple,
	show_long,
	format_list,

	deep_unique,
	beginning_word,
	use_emacs,
	paint_string,

	unique_word,
	show_dotfile,
	only_directory,
	only_paint_string,

	no_color,
	argv_color,
	default_color,

	mtime_sort,
	size_sort,
	name_sort,
	no_sort,

	readable_date,
	readable_size,
	aggregate_results,

	show_help,
	show_version,
	output_escape,

	AListCount
} ALIST;


#ifdef DEBUG
// 引数の全スイッチを表示
void
showArgvswitch(int alist[])
{
	// ALIST と同じ順番にする
	char *aname[AListCount] = {
		toStr(show_simple),
		toStr(show_long),
		toStr(format_list),

		toStr(deep_unique),
		toStr(beginning_word),
		toStr(use_emacs),
		toStr(paint_string),

		toStr(unique_word),
		toStr(show_dotfile),
		toStr(only_directory),
		toStr(only_paint_string),

		toStr(no_color),
		toStr(argv_color),
		toStr(default_color),

		toStr(mtime_sort),
		toStr(size_sort),
		toStr(name_sort),
		toStr(no_sort),

		toStr(readable_date),
		toStr(readable_size),
		toStr(aggregate_results),

		toStr(show_help),
		toStr(show_version),
		toStr(output_escape),
	};

	printf("show all switch:\n");
	for (int i=0; i<AListCount; i++) {
		if (alist[i]) {
			printf(" %s: %d\n", aname[i], alist[i]);
		}
	}
}
#endif


#define MAX(a, b) (((unsigned int)a) > ((unsigned int)b) ? ((unsigned int)a) : ((unsigned int)b))


void
swap(int *a, int *b)
{
	if (a != b) {			// XOR なので、異なる場合にのみ実行
		*a = *a ^ *b;
		*b = *a ^ *b;
		*a = *a ^ *b;
	}
}


void
ordersort(int showorder[], struct DENT dent[], int dirarg)
{
	for (int i = 0; i < dirarg - 1; i++) {
		int swapped = 0;

		for (int j = 0; j < dirarg - i - 1; j++) {
			if (dent[showorder[j]].is_file == 0 && dent[showorder[j + 1]].is_file == 1) {
				swap(&showorder[j], &showorder[j + 1]);
				swapped = 1;
			}
		}
		if (swapped == 0) {
			break;
		}
	}
}


int
main(int argc, char *argv[])
{
	char dirarglist[argc][FNAME_LENGTH];	// 引数の dir のリスト
	int dirarg = 0;
	int argverr[argc];						// argv のエラー記録

	struct ENCLOSING enc;

	unsigned short int termlen = 0;
	unsigned short int termhei = 0;

	// 引数のオプションフラグ
	int alist[AListCount];
	memset(alist, 0, sizeof(alist));

	char onlyPaintStr[FNAME_LENGTH];
	char formatListString[FNAME_LENGTH] = "mogcdPNkLE";

	// 引数のファイル指定の数を数える (ある/無しのフラグ)
	int count_is_file = 0;

	// ================================================================================
	// 毎回 fullpath を作成するのではなく cd する
	char cwd[FNAME_LENGTH];

	if (getcwd(cwd, FNAME_LENGTH) == NULL) {
		perror("getcwd");
		printf(" =>cwd:%s\n", cwd);

		exit(EXIT_FAILURE);
	}
	debug printf("cwd:%s\n", cwd);

	// ================================================================================
	// myStrlen() 用の初期化
	if (setlocale(LC_ALL, "") == NULL) {
		perror("setlocale");
		printf(" =>setlocale(LC_ALL, \"\");\n");

		exit(EXIT_FAILURE);
	}

	// ================================================================================
	// mytolower() 用の初期化
#ifdef MYTOLOWER
	for (int i = 0; i <= UCHAR_MAX; i++){
		map[i] = i;
	}

	int len = strlen(upper);
	for (int i = 0; i < len; i++){
		map[(unsigned char) upper[i]] = lower[i];
// 		map[lower[i]] = upper[i];
	}
#endif

	// ================================================================================
	// makeDate() 用の初期化

	// プログラム開始直後 (大体今) を1回だけ
	time_t lt;
	lt = time(NULL);

	// ================================================================================
	// Control Sequence Introducer
	// https://en.wikipedia.org/wiki/ANSI_escape_code
	// https://kmiya-culti.github.io/RLogin/ctrlcode.html			2.7.2 CSI

	// デフォルトカラー
	// 属性 太=1, (細=2), イタリック=3, 下線=4, 点滅=5, (速い点滅=6), 文字/背景の反転=7, (隠す=8), 取消=9
	// 他   2重下線=21, 上線=53

	char color_txt[sizeof(default_color_txt)] = 
		   // 8 色
		   "base=37:normal=34:dir=36:fifo=33:socket=35:device=33:error=31:paint=32:"					// 文字色
// 		   "base=100:"																					// 背景色
// 		   "paint=95:paint=5:"	// 点滅させるためには、明るい色を指定する必要あり

		   // 256 色、3000 は fore, 4000 は back
// 		   "base=3007:normal=3012:dir=3014:fifo=3011:socket=3009:device=3011:error=3009:paint=3032"		// 文字色

// 		   "normal=1:dir=1:socket=1:device=1:label=1:error=1:paint=1:"						// 属性
		   "reset=0"
		   ;


	strcpy(default_color_txt, color_txt);
	initColor(0, color_txt);

	// ================================================================================
	// 引数の処理
	memset(dirarglist, '\0', sizeof(dirarglist));
	memset(argverr, base, sizeof(argverr));
	paintString[0] = '\0';
	paintStringLen = 0;
	onlyPaintStr[0] = '\0';

	enc.textbegin[0] = '\0';
	enc.textend[0] = '\0';
	enc.lbegin = 0;
	enc.lend = 0;
	enc.tlen = 0;

	for (int i=1; i<argc; i++) {
		int len = strlen(argv[i]);
		// --------------------------------------------------------------------------------
		// 完全一致の引数
		if (strcmp(argv[i], "-256") == 0) {
			showEscapeList();
			exit(EXIT_SUCCESS);
		}

		if (strcmp(argv[i], "-help") == 0) {
			alist[show_help]++;
			continue;
		}

		if (strcmp(argv[i], "-version") == 0) {
			alist[show_version]++;
			continue;
		}

		if (strcmp(argv[i], "-always") == 0) {
			alist[output_escape]++;
			continue;
		}

		// --------------------------------------------------------------------------------
		// 引数の後にオプション文字列
		// 文字指定 (begin)
		if (strncmp(argv[i], "-TB", 3) == 0) {
			if (len == 3) {
				argverr[i] = error;
				continue;
			}

			strcpy(enc.textbegin, argv[i]+3);
			enc.lbegin = strlen(enc.textbegin);
			enc.tlen = enc.lbegin + enc.lend;
			continue;
		}
		// 文字指定 (end)
		if (strncmp(argv[i], "-TE", 3) == 0) {
			if (len == 3) {
				argverr[i] = error;
				continue;
			}

			strcpy(enc.textend, argv[i]+3);
			enc.lend = strlen(enc.textend);
			enc.tlen = enc.lbegin + enc.lend;
			continue;
		}

		// 表示順指定
		if (strncmp(argv[i], "-f", 2) == 0) {
			if (len == 2) {
				argverr[i] = error;
				continue;
			}

			alist[format_list]++;
			// 後の指定が優先 (上書き) される
			strcpy(formatListString, argv[i]+2);
			continue;
		}

		// 色指定
		if (strncmp(argv[i], "-c", 2) == 0) {
			if (len == 2) {
				argverr[i] = error;
				continue;
			}

			alist[argv_color]++;
			// 後の指定が優先 (上書き) される
			strcpy(color_txt, argv[i]+2);
			continue;
		}

		// 着色文字列指定
		if (strncmp(argv[i], "-p", 2) == 0) {
			if (len == 2) {
				argverr[i] = error;
				continue;
			}

			alist[paint_string]++;
			// 後の指定が優先 (上書き) される
			char *tmp = argv[i]+2;
			int tmplen = strlen(tmp);
			// 比較用に小文字化
			for (int j=0; j<tmplen; j++) {
				paintString[j] = myTolower(tmp[j]);
			}
			paintString[tmplen] = '\0';
			paintStringLen = tmplen;
			continue;
		}

		// マッチング文字列、該当ファイル表示
		if (strncmp(argv[i], "-P", 2) == 0) {
			if (len == 2) {
				argverr[i] = error;
				continue;
			}

			alist[only_paint_string]++;
			strcpy(onlyPaintStr, argv[i]+2);
			continue;
		}

		// --------------------------------------------------------------------------------
		// '-' から始まるのはオプション、、、パス名の場合は、"./-xx" などにして
		if (argv[i][0] == '-') {
			// スイッチが何も指定されていない "-" の時
			if (len == 1) {
				argverr[i] = error;
				continue;
			}

			for (int j=1; j<len; j++) {
				switch (argv[i][j]) {
					case 's': alist[show_simple]++;       break;	// lstat() を使用しない printShort()
					case 'l': alist[show_long]++;         break;	// long 表示

					case 'u': alist[deep_unique]++;       break;	// unique チェック回数を減らさない
					case 'b': alist[beginning_word]++;    break;	// uniqueCheckFirstWord() のみ
					case 'e': alist[use_emacs]++;         break;	// emacs 系ファイル名対応

					case 'a': alist[show_dotfile]++;      break;	// '.' から始まるファイルを表示
					case 'o': alist[only_directory]++;    break;	// ディレクトリのみ表示

					case 'n': alist[no_color]++;          break;	// no color 表示
					case 'd': alist[default_color]++;     break;	// default color 表示

					case 'm': alist[mtime_sort]++;        break;	// mtime でソート
					case 'z': alist[size_sort]++;         break;	// size でソート
					case 'N': alist[name_sort]++;         break;	// name でソート
					case 'O': alist[no_sort]++;           break;	// ソート無し

					case 't': alist[readable_date]++;     break;	// human readable time
					case 'i': alist[readable_size]++;     break;	// human readable size
					case 'r': alist[aggregate_results]++; break;	// 集計結果表示

					case 'h': alist[show_help]++;         break;	// help 表示
					case 'v': alist[show_version]++;      break;	// version 表示

					default: {
						// 上記以外の "-x" ではヘルプ表示
						argverr[i] = error;
					}
					break;
				}
			}
		} else {
			// '-' から始まっていないので、多分ファイル名、パスの指定
			strcpy(dirarglist[dirarg], argv[i]);
			dirarg++;
		}
	}

	// --------------------------------------------------------------------------------
	// 色をつけない
	if (alist[no_color]) {
		// 色の初期化
		for (int i=0; i<ListCount; i++) {
			colorlist[i][0] = '\0';
		}
	}

	// --------------------------------------------------------------------------------
	// 引数にエラーがあった
	for (int i=0; i<argc; i++) {
		if (argverr[i] == error) {
			argvUsage(argv);

			// 引数表示
			printf("\n");
			printf("Include Unknown or Missing Option.\n");
			for (int j=0; j<argc; j++) {
				printf(" ");
				printStr(argverr[j], argv[j]);
			}
			printf("\n");

			exit(EXIT_FAILURE);
		}
	}

	if (alist[show_help]) {
		initColor(1, "");
		argvUsage(argv);
		exit(EXIT_SUCCESS);
	}

	if (alist[show_version]) {
		showVersion(argv);
		exit(EXIT_SUCCESS);
	}

	// ================================================================================
	// 出力先ターミナルサイズの取得、取得できなければ 120x35 とする
	if (getTerminalSize(&termlen, &termhei) == -1) {
		termlen = 120;
		termhei = 35;
	}
	debug printf("terminal size: width:%d, height:%d\n", termlen, termhei);

	// --------------------------------------------------------------------------------
	// 出力先の確認
	// terminal でない場合は無色、-always が指定された場合は color
	if (isatty(fileno(stdout)) == 0 && alist[output_escape] == 0) {
		alist[no_color]++;
	}

	// ================================================================================
	// 依存・関連する argv の処理
	int (*comparefunc) (const struct dirent **, const struct dirent **);	// scandir() 時のソート
	int (*sortfunc)(const void *, const void *);							// 表示時のソート

	// デフォルトは、uniqueCheck() 向けの設定
	comparefunc = sortNameLength;		// ファイル名の長い順で scandir() -> uniqueCheck(), uniqueCheckFirstWord() の比較回数が一番小さくなる
	alist[unique_word] = 1;				// uniqueCheck(), uniqueCheckFirstWord() を実行する
	printName = printUnique;			// uniqueCheck(), uniqueCheckFirstWord() 結果を表示する
	sortfunc = myAlphaSort;				// 最後にアルファベット順で表示

	// --------------------------------------------------------------------------------
	// dirent 引数無しなので、カレントディレクトリをリストに加える
	if (dirarg == 0) {
		strcpy(dirarglist[0], "./");
		dirarg = 1;
	}

	// -p 指定文字列で色付け
	if (alist[paint_string]) {
		alist[unique_word] = 0;
		alist[deep_unique] = 0;
		alist[beginning_word] = 0;
		alist[use_emacs] = 0;
		comparefunc = sortNameAlphabet;
		printName = printMatchedString;
		sortfunc = NULL;					// sortNameAlphabet でソート済みだから、myAlphaSort しない
	}

	// shimple 表示は long にしない (ファイルの種類を問わず単色表示)
	if (alist[show_simple]) {
		alist[show_long] = 0;
		alist[readable_date] = 0;
		alist[readable_size] = 0;
		alist[format_list] = 0;
	}

	// ----------------------------------------
	// -l 表示に変更
	// format_list
	if (alist[format_list]) {
		alist[show_long]++;
	}

	// human readable date/size は long 表示
	if (alist[readable_date]) {
		alist[show_long]++;
	}
	if (alist[readable_size]) {
		alist[show_long]++;
	}

	// only_directory の時は、'.' から始まるディレクトリも表示する
	if (alist[only_directory]) {
		alist[show_dotfile]++;
	}

	// --------------------------------------------------------------------------------
	// 下に行くほど強い (sortfunc を上書きするから)
	if (alist[use_emacs]) {
		comparefunc = sortNameAlphabet;		// alphabet 順で scandir()
		sortfunc = NULL;					// sortNameAlphabet でソート済みだから、myAlphaSort しない
	}

	// mtime 順にソートする
	if (alist[mtime_sort]) {
		// 新しい順
		sortfunc = myMtimeSort;
		if (alist[mtime_sort] > 1) {
			// 逆順
			sortfunc = myMtimeSortRev;
		}
	}

	// size 順にソートする
	if (alist[size_sort]) {
		// 小さい順
		sortfunc = mySizeSort;
		if (alist[size_sort] > 1) {
			// 逆順
			sortfunc = mySizeSortRev;
		}
	}

	// name 順にソートする
	if (alist[name_sort]) {
		// 逆順
		sortfunc = myAlphaSortRev;
		if (alist[name_sort] > 1) {
			// 通常
			sortfunc = myAlphaSort;
		}
	}

	// ソートしない
	if (alist[no_sort]) {
		comparefunc = NULL;
		sortfunc = NULL;
	}

	// --------------------------------------------------------------------------------
	// -TB, -TE の設定
	// 片側の指定だけなら同じ文字列を使用
	if (enc.lbegin == 0) {
		strcpy(enc.textbegin, enc.textend);
		enc.lbegin = enc.lend;
		enc.tlen = enc.lbegin + enc.lend;
	}
	if (enc.lend == 0) {
		strcpy(enc.textend, enc.textbegin);
		enc.lend = enc.lbegin;
		enc.tlen = enc.lbegin + enc.lend;
	}
#ifdef DEBUG
	if (enc.lbegin == 0 && enc.lend == 0) {
		strcpy(enc.textbegin, "[");
		strcpy(enc.textend,   "]");
		enc.lbegin = strlen(enc.textbegin);
		enc.lend   = strlen(enc.textend);
		enc.tlen = enc.lbegin + enc.lend;
	}
#endif

	// --------------------------------------------------------------------------------
	// argv[i] は順不同なので、-n > -c > -d > env color > default color の優先順位を実装
	do {
		// 色をつけない
		if (alist[no_color]) {
			alist[argv_color] = 0;
			alist[default_color]++;		// 環境変数を見ない

			color_txt[0] = '\0';		// 引数の色指定も無し

			// -TB, -TE と併用で無ければ、色をつけないから uniqueCheck(), uniqueCheckFirstWord() を飛ばす
			// -TB, -TE が指定されていたら、uniqueCheck() は行う
			if (enc.lbegin == 0) {
				alist[unique_word] = 0;
				alist[use_emacs] = 0;
			}

			// 色の初期化
			for (int i=0; i<ListCount; i++) {
				colorlist[i][0] = '\0';
			}
			break;
		}

		// 引数の色指定
		if (alist[argv_color]) {
			alist[default_color] = 0;
			break;
		}

		// 環境変数を見ない、引数の色は見ない
		if (alist[default_color]) {
			break;
		}

		// env color
		// 環境変数の色を設定、引数の色は見ない
		alist[default_color] = 0;
		color_txt[0] = '\0';
	} while (0);

#ifdef DEBUG
	showArgvswitch(alist);
#endif

	// ================================================================================
	// 引数か、環境変数から色を指定する
	initColor(alist[default_color], color_txt);

	// ================================================================================
	// 引数のディレクトリ分の準備、初期化
	struct DENT dent[dirarg];
	int showorder[dirarg];

	// 初期化
	for (int i=0; i<dirarg; i++) {
		dent[i].path = dirarglist[i];
		dent[i].nth = 0;
		dent[i].startn = 0;

		memset(dent[i].count_chklen, 0, sizeof(dent[i].count_chklen));
		dent[i].is_file = 0;
		memset(dent[i].is_filename, 0, sizeof(dent[i].is_filename));

		dent[i].inode_digits  = 0;
		dent[i].nlink_digits  = 0;
		dent[i].mode_digits  = 0;
		dent[i].owner_digits = 0;
		dent[i].group_digits = 0;
		dent[i].size_digits  = 0;
		dent[i].count_digits = 0;
		dent[i].date_digits  = 0;
		dent[i].time_digits  = 0;
		dent[i].week_digits  = 0;
		dent[i].path_digits  = 0;
		dent[i].name_digits  = 0;
		dent[i].kind_digits  = 0;
		dent[i].linkname_digits = 0;
		dent[i].errnostr_digits = 0;

		// non-unique リストの初期化
		dent[i].duplist = malloc(sizeof(struct DLIST));
		dent[i].duplist->dupword[0] = '\0';
		dent[i].duplist->left = NULL;
		dent[i].duplist->right = NULL;
#ifdef DEBUG
		dent[i].countSearchDlist = 0;
#endif

		showorder[i] = i;
	}

	// ================================================================================
	// データの取得
	// 引数の処理
	for (int i=0; i<dirarg; i++) {
		struct DENT *p;
		p = &dent[i];

		// -l 指定時、最後に '/' がないと、ファイルとしてパスを分離
		// printShort() の場合、引数がディレクトリの場合、 / ある無しに関わらず、ディレクトリとして扱う
		struct stat st;
		int ret;

		ret = stat(dirarglist[i], &st);
		// -1: 失敗した場合もファイルとして扱う、そのために、最後の '/' を削除する
		if (ret == -1) {
			p->is_file = 1;
			char *tmp;

			tmp = dirarglist[i];
			if (tmp[strlen(tmp)-1] == '/') {
				tmp[strlen(tmp)-1] = '\0';
			}
		}

		// そのファイル自体とリンク先がディレクトリではない場合、ファイルとして扱う
		if (S_ISDIR(st.st_mode) == 0) {
			p->is_file = 1;
		}

		// printLong()  の場合、/ のある無しによって、ディレクトリとファイルの使い分け
		if (alist[show_long]) {
			if (dirarglist[i][strlen(dirarglist[i]) -1] != '/') {
				p->is_file = 1;
			}
		}

		if (p->is_file) {
			p->startn = -1;
			if (strrchr(dirarglist[i], '/')) {
				// ディレクトリとファイル名を '/' で分割する
				strcpy(p->is_filename, strrchr(dirarglist[i], '/') + 1);
				dirarglist[i][strlen(dirarglist[i]) - strlen(p->is_filename)] = '\0';
			} else {
				// カレントディレクトリっぽい
				strcpy(p->is_filename, dirarglist[i]);
				strcpy(dirarglist[i], "./");
			}
			debug printf("dir:[%s], fname[%s]\n", dirarglist[i], p->is_filename);
		}
	}

	// --------------------------------------------------------------------------------
	// 引数のディレクトリ分、表示を繰り返す
	// データの取得
	for (int i=0; i<dirarg; i++) {
		struct DENT *p;
		p = &dent[i];

		// 多分パス、移動に失敗したら次のパス
		if (chdir(dirarglist[i]) != 0) {
			perror("chdir");
			debug printf(" (=>chdir to %s fail.)\n", dirarglist[i]);
			printf("\n");

			continue;
		}

		// --------------------------------------------------------------------------------
		p->nth = scandir("./", &p->direntlist, NULL, comparefunc);
		if (p->nth == -1) {
			perror("scandir");
			printf(" =>path:%s\n", dirarglist[i]);

			exit(EXIT_FAILURE);
		}

		// --------------------------------------------------------------------------------
		// 配列で fnamelist の確保
		p->fnamelist = (struct FNAME *) malloc(sizeof(struct FNAME) * p->nth);

		struct FNAME *fnamelist;
		fnamelist = p->fnamelist;

		struct dirent **direntlist;
		direntlist = p->direntlist;

		if (p->fnamelist == NULL) {
			perror("malloc");
			printf(" =>size:%zu\n", sizeof(struct FNAME) * p->nth);

			// cwd を試みる
			if (chdir(cwd)) {
				perror("chdir");
				printf(" =>chdir to %s fail.\n", cwd);
			}

			exit(EXIT_FAILURE);
		}

		// fnamelist に登録
		for (int j=0; j<p->nth; j++) {
			// ファイル名の登録
			addFNamelist(&fnamelist[j], direntlist[j]->d_name);

			// linkedlist を繋ぐ
			fnamelist[j].nextn = j+1;
			fnamelist[j].prevn = j-1;

			// -s はファイル名しか使用しない
			if (alist[show_simple]) {
				continue;
			}

			// --------------------------------------------------------------------------------
			// 表示に関係なく、uniqueCheck(), uniqueCheckFirstWord() で使用
			// printLong(), printShort() 両方で必要な共通処理

			if (lstat(direntlist[j]->d_name, &fnamelist[j].sb) == -1) {
				// lstat() が失敗した時の処理 (-l /mnt/c/)

				// 失敗のエラーメッセージを errnostr に格納
				strcpy(fnamelist[j].errnostr, strerror(errno));
				fnamelist[j].color = error;

				// 必ず、SHOW_LONG, SHOW_SHORT に属させる事
				if (alist[show_long]) {
// 					strcpy(fnamelist[j].mode,  "//////////");
					strcpy(fnamelist[j].inode, "-");
					strcpy(fnamelist[j].nlink, "-");
					strcpy(fnamelist[j].mode,  "-");
					strcpy(fnamelist[j].owner, "-");
					strcpy(fnamelist[j].group, "-");
					strcpy(fnamelist[j].size,  "-");
					strcpy(fnamelist[j].count, "-");
					strcpy(fnamelist[j].date,  "-");
					strcpy(fnamelist[j].time,  "-");
					strcpy(fnamelist[j].week,  "-");
					strcpy(fnamelist[j].kind,  "");

					fnamelist[j].showlist = SHOW_LONG;
				} else {
					fnamelist[j].showlist = SHOW_SHORT;
				}

				continue;
			}

			// only_directory の IS_DIRECTORY() で使用
			makeMode(&fnamelist[j]);		// mode data
		}
		fnamelist[p->nth-1].nextn = -1;

		// --------------------------------------------------------------------------------
		// cwd ディレクトリに戻る
		if (chdir(cwd)) {
			perror("chdir");
			printf(" =>chdir to %s fail.\n", cwd);

			exit(EXIT_FAILURE);
		}
	}


#ifdef DEBUG
	for (int i=0; i<dirarg; i++) {
		printf("dent:\n");
		printf(" %d:\n", i);
		printf("  path:%s, nth:%d\n", dent[i].path, dent[i].nth);
		printf("  is_file:%d, (%s)\n", dent[i].is_file, dent[i].is_filename);
	}
	printf("\n");
#endif

	// ================================================================================
	// データの加工
	// fnamelist の処理、uniqueCheck() 対象のデータの選別
	// sourcelist: unique check の対象にするか
	// targetlist: uniqueCheck(), uniqueCheckFirstWord() の対象にするか
	// showlist:   printShort(), printLong() で表示する対象

	for (int i=0; i<dirarg; i++) {
		struct DENT *p;
		p = &dent[i];

		struct FNAME *fnamelist;
		fnamelist = p->fnamelist;

#ifdef DEBUG
		count_searchDuplist = &p->countSearchDlist;
#endif
		count_chklen = p->count_chklen;

		// ================================================================================
		// 引数は 1 ファイルの指定だった
		if (p->is_file) {
			int count = 0;

			for (int j=0; j<p->nth; j++) {
				if (strcmp(p->is_filename, fnamelist[j].name) != 0) {
					fnamelist[j].showlist = SHOW_NONE;		// 指定されたファイル以外、非表示設定
// 					fnamelist[j].sourcelist = -1;			// 検索候補からも除外
				} else {
					// 引数のファイル
					count++;

					// 指定されたパスを記録
					strcpy(fnamelist[j].path, dirarglist[i]);
				}
			}

			// 引数のファイルは見つからなかった
			// !! ここだけ表示が外れている
			if (count != 1) {
				char msg[strlen(dirarglist[i]) + strlen(p->is_filename) + 32];

				sprintf(msg, "No such file: [%s%s]\n", dirarglist[i], p->is_filename);
				printStr(base, msg);
			} else {
				// ファイル指定の数を数える
				count_is_file++;
			}
		}

		// --------------------------------------------------------------------------------
		// 対象は、emacs やアーカイバのタイプ (画像や音楽ファイル、3D ファイルも)
		if (alist[use_emacs]) {
			debug printf("emacs:\n");

			struct DLIST *extensionduplist;
			extensionduplist = malloc(sizeof(struct DLIST));
			extensionduplist->dupword[0] = '\0';
			extensionduplist->left = NULL;
			extensionduplist->right = NULL;

			// 1 個目だけ特別対応
			fnamelist[0].targetlist = 1;
			fnamelist[0].sourcelist = 1;

			for (int j=1; j<p->nth; j++) {
				// 拡張子対応、.el, .elc, .zip, .xxx 等
				// 2 回以上おなじ拡張子がある場合、uniqueCheck() の対象にならないように、拡張子を登録する
				char *extension = strchr(fnamelist[j].name, '.');
				if (extension) {
					int len = strlen(extension+1);
					if (searchDuplist(extensionduplist, extension+1, len)) {
						addDuplist(p->duplist, extension+1, len);
						debug printf("add duplist(extension):%s\n", extension+1);
					}
					addDuplist(extensionduplist, extension+1, len);
				}

				// --------------------------------------------------------------------------------
				// 2 つの名称を比較して、どの程度同じか判断する、、、、.el, .elc なら、[i] の方がファイル名が長いはず
				float m = matchPercent(fnamelist[j-1], fnamelist[j]);

				// 4 文字中 3 文字、5 文字中 4 文字以上同じ (x.el と、x.elc) で無ければ、別のファイルと考える
				if (m < 0.75) {
					fnamelist[j].targetlist = 1;
					fnamelist[j].sourcelist = 1;
				} else {
					fnamelist[j].targetlist = -1;
					fnamelist[j].sourcelist = -1;
// 長い方に色付け
#if 1
					// 同じグループと判断して、paint 色にする
					fnamelist[j].color = paint;

					// 前のファイル名とほぼ同じだけど、こっちの方がファイル名が長い (x.el と、x.elc)
					if (fnamelist[j].length > fnamelist[j-1].length) {
						fnamelist[j-1].targetlist = -1;
						fnamelist[j].targetlist = 1;

						fnamelist[j-1].sourcelist = -1;
						fnamelist[j].sourcelist = 1;
					}
#else
// 最初の方に色付け
					if (fnamelist[j].length > fnamelist[j-1].length) {
						fnamelist[j-1].color = paint;
					}
#endif
				}
			}
			freeDuplist(extensionduplist);

			runUniqueCheck(fnamelist, p->nth, &p->startn, p->duplist, uniqueCheck, alist[deep_unique]);

			// --------------------------------------------------------------------------------
			// 対象を戻す
			for (int j=0; j<p->nth; j++) {
				fnamelist[j].targetlist = 1;
				fnamelist[j].sourcelist = 1;
			}
		}

		// --------------------------------------------------------------------------------
		// '.' から始まるファイル/ディレクトリは表示対象外
		if (alist[show_dotfile] == 0) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].name[0] == '.') {
					fnamelist[j].showlist = SHOW_NONE;		// 非表示設定
// 					fnamelist[j].sourcelist = -1;			// 検索候補からも除外
// deleteList(fnamelist, j, &p->startn);
				}
			}
		}

		// --------------------------------------------------------------------------------
		// ディレクトリのみ表示する
		if (alist[only_directory]) {
			for (int j=0; j<p->nth; j++) {
				if (IS_DIRECTORY(fnamelist[j]) != 1) {
					fnamelist[j].showlist = SHOW_NONE;
// 					fnamelist[j].sourcelist = -1;		// 検索候補から除外
// deleteList(fnamelist, j, &p->startn);
				}
			}
		}

		// --------------------------------------------------------------------------------
		// printLong() でのみ使用する
		if (alist[show_long]) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}

				if (fnamelist[j].showlist == SHOW_SHORT) {
					fnamelist[j].showlist = SHOW_LONG;
				}

				// !! 本当は、sb をチェックする
				if (fnamelist[j].mode[1] == '\0') {
					continue;
				}

				struct stat sb;
				sb = fnamelist[j].sb;

				// filesize data
				makeSize(sb.st_size, fnamelist[j].size);

				// countsize data
				if (IS_DIRECTORY(fnamelist[j])) {
					// ディレクトリの場合は、最大エントリー数を記載
					int ret = countEntry(fnamelist[j].name, dirarglist[i]);

					if (ret != -1) {
						makeSize(ret, fnamelist[j].count);
					} else {
						// ディレクトリだけど、読めない
						strcpy(fnamelist[j].count, "-");
					}
				} else {
					// ファイルの時は、.size の値をコピー
					strcpy(fnamelist[j].count, fnamelist[j].size);
				}

				makeSize(sb.st_ino, fnamelist[j].inode);
				makeSize(sb.st_nlink, fnamelist[j].nlink);

				// サイズを読みやすくする
				if (alist[readable_size]) {
					makeReadableSize(fnamelist[j].size);
					makeReadableSize(fnamelist[j].count);

					makeReadableSize(fnamelist[j].nlink);
					// 単位じゃないから省略しない
// 					makeReadableSize(fnamelist[j].inode);
				}

				struct passwd *pw;
				struct group *gr;

				pw = getpwuid(sb.st_uid);					// owner
				if (pw == NULL) {
					perror("getpwuid");
					exit(EXIT_FAILURE);
				}
				strcpy(fnamelist[j].owner, pw->pw_name);

				gr = getgrgid(sb.st_gid);					// group
				if (gr == NULL) {
					perror("getgrgid");
					exit(EXIT_FAILURE);
				}
				strcpy(fnamelist[j].group, gr->gr_name);

				makeDate(&fnamelist[j], lt);

				// 時間を読みやすくする
				if (alist[readable_date]) {
					makeReadableDate(&fnamelist[j], lt);
				}
			}

#ifdef DEBUG
			printf("unique list: before\n");
			displayAllQsortdata(fnamelist, p->nth, enc);
#endif
		}


		// ================================================================================
		// 表示しないデータを間引く
		for (int j=0; j<p->nth; j++) {
			if (fnamelist[j].showlist == SHOW_NONE) {
				deleteList(fnamelist, j, &p->startn);
				continue;
			}

			// --------------------------------------------------------------------------------
			// SHOW_NONE 以外は continue はしない
			if (fnamelist[j].sourcelist == -1) {
				deleteList(fnamelist, j, &p->startn);
			}

			// "()" から始まるファイル/ディレクトリのマッチングは対象外、uniqueCheck() と合わせる ?
			if (strchr("()", fnamelist[j].name[0])) {
				deleteList(fnamelist, j, &p->startn);
			}
		}

		// ================================================================================
		// 文字列がマッチしたファイルのみ表示
		if (alist[only_paint_string]) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					deleteList(fnamelist, j, &p->startn);
					continue;
				}

				// pickupString() は makeDate() 後
				if (pickupString(fnamelist[j], onlyPaintStr, formatListString, strstr) == 1) {
					continue;
				}

				// 該当しない
				fnamelist[j].showlist = SHOW_NONE;
			}
		}

		// --------------------------------------------------------------------------------
		// 指定文字列に着色する
		if (alist[paint_string]) {
// 			memset(dent[i].count_chklen, 0, sizeof(dent[i].count_chklen));
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					deleteList(fnamelist, j, &p->startn);
					continue;
				}

				// pickupString() は makeDate() 後
				if (pickupString(fnamelist[j], paintString, formatListString, myStrcasestr) == 1) {
					count_chklen[paintStringLen]++;
					// hit した記録、aggregate_results でカウント
					fnamelist[j].uniqueend = 1;
				}
			}
		}

		// --------------------------------------------------------------------------------
		// unique 文字列に着色する
		if (alist[unique_word]) {
			void (*chkfunc)(struct FNAME *top, int chklen, int n, int *startn, struct DLIST *duplist);

			// uniqueCheckFirstWord() の次に uniqueCheck() を行う
			if (alist[beginning_word] == 0) {
				// --------------------------------------------------------------------------------
				// 対象を dotfile のみに
				for (int j=0; j<p->nth; j++) {
					if (fnamelist[j].name[0] == '.') {
						fnamelist[j].targetlist = 1;
						fnamelist[j].sourcelist = 1;
					} else {
						fnamelist[j].targetlist = -1;
						fnamelist[j].sourcelist = -1;
					}
				}

				runUniqueCheck(fnamelist, p->nth, &p->startn, p->duplist, uniqueCheckFirstWord, alist[deep_unique]);

				// --------------------------------------------------------------------------------
				// 対象を dotfile 以外に
				for (int j=0; j<p->nth; j++) {
					if (fnamelist[j].name[0] != '.') {
						fnamelist[j].targetlist = 1;
						fnamelist[j].sourcelist = 1;
					} else {
						fnamelist[j].targetlist = -1;
						fnamelist[j].sourcelist = -1;
					}
				}
				chkfunc = uniqueCheck;
			} else {
				// uniqueCheckFirstWord() のみを行う
				chkfunc = uniqueCheckFirstWord;
			}

			runUniqueCheck(fnamelist, p->nth, &p->startn, p->duplist, chkfunc, alist[deep_unique]);
		}

// 		printf("startn:%d\n", p->startn);
// 		printallList(fnamelist, p->nth);

		// ================================================================================
		// 全データに対して行う処理

		// 表示用にソートする
		if (sortfunc) {
			qsort(fnamelist, p->nth, sizeof(struct FNAME), sortfunc);
		}
		// scandir() と、qsort() で異なるソートをしたら、linkedlist を繋ぎなおす
// 		for (int j=0; j<p->nth; j++) {
// 			fnamelist[j].nextn = j+1;
// 			fnamelist[j].prevn = j-1;
// 		}
// 		fnamelist[p->nth -1].nextn = -1;

		// --------------------------------------------------------------------------------
		// 各項目の最大幅数の確定
		for (int j=0; j<p->nth; j++) {
			if (fnamelist[j].showlist == SHOW_NONE) {
				continue;
			}

			// 何文字エスケープされるか、printLength: の計算値、printUnique() と合わせる
			// !! printMatchedString() とも合わせる必用あり
			int count = 0;
			// uniqueCheck() が終わっている必要あり
			if (fnamelist[j].uniquebegin != -1) {
				for (int k=fnamelist[j].uniquebegin; k<=fnamelist[j].uniqueend; k++) {
					if (strchr(" ~#()\\$", fnamelist[j].name[k])) {
						count++;
					}
				}
				count += enc.tlen;
			}
			fnamelist[j].print_length = myStrlen(fnamelist[j]) + count;


			// --------------------------------------------------------------------------------
			// 各項目の最大表示長さ
			p->name_digits =  MAX(p->name_digits, countMatchedString(fnamelist[j].name) * enc.tlen + fnamelist[j].print_length);

			// -l で無ければ使用しない
			if (alist[show_long] == 0) {
				continue;
			}

			p->inode_digits = MAX(p->inode_digits, countMatchedString(fnamelist[j].inode) * enc.tlen + strlen(fnamelist[j].inode));
			p->nlink_digits = MAX(p->nlink_digits, countMatchedString(fnamelist[j].nlink) * enc.tlen + strlen(fnamelist[j].nlink));
			p->mode_digits =  MAX(p->mode_digits,  countMatchedString(fnamelist[j].mode)  * enc.tlen + strlen(fnamelist[j].mode));
			p->owner_digits = MAX(p->owner_digits, countMatchedString(fnamelist[j].owner) * enc.tlen + strlen(fnamelist[j].owner));
			p->group_digits = MAX(p->group_digits, countMatchedString(fnamelist[j].group) * enc.tlen + strlen(fnamelist[j].group));
			p->size_digits =  MAX(p->size_digits,  countMatchedString(fnamelist[j].size)  * enc.tlen + strlen(fnamelist[j].size));
			p->count_digits = MAX(p->count_digits, countMatchedString(fnamelist[j].count) * enc.tlen + strlen(fnamelist[j].count));
			p->date_digits =  MAX(p->date_digits,  countMatchedString(fnamelist[j].date)  * enc.tlen + strlen(fnamelist[j].date));
			p->time_digits =  MAX(p->time_digits,  countMatchedString(fnamelist[j].time)  * enc.tlen + strlen(fnamelist[j].time));
			p->week_digits =  MAX(p->week_digits,  countMatchedString(fnamelist[j].week)  * enc.tlen + strlen(fnamelist[j].week));
			p->kind_digits =  MAX(p->kind_digits,  countMatchedString(fnamelist[j].kind)  * enc.tlen + strlen(fnamelist[j].kind));
			p->path_digits =  MAX(p->path_digits,  countMatchedString(fnamelist[j].path)  * enc.tlen + strlen(fnamelist[j].path));
			p->linkname_digits =  MAX(p->linkname_digits, countMatchedString(fnamelist[j].linkname) * enc.tlen + strlen(fnamelist[j].linkname));
			p->errnostr_digits =  MAX(p->errnostr_digits, countMatchedString(fnamelist[j].errnostr) * enc.tlen + strlen(fnamelist[j].errnostr));
		}

#ifdef DEBUG
// 		printf("startn:%d\n", p->startn);
// 		printallList(fnamelist, p->nth);

		printf("unique list: after\n");
		displayAllQsortdata(fnamelist, p->nth, enc);
#endif
	}

	// ================================================================================
	// データの表示 (ファイル指定対応)

	// !! 表示順番の変更
	// 全データに対して行う
	for (int i=0; i<dirarg; i++) {
		struct DENT *p;
		p = &dent[i];

		struct FNAME *fnamelist;
		fnamelist = p->fnamelist;

		int llen = strlen(formatListString);

		// printLong() で無ければ使用しない
		if (alist[show_long]) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}

				// 各文字に対応するポインタを配列に格納
				char *info_pointers[256] = {NULL};
				info_pointers['i'] = info_pointers['I'] = fnamelist[j].inode;
				info_pointers['h'] = info_pointers['H'] = fnamelist[j].nlink;
				info_pointers['m'] = info_pointers['M'] = fnamelist[j].mode;
				info_pointers['o'] = info_pointers['O'] = fnamelist[j].owner;
				info_pointers['g'] = info_pointers['G'] = fnamelist[j].group;
				info_pointers['s'] = info_pointers['S'] = fnamelist[j].size;
				info_pointers['c'] = info_pointers['C'] = fnamelist[j].count;
				info_pointers['d'] = info_pointers['D'] = fnamelist[j].date;
				info_pointers['t'] = info_pointers['T'] = fnamelist[j].time;
				info_pointers['w'] = info_pointers['W'] = fnamelist[j].week;
				info_pointers['p'] = info_pointers['P'] = fnamelist[j].path;
				info_pointers['n'] = info_pointers['N'] = fnamelist[j].name;
				info_pointers['k'] = info_pointers['K'] = fnamelist[j].kind;
				info_pointers['l'] = info_pointers['L'] = fnamelist[j].linkname;
				info_pointers['e'] = info_pointers['E'] = fnamelist[j].errnostr;
				// --------------------------------------------------------------------------------
				info_pointers['['] = "[";
				info_pointers[']'] = "]";

				for (int k=0; k<llen; k++) {
					fnamelist[j].info[k] = info_pointers[(unsigned char) formatListString[k]];
				}
			}
		}
	}

	// --------------------------------------------------------------------------------
	// 引数の表示順変更
#ifdef DEBUG
	for (int i=0; i<dirarg; i++) {
		printf("show order:%d, %d\n", showorder[i], dent[showorder[i]].is_file);
	}
	printf("\n");
#endif

	// ディレクトリ表示があと
	ordersort(showorder, dent, dirarg);

#ifdef DEBUG
	for (int i=0; i<dirarg; i++) {
		printf("show order:%d, %d\n", showorder[i], dent[showorder[i]].is_file);
	}
#endif

	// --------------------------------------------------------------------------------
	// データ表示 (まずは is_file のデータ)、sort はしない
	if (count_is_file) {
		printStr(base, "single files:\n");

		int k = 0;
		if (alist[show_long]) {
			int digits[256] = {0};

			// is_file たちのそれぞれの最大桁数を記録する
			for (int i=0; i<dirarg; i++) {
				struct DENT *p;
				p = &dent[i];

				// is_file では無いので対象外
				if (p->is_file == 0) {
					continue;
				}

				// --------------------------------------------------------------------------------
				digits['i'] = MAX(p->inode_digits, digits['I']);		// inode
				digits['h'] = MAX(p->nlink_digits, digits['H']);		// hard link
				digits['m'] = MAX(p->mode_digits,  digits['m']);		// mode
				digits['o'] = MAX(p->owner_digits, digits['o']);		// owner
				digits['g'] = MAX(p->group_digits, digits['g']);		// group
				digits['w'] = MAX(p->week_digits,  digits['w']);		// 曜日
				digits['t'] = MAX(p->time_digits,  digits['T']);		// 日付
				digits['s'] = MAX(p->size_digits,  digits['s']);		// 最大ファイルサイズ
				digits['c'] = MAX(p->count_digits, digits['c']);		// 最大エントリー数の桁数
				digits['d'] = MAX(p->date_digits,  digits['d']);		// 日付の桁数
				digits['p'] = MAX(p->path_digits,  digits['p']);		// path
				digits['n'] = MAX(p->name_digits,  digits['n']);		// 名前の桁数
				digits['k'] = MAX(p->kind_digits,  digits['k']);		// 種類の桁数
				digits['l'] = MAX(p->linkname_digits, digits['l']);		// linkname
				digits['e'] = MAX(p->errnostr_digits, digits['e']);		// errnostr
			}

			// is_file のデータは全て表示する
			for (int i=0; i<dirarg; i++) {
				struct DENT *p;
				p = &dent[showorder[i]];

				struct FNAME *fnamelist;
				fnamelist = p->fnamelist;

				// is_file では無いので対象外
				if (p->is_file == 0) {
					continue;
				}

				// 表示レイアウト
				printLong(fnamelist, p->nth, enc, digits, formatListString);

				// 次があれば \n する
				if (i != dirarg-1) {
					k++;
				}
			}

		} else {

			// !! printShort() 向け
			struct FNAME data[count_is_file];

			int count = 0;
			// is_file のデータは全て表示する
			for (int i=0; i<dirarg; i++) {
				struct DENT *p;
				p = &dent[showorder[i]];

				struct FNAME *fnamelist;
				fnamelist = p->fnamelist;

				// is_file では無いので対象外
				if (p->is_file == 0) {
					continue;
				}

				// 表示レイアウト
				// is_file データを struct FNAME に登録
				for (int j=0; j<p->nth; j++) {
					if (fnamelist[j].showlist == SHOW_NONE) {
						continue;
					}

					// 必要な項目のコピー
					data[count].name = fnamelist[j].name;
					data[count].lowername = fnamelist[j].lowername;

					data[count].showlist = fnamelist[j].showlist;
					data[count].uniquebegin = fnamelist[j].uniquebegin;
					data[count].uniqueend = fnamelist[j].uniqueend;
					data[count].print_length = fnamelist[j].print_length;
					data[count].color = fnamelist[j].color;
					count++;

					// 次があれば \n する
					if (i != dirarg-1) {
						k++;
					}
				}
			}

			printShort(data, count_is_file, enc, termlen, termhei);
		}

#ifdef DEBUG
		printf("dirarg:%d\n", dirarg);

		for (int i=0; i<dirarg; i++) {
			printf("show order:%d, %d\n", showorder[i], dent[showorder[i]].is_file);
		}
		exit(1);
#endif

		// is_file が 1 つ有った
		if (k  && dent[showorder[dirarg-1]].is_file == 0) {
			printf("\n");
		}
	}
	// --------------------------------------------------------------------------------
	// データの表示 (パス指定対応)
	for (int i=0; i<dirarg; i++) {
		struct DENT *p;
		p = &dent[showorder[i]];

		// is_file は表示済み、その引数はスキップ
		if (p->is_file == 1) {
			continue;
		}

		struct FNAME *fnamelist;
		fnamelist = p->fnamelist;

		// 各 digis に対応するポインタを配列に格納
		int digits[256] = {0};
		if (alist[show_long]) {
			digits['i'] = p->inode_digits;
			digits['h'] = p->nlink_digits;
			digits['m'] = p->mode_digits;
			digits['o'] = p->owner_digits;
			digits['g'] = p->group_digits;
			digits['s'] = p->size_digits;
			digits['c'] = p->count_digits;
			digits['d'] = p->date_digits;
			digits['t'] = p->time_digits;
			digits['w'] = p->week_digits;
			digits['p'] = p->path_digits;
			digits['n'] = p->name_digits;
			digits['k'] = p->kind_digits;
			digits['l'] = p->linkname_digits;
			digits['e'] = p->errnostr_digits;
		}

		// ================================================================================
		// is_file 時の表示に合わせる
		if (dirarg > 1) {
			printStr(base, p->path);
			printStr(base, ":");
			printf("\n");
		}

		// --------------------------------------------------------------------------------
		// fnamelist の表示

		// 表示レイアウト
		printLong( fnamelist, p->nth, enc, digits, formatListString);
		printShort(fnamelist, p->nth, enc, termlen, termhei);

		// ================================================================================
		// 集計表示
		if (alist[aggregate_results]) {
			int hitcount = 0;
			int displaycount = 0;

			// 表示しないものは nth から引く
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}
				displaycount++;

				// paint_string は -1 -> 1 を設定
				if (fnamelist[j].uniqueend != -1) {
					hitcount++;
				}
			}

			printAggregate(hitcount, displaycount, p->nth, p->count_chklen);
		}

		// --------------------------------------------------------------------------------
		if (i != dirarg-1) {
			printf("\n");
		}
	}

	// ================================================================================
	// free() は表示終了後
	debug printf("----------\n");
	for (int i=0; i<dirarg; i++) {
		for (int j=0; j<dent[i].nth; j++) {
			free(dent[i].fnamelist[j].lowername);
		}
		if (dent[i].nth) {
			free(dent[i].fnamelist);
		}

		// --------------------------------------------------------------------------------
		// デバッグ情報の表示
#ifdef DEBUG
		printf("path: %s\n", dent[i].path);
		// duplist の情報表示
		printf(" duplist search count:%d\n", dent[i].countSearchDlist);


		printf(" deleteList count:%d\n", count_deletelist);

// 		displayDuplist(dent[i].duplist);
#endif

		freeDuplist(dent[i].duplist);
	}

	return (EXIT_SUCCESS);
}
