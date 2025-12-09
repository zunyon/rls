/*
	MIT License
	
	Copyright (c) 2025 zunyon
	
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	
	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

// ================================================================================
#define VERSION "0.4.0"
// 2024, 09/01 Ver. 0.1.0
// 2025, 01/13 Ver. 0.2.0
// 2025, 08/23 Ver. 0.3.0

// build date
#define INCDATE
#define BYEAR "2025"
#define BDATE "12/09"
#define BTIME "22:44:30"

#define RELTYPE "[CURRENT]"


// --------------------------------------------------------------------------------
// Last Update:
// my-last-update-time "2025, 12/09 22:42"

// 一覧リスト表示
//   ファイル名のユニークな部分の識別表示
//   指定文字情報を含むファイルの選別表示
// 出力レイアウトの変更
//   long list 表示では、横方向は -f で、縦方向はソート
//   short list 表示では、リダイレクト時もレイアウトを崩さない

// rls.fish の準備
// countfunction.c の準備
// OpenMP への対応


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

#ifdef MD5
#include <openssl/evp.h>
#endif

// ================================================================================
#define FNAME_LENGTH 256				// ファイル/ディレクトリ名
#define DATALEN 32						// mode, date, owner, group など
#define UNIQUE_LENGTH 32				// unique かどうか、最長連続 32 文字までカウント

#define ESCAPECHARACTER " ~#()\\$&"		// 表示時に \ でエスケープする文字、printUnique(), printLength: で共通

#define DELIMITER ":"					// -c の区切り、strtok() 
#define COLOR_TEXT 64					// -c の区切りの最大属性文字列数
#define ENVNAME "RLS_COLORS"			// 環境変数名

#define ListCountd FNAME_LENGTH			// info に属させる項目数、-f の最大文字数

#define SHOW_NONE   0					// struct FNAME の showlist、表示しない
#define SHOW_SHORT  1					// struct FNAME の showlist、printShort()
#define SHOW_LONG   2					// struct FNAME の showlist、printLong()

#define SEP ','							// makeSize() のセパレート文字

#define SKIP_LIST "()"					// uniqueCheck で SKIP 対象文字列


// 表示属性の切り替え
#define printEscapeColor(color) printf("%s", colorlist[color]);

// 変数名を文字列にする
#define toStr(n) #n

// 対象がディレクトリかどうか
#define IS_DIRECTORY(data) ((data).mode[0] == 'd' || ((data).linkname[0] != '\0' && (data).linkname[strlen((data).linkname) - 1] == '/'))

#define MAX(a, b) (((unsigned int)a) > ((unsigned int)b) ? ((unsigned int)a) : ((unsigned int)b))


// --------------------------------------------------------------------------------
#ifdef DEBUG
	#define debug
#else
	#define debug 1 ? (void) 0 :
#endif


#ifdef COUNTFUNC
	#include "countfunction.h"
	// scandir() 結果を malloc() に加算
	extern int count_malloc;
#endif


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

	#undef tolower
	#define tolower(i) map[(unsigned char)i]
#endif


// -P で使用、needle はすでに小文字化済み
#ifdef MYSTRCASESTR
char *
myStrcasestr(const char *haystack, const char *needle)
{
	for (; *haystack; haystack++) {
		if (tolower((unsigned char) *haystack) == (unsigned char) *needle) {
			const char *h, *n;

			for (h=haystack, n=needle; *h && *n; h++, n++) {
				if (tolower((unsigned char) *h) != (unsigned char) *n) {
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
	#define strcasestr myStrcasestr
#endif


// -r で使用
#ifdef MYROUND
	#define round(d) ((int) ((d + 0.05) * 10.0) / 10.0)
#else
	#include <math.h>
#endif


// ================================================================================
// 重複文字列リスト、、、unique 文字列をファイル名から検索しないように
struct DLIST {
	char dupword[UNIQUE_LENGTH + 1];	// 最大 32 文字
	struct DLIST *left;
	struct DLIST *right;
	int fnamelistNumber;				// unique な fnamelist の [] 番目、-1 なら複数存在 (unique でない)
	int length;
	int count;
};


// ================================================================================
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

	if (len > UNIQUE_LENGTH) {
		len = UNIQUE_LENGTH;
	}

	*new = (struct DLIST) {"", NULL, NULL, 0, len, 0};
// 	new->length = len;
// 	new->left = NULL;
// 	new->right = NULL;
// 	new->fnamelistNumber = 0;
// 	new->count = 0;

	if (len) {
		strncpy(new->dupword, word, len);
	}
	new->dupword[len] = '\0';

	return new;
}


// 必ず文字列を登録、多重で登録しないので、チェックは行わない
void
addDuplist(struct DLIST *p, char *word, int len, int number)
{
	struct DLIST *prev = NULL;
	char ret = 0;

	// 該当箇所まで移動
	while (p) {
		prev = p;			// 一つ上を覚えておく

		if (p->length == len) {
			ret = strncmp(word, p->dupword, len);
		} else {
			ret = (len < p->length) ? 1 : -1;
		}

		p = (ret < 0) ? p->left : p->right;
	}

	// 無かった方に新規登録
	struct DLIST *new_node = mallocDuplist(word, len);
	new_node->fnamelistNumber = number;
	new_node->count++;

	*(ret < 0 ? &prev->left : &prev->right) = new_node;
// 	if (ret < 0) {
// 		prev->left = new_node;
// 	} else {
// 		prev->right = new_node;
// 	}
}


// 検索履歴から、重複しているか確認する
int
searchDuplist(struct DLIST *p, char *word, int len, int number)
{
	while (p) {
		char ret;

		if (p->length == len) {
			ret = strncmp(word, p->dupword, len);

			// 見つかった
			if (ret == 0) {

#if 0
				printf("[");
				for (int i=0; i<len; i++) {
					printf("%c", word[i]);
				}
				printf(" number:%d, fnamelistNumber:%d ", number, p->fnamelistNumber);
				printf("]%d: dup\n", len);
#endif

				// 自分以外は重複と判断
				if (number != p->fnamelistNumber) {
// 				if (number - p->fnamelistNumber) {
					p->fnamelistNumber = -1;
					p->count++;
				}
				return 1;
			}
		} else {
			ret = (len < p->length) ? 1 : -1;
		}

		p = (ret < 0) ? p->left : p->right;
	}

	return 0;
}


// ================================================================================
// -c のカラー関連
typedef enum {
	base,
	normal,
	dir,
	fifo,
	socket,
	device,

	error,
	paint,

	label,
	reset,

	ListCount
} CLIST;

// カラーリスト
char colorlist[ListCount][COLOR_TEXT];


// --------------------------------------------------------------------------------
// ファイルの種類による色分け
char default_color_txt[ListCount * COLOR_TEXT];

void
printStr(CLIST color, const char *str)
{
	// 色付け後にリセット、、、背景 or 文字色だけの設定が引き継がれないように
	int len = strlen(str);
	printEscapeColor(color);
	if (len && str[len - 1] == '\n') {
		printf("%.*s", len - 1, str);
		printEscapeColor(reset);
		printf("\n");
	} else {
		printf("%s", str);
		printEscapeColor(reset);
	}
}


void
colorUsage(void)
{
	printStr(label, "Default Colors:\n");
	printf(" %s environment: same as -c option format, same restrictions. (set -x %s)\n", ENVNAME, ENVNAME);
	printf("  default setting: %s\n", default_color_txt);

	char *from;
	if ((from = getenv(ENVNAME)) != NULL) {
		printf("  %s env:  %s\n", ENVNAME, from);
	}

	printf("  setting color:   ");
	printStr(base,   "base");   printf(", ");
	printStr(normal, "normal"); printf(", ");
	printStr(dir,    "dir");    printf(", ");
	printStr(fifo,   "fifo");   printf(", ");
	printStr(socket, "socket"); printf(", ");
	printStr(device, "device"); printf(", ");
	printStr(error,  "error");  printf(", ");
	printStr(label,  "label");  printf(", ");
	printStr(paint,  "paint");
	printf("\n");
}


// 設定は、256 色 (5 で決め打ち、true color (2) の実装はしていない)
// !! argcolor が指定された時に内容が確認できない
void
initColor(int default_color, char *argcolor)
{
	debug printStr(label, "initColor:\n");

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
			debug printf(" env %s: empty.\n", ENVNAME);
			return;
		}
		debug printf("getenv(\"%s\"): %s\n", ENVNAME, from);
	} else {
		// 引数の色指定
		from = argcolor;
		debug printf(" argv: %s\n", from);
	}
	char *p;
	int usage = 0;

	int overwritelist[ListCount];
	memset(overwritelist, -1, sizeof(overwritelist));

	// 加工前の文字列
	char masterstr[strlen(from) + 1];
	strcpy(masterstr, from);
	from = masterstr;

	p = strtok(from, DELIMITER);
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
				char ctxt[COLOR_TEXT*2];

				// 指定された項目は、1 回目は上書き、それ以降は追記
				if (overwritelist[i] == -1) {
					overwritelist[i] = 0;
					colorlist[i][0] = '\0';
				}

				// 256 色: 3000 が fore、4000 が back
				if (atoi(valuechar) >= 3000) {
					if (colorlist[i][0] == '\0') {
						sprintf(ctxt, "\033[%c8;5;%sm", valuechar[0], valuechar + 1);
					} else {
						// 追記する記載
						sprintf(ctxt, ";%c8;5;%sm", valuechar[0], valuechar + 1);
						colorlist[i][strlen(colorlist[i]) - 1] = '\0';		// 最後の 'm' を削除
					}
				} else {
					// 8 色や、追記の属性
					if (colorlist[i][0] == '\0') {
						sprintf(ctxt, "\033[%sm", valuechar);
					} else {
						// 追記する記載
						sprintf(ctxt, ";%sm", valuechar);
						colorlist[i][strlen(colorlist[i]) - 1] = '\0';		// 最後の 'm' を削除
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
		int len = strlen(colorlist[i]);
		printf(" %6s: %d, len:%d, ", cname[i], overwritelist[i], len);

		// エスケープして表示
		for (int j=0; j<len; j++) {
			if (isprint(colorlist[i][j])) {
				printf("%c", colorlist[i][j]);
			} else {
				printf("\\033");
			}
		}
		printf("\n");
	}
#endif

	if (usage) {
		// 変な name
		printStr(label, "initColor:\n");
		printf("  item setting: %s\n", argcolor);
		printf("  bad setting:  %s\n", p);

		printf("\n");
		colorUsage();

		exit(EXIT_FAILURE);
	}

	debug printf(" ");
	debug colorUsage();
}


// ================================================================================
// 表示するファイルの情報
struct FNAME {
	struct stat sb;					// st_mode, st_mtime, st_size, st_uid, st_gid を使用

	char *info[ListCountd];			// この構造体の文字列要素の先頭
		char inode[DATALEN];				// inode
		char nlink[DATALEN];				// hard links
		char mode[DATALEN];					// mode bits
		char owner[DATALEN];				// owner
		char group[DATALEN];				// group
		char size[DATALEN];					// size の文字列
		char sizec[DATALEN];				// size の文字列、comma 表記
		char count[DATALEN];				// ディレクトリに含まれているファイル数と、size の混合
		char countc[DATALEN];				// ディレクトリに含まれているファイル数と、size の混合、comma 表記
		char date[DATALEN];					// mtime 日付
		char time[DATALEN];					// 日時
		char week[DATALEN];					// 曜日
		char path[FNAME_LENGTH];			// 絶対パスで指定されたパス名
		char unique[FNAME_LENGTH];			// ユニーク文字列
		char *name;							// 表示用ファイル名
		char kind[2];						// 種類
		char linkname[FNAME_LENGTH];		// link 名
		char errnostr[FNAME_LENGTH + 8];	// lstat() のエラー
		char extension[FNAME_LENGTH];		// 拡張子
#ifdef MD5
		char md5[33];						// 16 文字 * 2 バイト + '\0'
#endif

	// 各項目の長さ
	int inodel;
	int nlinkl;
	int model;
	int ownerl;
	int groupl;
	int sizel;
	int sizecl;
	int countl;
	int countcl;
	int datel;
	int timel;
	int weekl;
	int pathl;
	int uniquel;
	int extensionl;

	int print_length;				// 表示時のファイル名の長さ、全角考慮 wcStrlen()
	int length;						// ファイル名の長さ strlen()

	int kindl;
	int linknamel;
	int errnostrl;
#ifdef MD5
	int md5l;
#endif

	int date_f;						// makeDate() の difftime() が未来
	char *lowername;				// 比較用
	int uniquebegin;				// unique の開始
	int uniqueend;					// unique の終了、paint_string で該当した場合 1 を代入、aggregate でカウント
	CLIST color;					// 表示色

	int sourcelist;					// check の対象にするか/しないか
	int targetlist;					// uniqueCheck(), uniqueCheckFirstWord() の対象にするか/しないか
	int showlist;					// 表示するか/しないか
};


struct ALIST {
	int show_simple;
	int show_long;
	int format_list;

	// -f 関連は format_ に纏める
#ifdef MD5
	int format_md5;
#endif
	int format_unique;
	int format_extension;
	int format_owner;
	int format_group;
	int format_date;
	int format_size;
	int format_link;

	int do_uniquecheck;
	int deep_unique;
	int beginning_word;
	int do_emacs;
	int paint_string;
	int do_extension;

	int show_dotfile;
	int only_directory;
	int only_file;
	int only_paint_string;

	int no_color;
	int argv_color;
	int default_color;

	int mtime_sort;
	int size_sort;
	int name_sort;
	int no_sort;

	int readable_date;
	int readable_size;
	int readable_week;
	int aggregate_results;
	int aggregate_length;

	int show_help;
	int show_version;
	int show_escape;
	int output_escape;

	// text で囲む場合の文字列
	char textbegin[DATALEN];
	char textend[DATALEN];
	int lbegin;
	int lend;
	int tlen;

	char formatListString[ListCountd + 1];

	char color_txt[sizeof(default_color_txt)];
	char onlyPaintStr[FNAME_LENGTH + 1];

	int dirarg[256];

	unsigned short int termlen;
	unsigned short int termhei;
};


// --------------------------------------------------------------------------------
void
makeMode(struct FNAME *p, struct ALIST cfg)
{
	char c;
	mode_t st_mode = p->sb.st_mode;

	switch (st_mode & S_IFMT) {
		case S_IFDIR:  c = 'd'; p->color = dir;    p->kind[0] = '/'; break;	// dir
		case S_IFBLK:  c = 'b'; p->color = device;                   break;	// block device      /dev/
		case S_IFCHR:  c = 'c'; p->color = device;                   break;	// character device  /dev/
		case S_IFIFO:  c = '|'; p->color = fifo;   p->kind[0] = '|'; break;	// FIFO/pipe         /tmp/fish.ryoma/
		case S_IFSOCK: c = 's'; p->color = socket; p->kind[0] = '='; break;	// socket            /tmp/tmux-100/
		case S_IFLNK: {														// symlink
			struct stat sb;

			c = 'l'; p->kind[0] = '@';
			if (cfg.format_link) {
				// symlink 先のファイル名
				ssize_t rlen = readlink(p->name, p->linkname, sizeof(p->linkname) - 1);
				if (rlen == -1) {
					strcpy(p->errnostr, strerror(errno));
					p->color = error;
					p->linkname[0] = '\0';
					break;
				}
				p->linkname[rlen] = '\0';

				// link 先の sb を取得、link 先がディレクトリか
				if (lstat(p->linkname, &sb) == -1) {
					// データが取れなかったから異常
					strcpy(p->errnostr, strerror(errno));
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
		}
		break;

		default: c = '-'; break;
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
makeDate(struct FNAME *p, time_t lt, int readable_week)
{
	struct tm *t = localtime(&(p->sb.st_mtime));
	double dtime = difftime(lt, p->sb.st_mtime);

	// 未来
	if (dtime < 0) {
		p->date_f = 1;
	}

	// 現時刻から半年前かチェック (秒でチェック、60 * 60 * 24 * 365 / 2 = 15768000)、前なら年表示
	if (readable_week) {
		// 省略表示しない %B, %A
		strftime(p->date, DATALEN, (dtime < 15768000) ? "%B %e %H:%M" : "%B %e  %Y", t);
		strftime(p->week, DATALEN, "%A", t);
	} else {
		// 3 文字の省略表示 %b, %a
		strftime(p->date, DATALEN, (dtime < 15768000) ? "%b %e %H:%M" : "%b %e  %Y", t);
		strftime(p->week, DATALEN, "%a", t);
	}

	strftime(p->time, DATALEN, "%Y, %m/%d %H:%M:%S", t);
}


void
makeReadableDate(struct FNAME *p, time_t lt)
{
	double delta = difftime(lt, p->sb.st_mtime);
	// 未来、/proc, /sys
	if (p->date_f) {
		if (delta < 0) {
			delta *= -1.0;
		} else {
			p->date_f = 0;
		}
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

	if (p->date_f) {
		char *found = strchr(p->date, 'g');
		found[0] = 'f';		// after に変更
		found[1] = 't';
	}
}


// --------------------------------------------------------------------------------
void
makeSize(char *digits, long int num)
{
	if (num < 1000) {
		sprintf(digits, "%ld", num);
	} else {
		makeSize(digits, num / 1000);

		char tmp[DATALEN];
		sprintf(tmp, "%c%03ld", SEP, num % 1000);
		strcat(digits, tmp);
	}
}


// size, count, nlink で使用
void
makeReadableSize(char *numstr)
{
	// 1,234,567,890,123,456,789
	int len = strlen(numstr);

	if (len < 4) {			// 1K 以下
		return;
	}

	char unit;
	do {
		if (len > 24) { unit = 'E'; break; }
		if (len > 20) { unit = 'P'; break; }
		if (len > 16) { unit = 'T'; break; }
		if (len > 12) { unit = 'G'; break; }
		if (len >  8) { unit = 'M'; break; }
		unit = 'K';
	} while (0);

	numstr = strchr(numstr, SEP);
	numstr[0] = '.';		// ',' を '.' に置換
	numstr[2] = unit;		// 小数点以下 1 桁は表示
	numstr[3] = '\0';
}


// --------------------------------------------------------------------------------
// -fc のディレクトリ内のエントリー数
// 指定ディレクトリに含まれているエントリー数を返す、stat() をしないから、chdir() は不要
int
countEntry(char *dname, char *path)
{
// 	debug printStr(label, "countEntry:\n");

	struct dirent **namelist;
	char fullpath[FNAME_LENGTH];
	char *tmppath;

	if (dname[0] ==  '/') {
		tmppath = dname;
	} else {
		sprintf(fullpath, "%s%s/", path, dname);
		tmppath = fullpath;
	}

	int file_count = scandir(tmppath, &namelist, NULL, NULL);	// sort 不要

	if (file_count == -1) {
		debug printf(" scandir: %s: -1.\n", tmppath);
		return -1;
	}
#ifdef COUNTFUNC
	count_malloc += file_count;
#endif

	for (int i=0; i<file_count; i++) {
		free(namelist[i]);
	}
	free(namelist);

// 	debug printf(" %d, %s\n", file_count, tmppath);

	return file_count -2;			// "." と ".." を除く
}


// ================================================================================
#ifdef MD5
int
makeMD5(char *fname, char *md5)
{
	debug printStr(label, "md5: ");

	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (!mdctx) {
		strcpy(md5, "-");
		return -1;
	}

	if (EVP_DigestInit_ex(mdctx, EVP_md5(), NULL) != 1) {
		EVP_MD_CTX_free(mdctx);
		strcpy(md5, "-");
		return -1;
	}

	FILE *fp;
	fp = fopen(fname, "rb");
	if (fp == NULL) {
		strcpy(md5, "-");
		return -1;
	}

	// BUFSIZ より大きい 256k 単位で読み込めば遅くはない
	unsigned char buf[256000];
	size_t n;
	while ((n = fread(buf, sizeof(char), 256000, fp)) > 0) {
		if (EVP_DigestUpdate(mdctx, buf, n) != 1) {
			EVP_MD_CTX_free(mdctx);
			fclose(fp);
			strcpy(md5, "-");
			return -1;
		}
	}
	fclose(fp);

	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;

	if (EVP_DigestFinal_ex(mdctx, md_value, &md_len) != 1) {
		EVP_MD_CTX_free(mdctx);
		strcpy(md5, "-");
		return -1;
	}
	EVP_MD_CTX_free(mdctx);

	for (unsigned int i = 0; i < md_len; i++) {
		sprintf(&md5[i * 2], "%02x", md_value[i]);
	}
	md5[md_len * 2] = '\0';

	debug printf("%s: %s\n", md5, fname);

	return 0;
}
#endif


// ================================================================================
void (*printName)(struct FNAME, const char *, struct ALIST);
char paintString[FNAME_LENGTH];
int paintStringLen;


// -fn 表示、unique 文字列の色わけ表示
void
printUnique(struct FNAME p, const char *dummy, struct ALIST cfg)
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
	printf("%s", cfg.textbegin);
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
		if (strchr(ESCAPECHARACTER, p.name[i])) {
			printf("\\");
		}

		// 普通の文字表示
		printf("%c", p.name[i]);
	}
	printEscapeColor(reset);

	// --------------------------------------------------------------------------------
	printEscapeColor(base);
	printf("%s", cfg.textend);
	// あれば uniqueend 後を表示
	printf("%s", p.name + p.uniqueend + 1);
	printEscapeColor(reset);

	// dummy
	if (dummy == NULL) {
	}
}


// -p 時、対象文字列に何回該当したか、、、"aaaaaa" の場合 "aa" は 3 回該当の仕様
int
countMatchedString(const char *str)
{
	if (paintStringLen == 0) {
		return 0;
	}

	int length = strlen(str);
	// 比較用に小文字化
	char name[length + 1];
	for (int i=0; i<length; i++) {
		name[i] = tolower(str[i]);
	}
	name[length] = '\0';

	if (strstr(name, paintString) == NULL) {
		return 0;
	}

	int ret = 0;
// 	for (int i=0; i<length; i++) {
// 		if (strncmp(paintString, name + i, paintStringLen) == 0) {
// 			i += paintStringLen - 1;
// 			ret++;
// 		}
// 	}

	const char *pos = name;
	while ((pos = strstr(pos, paintString)) != NULL) {
		ret++;
		pos += (paintStringLen > 1) ? paintStringLen : 1; // 一致時のスキップ
	}

	return ret;
}


// -p 時の色分け文字列表示
void
printMatchedString(struct FNAME p, const char *str, struct ALIST cfg)
{
	// このファイル情報のどれにも該当していない pickupString() されていない
	if (p.uniqueend == -1 || paintStringLen == 0) {
		printStr(base, str);
		return;
	}

	int length = strlen(str);
	char name[length + 1];
	// 比較用に小文字化
	for (int i=0; i<length; i++) {
		name[i] = tolower(str[i]);
	}
	name[length] = '\0';

	if (strstr(name, paintString) == NULL) {
		printStr(base, str);
		return;
	}

	printEscapeColor(base);

	const char *ptr = name;
	const char *orig_ptr = str;
	char *match;

	while ((match = strstr(ptr, paintString)) != NULL) {
		// 該当前の部分
		printf("%.*s", (int)(match - ptr), orig_ptr);

		// 該当部分
		printf("%s", cfg.textbegin);
		printEscapeColor(paint);
		printf("%.*s", paintStringLen, orig_ptr + (match - ptr));			// マッチした文字列を、元のままで表示
		printEscapeColor(reset);
		printEscapeColor(base);
		printf("%s", cfg.textend);

		// 次の検索開始位置
		ptr = match + paintStringLen;
		orig_ptr = str + (ptr - name);
	}

	// 該当後の部分
	printf("%s", orig_ptr);

	printEscapeColor(reset);
}


// ファイルの種類の文字を表示
int
printKind(struct FNAME p, const char *str, struct ALIST cfg)
{
	if (str[0] == '\0') {
		return 0;
	}

	printMatchedString(p, str, cfg);
	return 1;
}


// ================================================================================
// ファイル単位で管理
void
addFNamelist(struct FNAME *p, char *name)
{
// 	p->size[0] = '\0';
// 	p->sizec[0] = '\0';
// 	p->count[0] = '\0';
// 	p->countc[0] = '\0';
	p->path[0] = '\0';
	p->unique[0] = '\0';
	p->name = name;		// namelist[i] をそのまま使用
	p->kind[0] = '\0'; p->kind[1] = '\0';
	p->linkname[0] = '\0';
	p->errnostr[0] = '\0';
	p->extension[0] = '\0';
	p->date_f = 0;

#ifdef MD5
	p->md5[0] = '\0';
#endif

	p->length = strlen(p->name);
// 	p->lowername = strdup(p->name);
	p->lowername = (char *) malloc(sizeof(char) * (p->length + 1));
	if (p->lowername == NULL) {
		perror("malloc");
		printf(" =>size:%zu\n", sizeof(char) * (p->length + 1));
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
		p->lowername[i] = tolower(p->name[i]);
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
}


// ================================================================================
// scandir() 時ソート
// ファイル名の長い順にする
int
compareNameLength(const struct dirent **s1, const struct dirent **s2)
{
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
compareNameAlphabet(const struct dirent **s1, const struct dirent **s2)
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
	return - myAlphaSort(a, b);
}


// size の小さい順
int
mySizeSort(const void *a, const void *b)
{
	struct FNAME *s1 = (struct FNAME *)a;
	struct FNAME *s2 = (struct FNAME *)b;

	if (s1->sb.st_size == s2->sb.st_size) {
		// 同じファイルサイズの時、ファイル名をアルファベット順でソート
// 		return strcmp(s1->lowername, s2->lowername);
		return strcasecmp(s1->name, s2->name);
	}

	return (s1->sb.st_size < s2->sb.st_size) ? -1 : 1;
}

// size の大きい順
int
mySizeSortRev(const void *a, const void *b)
{
	return - mySizeSort(a, b);
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
	return - myMtimeSort(a, b);
}


// --------------------------------------------------------------------------------
#ifdef DEBUG
void
debug_displayAllQsortdata(struct FNAME *data, int n, struct ALIST cfg)
{
	debug printStr(label, "displayAllQsortdata:\n");
	printf(" No:len:[unibegin,uniend] show: source: target: name/  lower:[name]\n");

	for (int i=0; i<n; i++) {
		printf(" ");
		printf("%2d:%2d:", i + 1, data[i].length);		// 数、長さ
		printf("[%2d,%2d] ", data[i].uniquebegin, data[i].uniqueend);
		printf("sh:%2d ", data[i].showlist);			// -1(error), 0(非表示), 1(short), 2(long)
		printf("sr:%2d ", data[i].sourcelist);
		printf("tr:%2d ", data[i].targetlist);
		printName(data[i], data[i].name, cfg);			// name
		if (strcmp(data[i].name, data[i].lowername)) {
			printf("  lower:[%s]", data[i].lowername);
		}
		printf("\n");
	}
}
#endif


// ================================================================================
// unique を全ファイルリストに反映する
void
refrectDuplist(struct DLIST *node, struct FNAME *p)
{
	if (node == NULL) {
		return;
	}

	// 左から順番に対応
	refrectDuplist(node->left, p);

	if (node->fnamelistNumber != -1 && node->length) {
		int j = node->fnamelistNumber;

		int obeg = p[j].uniquebegin;
		int oend = p[j].uniqueend;

		char *found = strstr(p[j].lowername, node->dupword);
// 		int nbeg = p[j].length - strlen(found);
		int nbeg = found - p[j].lowername;
		int nend = nbeg + node->length -1;

#if 0
		printf("<%s>[%s]", p[j].lowername, node->dupword);
		printf(", num:%d", j);
// 		printf(", O:[%d-%d=%d, %d], N:[%d-%d=%d, %d]", oend, obeg, oend - obeg, olen, nend, nbeg, nend - nbeg, nend - nbeg +1);
		printf(" [");
		for (int i=nbeg; i<=nend; i++) {
			printf("%c", p[j].lowername[i]);
		}
		printf("]");
#endif

		// 最初 unique の登録
		if (obeg == -1) {
			p[j].uniquebegin = nbeg;
			p[j].uniqueend = nend;
		} else {
			int nlen = nend - nbeg + 1;
			int olen = oend - obeg + 1;
			// 短い方
			if (olen > nlen) {
				p[j].uniquebegin = nbeg;
				p[j].uniqueend = nend;
			}

			if (olen == nlen) {
				// 同じ長さなら、先頭に近い方
				if (p[j].uniquebegin > nbeg) {
					p[j].uniquebegin = nbeg;
					p[j].uniqueend = nend;
				}
			}
		}

#if 0
		printf(",[");
		for (int i=p[j].uniquebegin; i<=p[j].uniqueend; i++) {
			printf("%c", p[j].lowername[i]);
		}
		printf("]\n");
#endif

	}

	refrectDuplist(node->right, p);
}


// uniqueCheck(), uniqueCheckFirstWord() の実行
void
runUniqueCheck(struct FNAME *fnamelist, int pnth, struct DLIST *duplist, void (*chkfunc)(struct FNAME *p, int j, int len, struct DLIST *duplist), int deep_unique)
{
	debug printStr(label, "runUniqueCheck:\n");

	for (int i=1; i<UNIQUE_LENGTH; i++) {
		int count_chklen[UNIQUE_LENGTH] = {0};

		debug printf(" uniqueCheck len:%d\n", i);
		for (int j=0; j<pnth; j++) {
			if (fnamelist[j].sourcelist == -1) {
				continue;
			}

			chkfunc(fnamelist, j, i, duplist);
		}

		// 最後までチェックする
		if (deep_unique) {
			continue;
		}

		// --------------------------------------------------------------------------------
		// 途中で終了するか、判断
		refrectDuplist(duplist, fnamelist);

		int sum = 0;
		// unique 該当を数える
		for (int j=0; j<pnth; j++) {
			if (fnamelist[j].showlist == SHOW_NONE) {
				continue;
			}
			if (fnamelist[j].uniqueend != -1) {
				int len = fnamelist[j].uniqueend - fnamelist[j].uniquebegin + 1;
				count_chklen[len]++;

				// refrectDuplist() を白紙に戻す
				fnamelist[j].uniquebegin = -1;
				fnamelist[j].uniqueend = -1;

				sum++;
			}
		}

		// --------------------------------------------------------------------------------
		// unique 数が前回の半分以下、0 になった、これ以上続けても非効率、、、ただし、最低 3 文字までは行う
		if ((i > 2) && (count_chklen[i - 1] / 2 > count_chklen[i] || count_chklen[i] == 0)) {
			debug printf(" done, i:%d\n", i);
			break;
		}

		// unique 数が 9 割りを超えていたらやめる
		if (sum > pnth * 0.9) {
			debug printf(" done %d/%d.\n", sum, pnth);
			break;
		}
	}
}


void
uniqueCheck(struct FNAME *p, int j, int len, struct DLIST *duplist)
{
	int l = p[j].length - len;

	if (l == 0) {
// 		searchDuplist(duplist, p[j].lowername, len, -1);
		if (searchDuplist(duplist, p[j].lowername, len, -1) == 0) {
			addDuplist(duplist, p[j].lowername, len, -1);
		}
		return;
	}

	// 1 文字目から、len 文字ずつ最後まで繰り返す
	for (int i=0; i<=l; i++) {
		char *tmp = p[j].lowername + i;
		int brk = 0;

		for (int k=0; k<len; k++) {
			// 漢字が含まれている || '()' だとエスケープできないから飛ばす、tolower() 後の文字列で
			if (isprint((int) tmp[k]) == 0 || strchr(SKIP_LIST, tmp[k])) {
				brk = 1;
				break;
			}
		}
		if (brk) {
			addDuplist(duplist, tmp, len, -1);
			continue;
		}

		if (searchDuplist(duplist, tmp, len, j) == 0) {
			addDuplist(duplist, tmp, len, j);

// #ifdef DEBUG
// 			printf("[");
// 			for (int i=0; i<len; i++) {
// 				printf("%c", tmp[i]);
// 			}
// 			printf("], %d\n", len);
// #endif
		}
	}
}


void
uniqueCheckFirstWord(struct FNAME *p, int j, int len, struct DLIST *duplist)
{
	char *tmp = p[j].lowername;

	if (p[j].length == len) {
// 		searchDuplist(duplist, p[j].lowername, len, -1);
		if (searchDuplist(duplist, p[j].lowername, len, -1) == 0) {
			addDuplist(duplist, p[j].lowername, len, -1);
		}
		return;
	}

	for (int k=0; k<len; k++) {
		// 漢字が含まれている || '()' だとエスケープできないから飛ばす、tolower() 後の文字列で
		if (isprint((int) tmp[k]) == 0 || strchr(SKIP_LIST, tmp[k])) {
			addDuplist(duplist, tmp, len, -1);
			return;
		}
	}

	if (searchDuplist(duplist, tmp, len, j) == 0) {
		addDuplist(duplist, tmp, len, j);

// #ifdef DEBUG
// 		printf("[");
// 		for (int i=0; i<len; i++) {
// 			printf("%c", tmp[i]);
// 		}
// 		printf("], %d\n", len);
// #endif
	}
}


// --------------------------------------------------------------------------------
void
uniqueCheckEmacs(struct FNAME *p, int j, int len, struct DLIST *duplist)
{
	// uniqueCheckEmacs 特有 ----------------------------------------
	char ename[FNAME_LENGTH];
	strcpy(ename, p[j].lowername);

	char *extension = strchr(ename, '.');
	// 拡張子が 1 つだけなら、削除する必要は無い
	if (extension) {
		extension++;
		// 1 つ以上見つかったので、拡張子を切断
		if (searchDuplist(duplist, extension, strlen(extension), j)) {
			if (extension) {
				ename[strlen(ename) - strlen(extension)] = '\0';
			}
		}
	}
	// --------------------------------------------------------------------------------

	int l = strlen(ename) - len;

	if (l == 0) {
// 		searchDuplist(duplist, p[j].lowername, len, -1);
		if (searchDuplist(duplist, ename, len, -1) == 0) {
			addDuplist(duplist, ename, len, -1);
		}
		return;
	}

	// 1 文字目から、len 文字ずつ最後まで繰り返す
	for (int i=0; i<=l; i++) {
		char *tmp = ename + i;
		int brk = 0;

		for (int k=0; k<len; k++) {
			// 漢字が含まれている || '()' だとエスケープできないから飛ばす、tolower() 後の文字列で
			if (isprint((int) tmp[k]) == 0 || strchr(SKIP_LIST, tmp[k])) {
				brk = 1;
				break;
			}
		}
		if (brk) {
			addDuplist(duplist, tmp, len, -1);
			continue;
		}

		if (searchDuplist(duplist, tmp, len, j) == 0) {
			addDuplist(duplist, tmp, len, j);

// #ifdef DEBUG
// 			printf("[");
// 			for (int i=0; i<len; i++) {
// 				printf("%c", tmp[i]);
// 			}
// 			printf("], %d\n", len);
// #endif
		}
	}
}


// do_emacs で使用
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
wcStrlen(struct FNAME p)
{
	// ワイド文字列に変換
	wchar_t wstr[FNAME_LENGTH + 1];
	size_t wlen = mbstowcs(wstr, p.name, sizeof(wstr)/sizeof(wchar_t) - 1);
	if (wlen == (size_t) -1) {
		// 変換失敗時はバイト長を返す
		return strlen(p.name);
	}
	wstr[wlen] = L'\0';

	// 各文字の表示幅を計算
	int total = 0;
	for (size_t i=0; i<wlen; i++) {
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
		  case 'i': case 'I': if (func(p.inode,  string)) { return 1; } break;
		  case 'h': case 'H': if (func(p.nlink,  string)) { return 1; } break;
		  case 'm': case 'M': if (func(p.mode,   string)) { return 1; } break;
		  case 'o': case 'O': if (func(p.owner,  string)) { return 1; } break;
		  case 'g': case 'G': if (func(p.group,  string)) { return 1; } break;
		  case 'S':           if (func(p.size,   string)) { return 1; } break;
		  case 's':           if (func(p.sizec,  string)) { return 1; } break;
		  case 'C':           if (func(p.count,  string)) { return 1; } break;
		  case 'c':           if (func(p.countc, string)) { return 1; } break;
		  case 'd': case 'D': if (func(p.date,   string)) { return 1; } break;
		  case 't': case 'T': if (func(p.time,   string)) { return 1; } break;
		  case 'w': case 'W': if (func(p.week,   string)) { return 1; } break;
		  case 'p': case 'P': if (func(p.path,   string)) { return 1; } break;
		  case 'u': case 'U': if (func(p.unique, string)) { return 1; } break;
		  case 'n': case 'N': if (func(p.name,   string)) { return 1; } break;
		  case 'k': case 'K': if (func(p.kind,   string)) { return 1; } break;
		  case 'x': case 'X': if (func(p.extension,      string)) { return 1; } break;
		  case 'l': case 'L': if (strcasestr(p.linkname, string)) { return 1; } break;
		  case 'e': case 'E': if (strcasestr(p.errnostr, string)) { return 1; } break;
#ifdef MD5
		  case '5':           if (func(p.md5,  string)) { return 1; } break;
#endif
		}
	}

	return 0;
}


// ================================================================================
// リダイレクトでもレイアウトを維持するようにスペースを使用
void
printShort(struct FNAME *data, int n, struct ALIST cfg)
{
	debug printStr(label, "printShort:\n");

	if (cfg.show_long) {
		return;
	}

	int printshort_count = 0;
	int attempts = 0;

	int sep = 2;		// ファイル名間の空白 + printKind() で 2 文字
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

// 	char buffer[65536];
// 	setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

#ifdef DEBUG
	printf("No:len:[unibegin,uniend] name/  lower:[name]\n");

	for (int i=0; i<n; i++) {
		if (data[i].showlist != SHOW_SHORT) {
			printf("%2d:%2d( -):", i + 1, data[i].length);							// 数
		} else {
			printf("%2d:%2d(%2d):", i + 1, data[i].length, data[i].print_length);	// 数、長さ
		}
		printf("[%2d,%2d] ", data[i].uniquebegin, data[i].uniqueend);
		printName(data[i], data[i].name, cfg);										// name
		printKind(data[i], data[i].kind, cfg);
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
		int row = nth/col + 1;

		// row の数が、termlen/3 以下になるように
		if (row > cfg.termlen/3) {
			continue;
		}

		// 2 次元配列を確保
		int rowcolumnlist[row][col];
		memset(rowcolumnlist, -1, sizeof(rowcolumnlist));

		// 1 行のレイアウト + 番兵
		int columnlist[row + 1];
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
					if (maxlength >= cfg.termlen) {
						break;
					}
				}

				rowcolumnlist[i][j] = count;
				count++;

				debug attempts++;
			}
			// はみ出ている
			if (maxlength >= cfg.termlen) {
				break;
			}
		}

		// --------------------------------------------------------------------------------
		// termlen に収まったか
		if (maxlength >= cfg.termlen) {
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
		printf("termlen:%d, maxlength:%d\n", cfg.termlen, maxlength);
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
				printName(data[index], data[index].name, cfg);
				int ret = printKind(data[index], data[index].kind, cfg);
				// 右が -1 なら出力しない
				if (j < row - 1 && rowcolumnlist[j + 1][i] != -1) {
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

		// + ガイダンス行表示 (DEBUG 時は必ず表示)
#ifdef DEBUG
		repeat = cfg.termhei + 1;
#endif
		// スクロールするなら + ガイダンス行表示
		if (repeat > cfg.termhei) {
			for (int j=0; columnlist[j] != -1; j++) {
				printStr(base, "+");
				if (columnlist[j + 1] != -1) {
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
		printName(data[i], data[i].name, cfg);
		printKind(data[i], data[i].kind, cfg);
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
// 	debug printStr(label, "haveAfterdata:\n");
// 	debug printf("p->info:%s, orderlist:[%s], i:%d\n", p->info[i+1], orderlist, i);

	if (orderlist[i + 1] == '\0') {
		return 0;
	}

	// 最後まで確認する
	for (int j=i + 1; orderlist[j] != '\0'; j++) {
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
printLong(struct FNAME *data, int n, struct ALIST cfg, int digits[])
{
	debug printStr(label, "printLong:\n");

	if (cfg.show_long == 0) {
		return;
	}

	int printlong_count = 0;

// 	char buffer[65536];
// 	setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

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
		// formatListString[k] 番目と、&fnamelist[j].xxx でデータが続くか確認して、haveAfterdataStr[k] に 0, 1 を入れる
		char haveAfterdataStr[ListCountd + 1] = "";

// 		printf("%s: ", cfg.formatListString);
		int flen = strlen(cfg.formatListString);
		for (int j=0; j<flen; j++) {
			haveAfterdataStr[j] = haveAfterdata(&data[i], cfg.formatListString, j);

			// 一度 0 だった場合、以降はずっと 0
			if (haveAfterdataStr[j] == 0) {
// 				printf("%2d:(%c) ", j, cfg.formatListString[j]);
				for (; cfg.formatListString[j] != '\0'; j++) {
					haveAfterdataStr[j] = 0;
				}
			}
		}

		// --------------------------------------------------------------------------------
		for (int j=0; cfg.formatListString[j] != '\0'; j++) {
			int count = 0;
			int len = 0;

			count = countMatchedString(data[i].info[j]);

			// --------------------------------------------------------------------------------
			// 左寄せ項目
#ifdef MD5
			if (strchr("mMoOkKgGtTwWIHDuUxX5", (unsigned char) cfg.formatListString[j])) {
#else
			if (strchr("mMoOkKgGtTwWIHDuUxX", (unsigned char) cfg.formatListString[j])) {
#endif
				switch ((unsigned char) cfg.formatListString[j]) {
				  case 'm': case 'M': len = data[i].model   + count * cfg.tlen; break;
				  case 'o': case 'O': len = data[i].ownerl  + count * cfg.tlen; break;
				  case 'k': case 'K': len = data[i].kindl   + count * cfg.tlen; break;
				  case 'g': case 'G': len = data[i].groupl  + count * cfg.tlen; break;
				  case 't': case 'T': len = data[i].timel   + count * cfg.tlen; break;
				  case 'w': case 'W': len = data[i].weekl   + count * cfg.tlen; break;
				  case 'I':           len = data[i].inodel  + count * cfg.tlen; break;
				  case 'H':           len = data[i].nlinkl  + count * cfg.tlen; break;
				  case 'D':           len = data[i].datel   + count * cfg.tlen; break;
				  case 'u': case 'U': len = data[i].uniquel + count * cfg.tlen; break;
				  case 'x': case 'X': len = data[i].extensionl + count * cfg.tlen; break;
#ifdef MD5
				  case '5':           len = data[i].md5l    + count * cfg.tlen; break;
#endif
				}

				debug printf("%c:", cfg.formatListString[j]);
				printMatchedString(data[i], data[i].info[j], cfg);
				if (haveAfterdataStr[j] == 0) {
					continue;
				}
				if (digits[(unsigned char) cfg.formatListString[j]]) {
					printf("%*s", digits[(unsigned char) cfg.formatListString[j]] - len, "");
				}
				printf(" ");

				continue;
			}

			// --------------------------------------------------------------------------------
			// 右寄せ
			if (strchr("isSh", (unsigned char) cfg.formatListString[j])) {
				switch (cfg.formatListString[j]) {
					case 'i': len = data[i].inodel + count * cfg.tlen; break;
					case 'h': len = data[i].nlinkl + count * cfg.tlen; break;
					case 's': len = data[i].sizecl + count * cfg.tlen; break;
					case 'S': len = data[i].sizel  + count * cfg.tlen; break;
				}

				printf("%*s", digits[(unsigned char) cfg.formatListString[j]] - len, "");
				debug printf("%c:", cfg.formatListString[j]);
				printMatchedString(data[i], data[i].info[j], cfg);
				if (haveAfterdataStr[j]) {
					printf(" ");
				}

				continue;
			}

			// --------------------------------------------------------------------------------
			// 特殊項目
			switch (cfg.formatListString[j]) {
			  case 'd':
				len = data[i].datel + count * cfg.tlen;
				// -t の時
				if (digits[(unsigned char) cfg.formatListString[j]]) {
					printf("%*s", digits[(unsigned char) cfg.formatListString[j]] - len, "");
				}
				debug printf("%c:", cfg.formatListString[j]);
				// 未来の場合、色を変える
				if (data[i].date_f) {
					printStr(paint, data[i].info[j]);
				} else {
					printMatchedString(data[i], data[i].info[j], cfg);
				}
				if (haveAfterdataStr[j]) {
					printf(" ");
				}
				break;

			  case 'c': case 'C':
				len = ((cfg.formatListString[j] == 'c') ? data[i].countcl : data[i].countl) + count * cfg.tlen;
				// 数値なので右寄せ表示
				if (digits[(unsigned char) cfg.formatListString[j]]) {
					printf("%*s", digits[(unsigned char) cfg.formatListString[j]] - len, "");
				}
				// エントリー数は dir 色で表示するが、-n の時は表見できない
				if (data[i].color == dir && paintStringLen == 0) {
					// -n の時は表現できない
					debug printf("%c:", cfg.formatListString[j]);
					printStr(dir, data[i].info[j]);
				} else {
					debug printf("s:");
					printMatchedString(data[i], data[i].info[j], cfg);			// size
				}
				if (haveAfterdataStr[j]) {
					printf(" ");
				}
				break;

				// --------------------------------------------------------------------------------
			  case '[':
			  case ']':
			  case '|':
			  case ',':
				printf("%s", data[i].info[j]);
				if (haveAfterdataStr[j]) {
					printf(" ");
				}
				break;

				// --------------------------------------------------------------------------------
			  case 'p': case 'P':
				if (data[i].info[j][0] == '\0') {
					break;
				}
				len = data[i].pathl + count * cfg.tlen;
				debug printf("%c:", cfg.formatListString[j]);
				printMatchedString(data[i], data[i].info[j], cfg);
				if (haveAfterdataStr[j]) {
					if (digits[(unsigned char) cfg.formatListString[j]]) {
						printf("%*s", digits[(unsigned char) cfg.formatListString[j]] - len, "");
					}
					// 特殊対応
					if (!(cfg.formatListString[j] == 'P' && (cfg.formatListString[j + 1] == 'n' || cfg.formatListString[j + 1] == 'N'))) {
						printf(" ");
					}
				}
				break;

			  case 'n': case 'N':
				len = data[i].print_length + count * cfg.tlen;
				debug printf("%c:", cfg.formatListString[j]);
				printName(data[i], data[i].info[j], cfg);					// name (printUnique(), printMatchedString() の切替)
				if (haveAfterdataStr[j]) {
					if (digits[(unsigned char) cfg.formatListString[j]]) {
						printf("%*s", digits[(unsigned char) cfg.formatListString[j]] - len, "");
					}
					// 特殊対応
					if (cfg.formatListString[j + 1] != 'k' && cfg.formatListString[j + 1] != 'K') {
						printf(" ");
					}
				}
				break;

			  case 'l': case 'L':
				len = data[i].linknamel + count * cfg.tlen;
				// symlink 先を表示
				if (data[i].mode[0] == 'l') {
					printStr(data[i].color, "->");							// link 先と同じ色
					printf(" ");
					debug printf("%c:", cfg.formatListString[j]);
					printMatchedString(data[i], data[i].info[j], cfg);
				}
				// -> の分を表示する
				if (len == 0) {
					if (cfg.formatListString[j] == 'l') {
						printf("   ");
					}
				}
				if (haveAfterdataStr[j]) {
					if (digits[(unsigned char) cfg.formatListString[j]]) {
						printf("%*s", digits[(unsigned char) cfg.formatListString[j]] - len, "");
					}
					printf(" ");
				}
				break;

			  case 'e': case 'E':
				len = data[i].errnostrl + count * cfg.tlen;
				// エラー表示
				if (data[i].errnostr[0] != '\0') {
					debug printf("%c:", cfg.formatListString[j]);
					printStr(data[i].color, "[");
					printMatchedString(data[i], data[i].info[j], cfg);
					printStr(data[i].color, "]");
				} else {
					// [] の分を表示する
					if (cfg.formatListString[j] == 'e') {
						printf("  ");
					}
				}
				if (haveAfterdataStr[j]) {
					if (digits[(unsigned char) cfg.formatListString[j]]) {
						printf("%*s", digits[(unsigned char) cfg.formatListString[j]] - len, "");
					}
					printf(" ");
				}
				break;

			  default:
				// -fz などの指定文字列に無いものを指摘、指定文字列 z が最大 n 行分表示される
				printEscapeColor(error);
				printf("%c ", cfg.formatListString[j]);
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
void
displayDuplist(struct DLIST *node)
{
	if (node == NULL) {
		return;
	}

	// 左から順番に表示
	displayDuplist(node->left);
	if (node->count) {
		printf("[%s:%d] ", node->dupword, node->count);
// 		printf("dup: [%s]:%d, %d\n", node->dupword, node->fnamelistNumber, node->count);
	}
	displayDuplist(node->right);
}


void
printExtension(struct FNAME *fnamelist, int nth)
{
	debug printStr(label, "printExtension:\n");

	struct DLIST *extensionduplist;
	extensionduplist = mallocDuplist("", 0);

	for (int i=0; i<nth; i++) {
		if (fnamelist[i].showlist == SHOW_NONE) {
			continue;
		}

		char *extension = fnamelist[i].extension;
		int len = strlen(extension);
		// !! if 文が無ければ、拡張子無しをカウント
// 		if (len) {
			// . が 1 文字目だったら拡張子と判断しない
			if (len == fnamelist[i].length - 1) {
				continue;
			}

			if (searchDuplist(extensionduplist, extension, len, i) == 0) {
				addDuplist(extensionduplist, extension, len, i);
			}
// 		}
	}

	debug printf(" ");

	printStr(label, "extension: ");
	displayDuplist(extensionduplist);
	printf("\n");

	freeDuplist(extensionduplist);
}


// ================================================================================
// どの程度の文字列の長さで unique になっているか表示
// -R や、-p の結果も paint で表示する
void
printAggregate(struct FNAME *fnamelist, int nth, int aggregate_length)
{
	debug printStr(label, "printAggregate:\n");

	int hitcount = 0;
	int displaycount = 0;
	int count_chklen[UNIQUE_LENGTH] = {0};

	// 表示しないものを nth から引く
	for (int i=0; i<nth; i++) {
		if (fnamelist[i].showlist == SHOW_NONE) {
			continue;
		}
		displaycount++;

		// paint_string は -1 -> 1 を設定
		if (fnamelist[i].uniqueend != -1) {
			if (fnamelist[i].uniquebegin != -1) {
				int len = fnamelist[i].uniqueend - fnamelist[i].uniquebegin + 1;
				count_chklen[len]++;
			} else {
				// paintString を格納すると長さがわかる
				count_chklen[paintStringLen]++;
			}
			hitcount++;
		}
	}

	// 少しだけ計算結果が違う、個別の四捨五入の方が実測値に近い
// 	double percent = hitcount * 100.0 / nth;
	double percent = 0.0;
	for (int i=1; i<UNIQUE_LENGTH; i++) {
		percent += round(count_chklen[i] * 100.0 / nth);
	}

	// 表示
	debug printf(" ");
	printStr(label, "aggregate:");

	// -P の時、表示数が変化するから
	if (displaycount != nth) {
		printf(" display:%d/%d(%.1f%%)", hitcount, displaycount, displaycount ? round(hitcount * 100.0 / displaycount): 0);
	}
	// 全体のデータ表示
	printf(" all:%d/%d(%.1f%%) lines ", hitcount, nth, percent);
	for (int i=1; i<UNIQUE_LENGTH; i++) {
		if (count_chklen[i] != 0) {
			if (i == aggregate_length) { printEscapeColor(paint); }			// -R, -p 指定時に色付け
			printf("[%d:%d(%.1f%%)]", i, count_chklen[i], round(count_chklen[i] * 100.0 / nth));
			if (i == aggregate_length) { printEscapeColor(reset); }
			printf(" ");
		}
	}
	printf("\n");
}


// ================================================================================
// 256 color、-256 |less すると、表示できる色が変わる、/bin/ls --color |less と比べる
void
showEscapeList(void)
{
	printStr(label, "Show Escape List:\n");

	printf(" 8 color:\n");
	printf(" standard colors/high-intensity colors\n");

	for (int i=30; i<38; i++) {
		printf("\033[%dm", i);
		printf("%5d", i);
		printf("\033[0m");

		printf("\033[%dm", i + 10);
		printf("%5d", i + 10);
		printf("\033[0m");
	}
	printf("\n");

	for (int i=90; i<98; i++) {
		printf("\033[%dm", i);
		printf("%5d", i);
		printf("\033[0m");

		printf("\033[%dm", i + 10);
		printf("%5d", i + 10);
		printf("\033[0m");
	}
	printf("\n\n");

	// --------------------------------------------------------------------------------
	int j;
	printf(" 256 color:\n");
	j=1;
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
	printf(" grayscale colors:\n");
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

	printf("\n");
	colorUsage();
}


void
showVersion(char **argv)
{
	printf("%s:\n", argv[0]);
	printf(" Displays a list of directories and files.\n");
	printf(" Displays Unique Parts of directory and file names in Color for shell filename completion.\n");
	printf("  default unique word:    for fish shell.\n");
	printf("  beginning of file name: like tcsh shell. (-b)\n");
	printf("  unique group name word: elisp like file name, same name and differet extension. (-e)\n");
	printf("  without color:          enclosing string and no color. (-TB\\[ -TE\\] -n)\n");
	printf(" Displays only files for which the specified string matches. (-P)\n");

	printf("\n");
	printf(" Change Layout of display list.\n");
	printf("  Long listing format:  change format orders width -f.\n");
	printf("  Short lisring format: layout is preserved on redirect. (similar to result layout on redirect: -fNk)\n");


	// --------------------------------------------------------------------------------
#ifndef VERSION
	#define VERSION "Local Build"
#endif
#ifndef RELTYPE
	#define RELTYPE "[SNAPSHOT]"
#endif

#ifdef VERSION
	printf("\n");
	printf(" Version: %s %s", VERSION, RELTYPE);

	#ifdef MD5
		printStr(label, " [MD5]");
	#endif
	#ifdef DEBUG
		printStr(label, " [DEBUG]");
	#endif
	#ifdef COUNTFUNC
		printStr(label, " [COUNT]");
	#endif
	#ifdef PROFILE
		printStr(label, " [PROFILE]");
	#endif
	#ifdef OMP
		printStr(label, " [OpenMP]");
	#endif

	printf("\n");
#endif

	// --------------------------------------------------------------------------------
	printf(" Build:   ");
#ifdef INCDATE
	printf("%s, %s %s [%s]\n", BYEAR, BDATE, BTIME, __FILE__);
#else
	printf("%s, %s\n", __DATE__, __TIME__);
#endif
}


// --------------------------------------------------------------------------------
void
showUsage(char **argv)
{
	printStr(label, "Usage:\n");
	printf(" %s [OPTION]... [DIR/|FILE]...\n", argv[0]);
	printf("\n");
	printf(" List of directories and files. If nothing is listed, the current directory is used.\n");
	printf("  Long listing format:  /bin is FILE, /bin/ is DIR.\n");
	printf("  Short listing format: /bin is DIR.\n");

	printf("\n");
	printf(" Options may be specified individually, as in -l -a -u, or collectively, as in -alu. (In no particular order)\n");
	printf(" Color "); printStr(normal, "-xxx"); printf(" option is specified separately from the other options.\n");

	printf("\n");
	printf(" If multiple options are specified:\n");
	printf("  The last option is overwritten: -c, -p, -P, -f, -TB, -TE, -R.\n");
	printf("  All options are merged:         -m, -z, -N.\n");

	printf("\n");
	printf(" Options have priority. (Last line of each "); printStr(label, "options:"); printf(")\n");
	printf("  If a higher priority option is set, lower priority options are ignored.\n");

	printf("\n");
	printStr(label, "Listing format options:\n");
	printf(" -s: Short listing format. (no kind, one color)\n");
	printf(" -l: Long listing format.\n");
	printf(" "); printStr(normal, "-f"); printf(": with -l, change Format orders. (default: -fmogcdPNkLE)\n");
	printf("      m:mode, o:owner, g:group, c:count, d:date, p:path, n:name, k:kind, l:linkname, e:errno,\n");
	printf("      i:inode, h:hardlinks, s:size, t:time, w:week, u:uniqueword, x:extension.\n");
#ifdef MD5
	printf("      5:MD5 message digest.\n");
#endif
	printf("      [, ], |, ',':specified character is displayed.\n");
	printf("      ---\n");
	printf("      d:\"%%b %%e %%H:%%M\" or \"%%b %%e  %%Y\" format,\n");
	printf("      p:with -l, argv is \"PATH/FILE\" format,\n");
	printf("      s:size of DIRECTORY and FILE,\n");
	printf("      c:if FILE, the size of FILE. Otherwise, number of directory entries. (without \".\" and \"..\")\n");
	printf("      x:word after the last dot, dot is not the beginning character of the filename.\n");
	printf("      S, C:no comma output.\n");
	printf("      Upper case is padding off. (Same length: no change in appearance)\n");
	printf("     -s > -l = -f > default short listing (include file status)\n");

	printf("\n");
	printStr(label, "Coloring algorithm options:\n");
	printf(" -u: deep Unique word check. (default check -> deep check)\n");
	printf(" -b: Beginning of file name. (default check -> beginning check)\n");
	printf(" "); printStr(normal, "-p"); printf(": Paint the matched string. (-pstring, case insensitive)\n");
	printf(" -e: paint Elisp like file unique group name word check.\n");
	printf("     -p > -u > -b = -e > default unique word check algorithm\n");

	printf("\n");
	printStr(label, "Output data options:\n");
	printf(" -a: show All dot files.\n");
	printf(" -o: with -a, show Only directories. (-s > -o > [FILE])\n");
	printf(" -O: show Only files.\n");
	printf(" "); printStr(normal, "-P"); printf(": like -p, Pickup only the string matched. (-Pstring, case sensitive)\n");
	printf("     -P = -O > -o > -a\n");

	printf("\n");
	printStr(label, "Color options:\n");
	printf(" -n: No colors.\n");
	printf(" -d: use Default colors.\n");
	printf(" "); printStr(normal, "-c"); printf(": set Custom colors. (8: -cbase=37:normal=34:normal=1:..., 256: -cbase=3007:normal=3012:normal=1:...)\n");
	printf("      "); printStr(base,   "base");   printf( ":   not unique string.\n");
	printf("      "); printStr(normal, "normal"); printf(   ": normal files.\n");
	printf("      "); printStr(dir,    "dir");    printf(":    directories.\n");
	printf("      "); printStr(fifo,   "fifo");   printf( ":   FIFO/pipe.\n");
	printf("      "); printStr(socket, "socket"); printf(   ": socket.\n");
	printf("      "); printStr(device, "device"); printf(   ": block/character device.\n");
	printf("      "); printStr(label,  "label");  printf(  ":  label string.\n");
	printf("      "); printStr(error,  "error");  printf(  ":  error string.\n");
	printf("      "); printStr(paint,  "paint");  printf(  ":  matched string.\n");
	printf("      ---\n");
	printf("      8 colors:   Control Sequence Introducer. (Depends on terminal implementation)\n");
	printf("      256 colors: fore:30xx, back:40xx.\n");
	printf("      8 colors and 256 colors cannot be specified at the same time. (-256)\n");
	printf("      %s environment: same as -c option format, same restrictions.\n", ENVNAME);
	printf("     -n > -c > -d > %s env color > default color\n", ENVNAME);

	printf("\n");
	printStr(label, "Sort options:\n");
	printf(" -m: Mtime sort. (alphabet sort -> mtime sort, -mm: reverse mtime sort)\n");
	printf(" -z: siZe sort. (alphabet sort -> size sort, -zz: reverse size sort, -fc is calculated as -fs)\n");
	printf(" -N: reverse alphabet (Name) sort. (alphabet sort -> reverse alphabet sort, -NN: alphabet sort)\n");
	printf(" -S: no Sort order.\n");
	printf("     -S > -N(N) > -z(z) > -m(m)\n");

	printf("\n");
	printStr(label, "Additional options:\n");
	printf(" -t: with -l, human-readable daTe. (-f with date)\n");
	printf(" -i: with -l, human-readable sIze. (-f with count, size, hardlinks)\n");
	printf(" -w: with -l, day of the week, month Without abbreviation. (-f with week, date (month))\n");
	printf(" "); printStr(label, "-X"); printf(": show extension Results.\n");
	printf(" "); printStr(label, "-r"); printf(": show aggregate Results.\n");
	printf(" "); printStr(normal, "-R"); printf(": color the corresponding length of the aggregate Results with the \"paint\" color. (-Rnumber)\n");
	printf("     -r = -R = -E = -i = -t > -w\n");

	printf("\n");
	printStr(label, "Other options:\n");
	printf(" -h, "); printStr(normal, "--help"); printf(":    show this message.\n");
	printf(" -v, "); printStr(normal, "--version"); printf(": show Version.\n");
	printf(" "); printStr(normal,"-TB"); printf(", "); printStr(normal, "-TE"); printf(":      enclosing unique word string. (-TB\\[ -TE\\])\n");
	printf(" "); printStr(normal,"-always"); printf(":       output the escape sequence characters in redirect.\n");
	printf(" "); printStr(normal,"-256"); printf(":          color text output.\n");
}


// ================================================================================
// terminal のサイズを取得
int
getTerminalSize(unsigned short int *x, unsigned short int *y)
{
	debug printStr(label, "getTerminalSize:\n");

	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
// 		debug printf(" ERROR: width:%d, height:%d\n", ws.ws_col, ws.ws_row);
// 		perror("ioctl");

		// 取得できないので 120x35 とする
		*x = 120;
		*y = 35;
		debug printf(" Assumption: width:%d, height:%d.\n", *x, *y);
		return -1;
	}

	debug printf(" width:%d, height:%d\n", ws.ws_col, ws.ws_row);
	*x = ws.ws_col;
	*y = ws.ws_row;

	return 0;
}


// ================================================================================
// 複数のディレクトリ/ファイル引数対応の構造体、1 引数毎に管理する
struct DENT {
	// データ
	char *path;							// 引数の指定ディレクトリ
	struct dirent **direntlist;			// ディレクトリの全エントリ
	int nth;							// エントリ数
	struct FNAME *fnamelist;			// 全エントリのファイル情報

	// 引数の処理
	int is_file;						// その引数はファイル
	char is_filename[FNAME_LENGTH];		// パスから切り離したファイル名

	// インデント用、文字列の最大桁数
	int inode_digits;
	int nlink_digits;
	int mode_digits;
	int owner_digits;
	int group_digits;
	int size_digits;
	int sizec_digits;
	int count_digits;
	int countc_digits;
	int date_digits;
	int time_digits;
	int week_digits;
	int path_digits;
	int unique_digits;
	int name_digits;
	int kind_digits;
	int linkname_digits;
	int errnostr_digits;
	int extension_digits;
#ifdef MD5
	int md5_digits;
#endif

	// 重複リスト
	struct DLIST *duplist;
};


// --------------------------------------------------------------------------------
#ifdef DEBUG
#define showSwitch(name) if (cfg.name) printf(" %s: %d\n", #name, cfg.name)
// 引数の全スイッチを表示
void
debug_showArgvswitch(struct ALIST cfg)
{
	printStr(label, "show all switch:\n");
	showSwitch(show_simple);
	showSwitch(show_long);
	showSwitch(format_list);
#ifdef MD5
	showSwitch(format_md5);
#endif
	showSwitch(format_unique);
	showSwitch(format_extension);

	showSwitch(deep_unique);
	showSwitch(beginning_word);
	showSwitch(do_emacs);
	showSwitch(paint_string);

	showSwitch(do_uniquecheck);
	showSwitch(show_dotfile);
	showSwitch(only_directory);
	showSwitch(only_file);
	showSwitch(only_paint_string);

	showSwitch(no_color);
	showSwitch(argv_color);
	showSwitch(default_color);

	showSwitch(mtime_sort);
	showSwitch(size_sort);
	showSwitch(name_sort);
	showSwitch(no_sort);

	showSwitch(readable_date);
	showSwitch(readable_size);
	showSwitch(readable_week);
	showSwitch(aggregate_results);
	showSwitch(aggregate_length);

	showSwitch(show_help);
	showSwitch(show_version);
	showSwitch(show_escape);
	showSwitch(output_escape);
}
#endif


// --------------------------------------------------------------------------------
void
swap(int *a, int *b)
{
	if (a != b) {			// XOR なので、異なる場合にのみ実行
		*a = *a ^ *b;
		*b = *a ^ *b;
		*a = *a ^ *b;
	}
}


// 引数の順番を調整する
void
orderSort(int showorder[], struct DENT dent[], int dirarg)
{
	debug printStr(label, "orderSort:\n");
// 	for (int i=0; i<dirarg; i++) {
// 		printf("show order:%d, %d\n", showorder[i], dent[showorder[i]].is_file);
// 	}

	for (int i=0; i<dirarg - 1; i++) {
		int swapped = 0;

		for (int j=0; j<dirarg - i - 1; j++) {
			if (dent[showorder[j]].is_file == 0 && dent[showorder[j + 1]].is_file == 1) {
				swap(&showorder[j], &showorder[j + 1]);
				swapped = 1;
			}
		}
		if (swapped == 0) {
			break;
		}
	}

#ifdef DEBUG
	for (int i=0; i<dirarg; i++) {
		printf(" show order:%d, %d\n", showorder[i], dent[showorder[i]].is_file);
	}
#endif
}


// 各項目の最大表示長さ
void
fnameLength(struct FNAME *p)
{
	// 特殊計測
// 	p->length = strlen(p->name);

	// 固定長の項目も含め、lstat() が失敗した時は "-" になる
	p->inodel  = strlen(p->inode);
	p->nlinkl  = strlen(p->nlink);
	p->model   = strlen(p->mode);	// 固定長
	p->ownerl  = strlen(p->owner);
	p->groupl  = strlen(p->group);
	p->sizel   = strlen(p->size);
	p->sizecl  = strlen(p->sizec);
	p->countl  = strlen(p->count);
	p->countcl = strlen(p->countc);
	p->datel   = strlen(p->date);
	p->timel   = strlen(p->time);	// 固定長
	p->weekl   = strlen(p->week);	// 固定長
#ifdef MD5
	p->md5l    = strlen(p->md5);	// 固定長
#endif

	// lstat() が失敗しても、"-" にならない
	p->kindl  = strlen(p->kind);	// 固定長
	p->pathl  = strlen(p->path);
	p->uniquel    = strlen(p->unique);
	p->linknamel  = strlen(p->linkname);
	p->errnostrl  = strlen(p->errnostr);
	p->extensionl = strlen(p->extension);
}


// 確保した DENT 構造体のデータを開放する
void
freeDENT(struct DENT *dent, int dirarg)
{
	debug printStr(label, "freeDENT:\n");

	for (int i=0; i<dirarg; i++) {
		if (dent[i].fnamelist) {
			for (int j=0; j<dent[i].nth; j++) {
				free(dent[i].fnamelist[j].lowername);
			}
			if (dent[i].nth) {
				free(dent[i].fnamelist);
// 				dent[i].fnamelist = NULL;
			}
		}

		if (dent[i].direntlist) {
			for (int j=0; j<dent[i].nth; j++) {
				free(dent[i].direntlist[j]);
			}
			if (dent[i].nth) {
				free(dent[i].direntlist);
// 				dent[i].direntlist = NULL;
			}
		}

		// --------------------------------------------------------------------------------
		// デバッグ情報の表示
		debug printf(" path: %s\n", dent[i].path);

		freeDuplist(dent[i].duplist);
	}
}


// ================================================================================
int
initAlist(int argc, char *argv[], struct ALIST *cfg, int argverr[])
{
	debug printStr(label, "initAlist:\n");

	for (int i=1; i<argc; i++) {
		int len = strlen(argv[i]);

		if (len >= FNAME_LENGTH) {
			if (argv[i][0] == '-' && argv[i][1] == 'c') {
				// default_color_txt
				argv[i][ListCount * COLOR_TEXT - 1] = '\0';
			} else {
				argv[i][FNAME_LENGTH - 1] = '\0';
			}
		}

		// --------------------------------------------------------------------------------
		// 完全一致の引数
		if (strcmp(argv[i], "-256") == 0) {
			cfg->show_escape++;
			continue;
		}

		if (strcmp(argv[i], "--help") == 0) {
			cfg->show_help++;
			continue;
		}

		if (strcmp(argv[i], "--version") == 0) {
			cfg->show_version++;
			continue;
		}

		if (strcmp(argv[i], "-always") == 0) {
			cfg->output_escape++;
			continue;
		}

		// --------------------------------------------------------------------------------
		// 引数の後に文字列指定
		// 文字指定 -TB
		if (strncmp(argv[i], "-TB", 3) == 0) {
			if (len == 3) { argverr[i] = error; continue; }

			strcpy(cfg->textbegin, argv[i] + 3);
			cfg->lbegin = len - 3;
			cfg->tlen = cfg->lbegin + cfg->lend;
			continue;
		}
		// 文字指定 -TE
		if (strncmp(argv[i], "-TE", 3) == 0) {
			if (len == 3) { argverr[i] = error; continue; }

			strcpy(cfg->textend, argv[i] + 3);
			cfg->lend = len - 3;
			cfg->tlen = cfg->lbegin + cfg->lend;
			continue;
		}

		// 表示順指定
		if (strncmp(argv[i], "-f", 2) == 0) {
			if (len == 2) { argverr[i] = error; continue; }

			cfg->format_list++;
			if (len - 2 > ListCountd) {
				len = ListCountd + 2;
			}
			// 後の指定が優先 (上書き) される
			strncpy(cfg->formatListString, argv[i] + 2, len - 2);
			cfg->formatListString[len - 2] = '\0';
			continue;
		}

		// 色指定
		if (strncmp(argv[i], "-c", 2) == 0) {
			if (len == 2) { argverr[i] = error; continue; }

			cfg->argv_color++;
			// 後の指定が優先 (上書き) される
			strcpy(cfg->color_txt, argv[i] + 2);
			debug printf("argv color:[%s]\n", cfg->color_txt);
			continue;
		}

		// 着色文字列指定
		if (strncmp(argv[i], "-p", 2) == 0) {
			if (len == 2) { argverr[i] = error; continue; }

			cfg->paint_string++;
			// 後の指定が優先 (上書き) される
			char *tmp = argv[i] + 2;
			int tmplen = len - 2;
			// 比較用に小文字化
			for (int j=0; j<tmplen; j++) {
				paintString[j] = tolower(tmp[j]);
			}
			paintString[tmplen] = '\0';
			paintStringLen = tmplen;
			// aggregate の該当長さに paint 色付け
			cfg->aggregate_length = paintStringLen;
			debug printf("paintString:[%s]\n", paintString);
			continue;
		}

		// 該当ファイル指定
		if (strncmp(argv[i], "-P", 2) == 0) {
			if (len == 2) { argverr[i] = error; continue; }

			cfg->only_paint_string++;
			if (len - 2 > FNAME_LENGTH) {
				len = FNAME_LENGTH + 2;
			}
			strncpy(cfg->onlyPaintStr, argv[i] + 2, len - 2);
			cfg->onlyPaintStr[len - 2] = '\0';
			continue;
		}

		// aggregate の該当長さに paint 色付け
		if (strncmp(argv[i], "-R", 2) == 0) {
			if (len == 2) { argverr[i] = error; continue; }

			cfg->aggregate_length = atoi(argv[i] + 2);
			if (cfg->aggregate_length <= 0) { argverr[i] = error; continue; }
			debug printf("aggregate length:%d\n", cfg->aggregate_length);
			continue;
		}

		// --------------------------------------------------------------------------------
		// '-' から始まるのはオプション、、、パス名の場合は、"./-xxx" などにして
		if (argv[i][0] == '-') {
			// スイッチが何も指定されていない "-" の時
			if (len == 1) { argverr[i] = error; continue; }

			// 1 文字引数
			for (int j=1; j<len; j++) {
				switch (argv[i][j]) {
					case 's': cfg->show_simple++;       break;	// lstat() を使用しない printShort()
					case 'l': cfg->show_long++;         break;	// long 表示

					case 'u': cfg->deep_unique++;       break;	// unique チェックを最後まで行う
					case 'b': cfg->beginning_word++;    break;	// uniqueCheckFirstWord() のみ
					case 'e': cfg->do_emacs++;          break;	// emacs 系ファイル名対応
					case 'X': cfg->do_extension++;      break;	// 拡張子

					case 'a': cfg->show_dotfile++;      break;	// '.' から始まるファイルを表示
					case 'o': cfg->only_directory++;    break;	// ディレクトリのみ表示
					case 'O': cfg->only_file++;         break;	// ファイルのみ表示

					case 'n': cfg->no_color++;          break;	// no color 表示
					case 'd': cfg->default_color++;     break;	// default color 表示

					case 'm': cfg->mtime_sort++;        break;	// mtime でソート
					case 'z': cfg->size_sort++;         break;	// size でソート
					case 'N': cfg->name_sort++;         break;	// name でソート
					case 'S': cfg->no_sort++;           break;	// ソート無し

					case 't': cfg->readable_date++;     break;	// human readable time
					case 'i': cfg->readable_size++;     break;	// human readable size
					case 'w': cfg->readable_week++;     break;	// human readable 省略しない曜日/月表示
					case 'r': cfg->aggregate_results++; break;	// 集計結果表示

					case 'h': cfg->show_help++;         break;	// help 表示
					case 'v': cfg->show_version++;      break;	// version 表示

					default: {
						// 上記以外の "-x" ではヘルプ表示
						argverr[i] = error;
					}
					break;
				}
			}
		} else {
			// '-' から始まっていないので、多分ファイル名、パスの指定
// 			strcpy(dirarglist[dirarg], argv[i]);
// 			dirarg++;
			cfg->dirarg[i] = 1;
		}
	}

	// --------------------------------------------------------------------------------
	// -f で行う内容を決定
	for (int i=0; cfg->formatListString[i] != '\0'; i++) {
		switch (cfg->formatListString[i]) {
#ifdef MD5
			// md5 を使用する
			case '5':           cfg->format_md5++;    break;
#endif
			case 'u': case 'U': cfg->format_unique++; break;
			case 'o': case 'O': cfg->format_owner++;  break;
			case 'g': case 'G': cfg->format_group++;  break;
			case 'x': case 'X': cfg->format_extension++; break;

			// makeDate() で処理
			case 'd': case 'D': cfg->format_date++;   break;
			case 'w': case 'W': cfg->format_date++;   break;
			case 't': case 'T': cfg->format_date++;   break;

			// size, count
			case 's': case 'S': cfg->format_size++;   break;
			case 'c': case 'C': cfg->format_size++;   break;
			case 'i': case 'I': cfg->format_size++;   break;
			case 'h': case 'H': cfg->format_size++;   break;

			case 'l': case 'L': cfg->format_link++;   break;
		}
	}

	return 0;
}


// 各引数の依存関係の処理を行う
void
progressAlist(struct ALIST *cfg)
{
	debug printStr(label, "progressAlist:\n");

	// ================================================================================
	// -f が指定された場合は、long 表示
	if (cfg->format_list) {
		cfg->show_long++;
	}

	// human readable date/size/week は long 表示
	if (
		// ただし、-f にその要素があれば
// 		(cfg->format_size && cfg->readable_size) ||
// 		(cfg->format_date && (cfg->readable_date || cfg->readable_week))

		// -f にその要素が無くても -l にはする
		cfg->readable_size || cfg->readable_date || cfg->readable_week
		) {
		cfg->show_long++;
	}

	// ================================================================================
	// only_file の時は、ディレクトリ表示を行わない
	if (cfg->only_file) {
		cfg->only_directory = 0;
	}

	// only_directory の時は、'.' から始まるディレクトリも表示する
	if (cfg->only_directory) {
		cfg->show_dotfile++;
	}

	// -p 指定文字列で色付け
	if (cfg->paint_string) {
		cfg->do_uniquecheck = 0;
		cfg->deep_unique = 0;
		cfg->beginning_word = 0;
		cfg->do_emacs = 0;
	}

	// simple 表示は long にしない (ファイルの種類を問わず単色表示)
	if (cfg->show_simple) {
		cfg->show_long = 0;
		cfg->readable_date = 0;
		cfg->readable_size = 0;
		cfg->readable_week = 0;
	}

	// --------------------------------------------------------------------------------
	// -TB, -TE の設定
	// 片側の指定だけなら同じ文字列を使用
	if (cfg->lbegin == 0) {
		strcpy(cfg->textbegin, cfg->textend);
		cfg->lbegin = cfg->lend;
		cfg->tlen = cfg->lbegin + cfg->lend;
	}

	if (cfg->lend == 0) {
		strcpy(cfg->textend, cfg->textbegin);
		cfg->lend = cfg->lbegin;
		cfg->tlen = cfg->lbegin + cfg->lend;
	}

#ifdef DEBUG
	if (cfg->lbegin == 0 && cfg->lend == 0) {
		strcpy(cfg->textbegin, "[");
		strcpy(cfg->textend,   "]");
		cfg->lbegin = strlen(cfg->textbegin);
		cfg->lend   = strlen(cfg->textend);
		cfg->tlen = cfg->lbegin + cfg->lend;
	}
#endif

	// --------------------------------------------------------------------------------
	// -l が指定されなかったら、-f も基本的に無し
	// pickupString() 向けに操作
	if (cfg->show_long == 0) {
		cfg->formatListString[0] = 'n';
		cfg->formatListString[1] = '\0';

#ifdef MD5
		cfg->format_md5 = 0;
#endif
		cfg->format_unique = 0;
		cfg->format_owner = 0;
		cfg->format_group = 0;
		cfg->format_extension = 0;
	}

	// 引数の色指定
	if (cfg->argv_color) {
		cfg->default_color = 0;		// 環境変数を見ない
	}

	// 色をつけない
	if (cfg->no_color) {
		cfg->argv_color = 0;
		cfg->default_color = 0;

		// -TB, -TE と併用で無ければ、色をつけないから uniqueCheck(), uniqueCheckFirstWord() を飛ばす
		// -TB, -TE が指定されていたら、uniqueCheck() は行う
		// -fu も同様
		if (cfg->lbegin == 0 && cfg->format_unique == 0) {
			cfg->do_uniquecheck = 0;
			cfg->do_emacs = 0;
		}
	}
}


// ================================================================================
void
doOUTPUT(struct DENT *dent, int showorder[], int dirarg, struct ALIST cfg, int count_is_file)
{
	// データの表示、ファイル指定対応 (まずは is_file のデータ)、sort はしない
	if (count_is_file) {
		printStr(base, "single files:\n");

		int k = 0;
		if (cfg.show_long) {
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
				digits['i'] = MAX(p->inode_digits,  digits['I']);		// inode
				digits['h'] = MAX(p->nlink_digits,  digits['H']);		// hard link
				digits['m'] = MAX(p->mode_digits,   digits['m']);		// mode
				digits['o'] = MAX(p->owner_digits,  digits['o']);		// owner
				digits['g'] = MAX(p->group_digits,  digits['g']);		// group
				digits['w'] = MAX(p->week_digits,   digits['w']);		// 曜日
				digits['t'] = MAX(p->time_digits,   digits['T']);		// 日付
				digits['S'] = MAX(p->size_digits,   digits['S']);		// 最大ファイルサイズ
				digits['s'] = MAX(p->sizec_digits,  digits['s']);		// 最大ファイルサイズ、comma 表記
				digits['C'] = MAX(p->count_digits,  digits['C']);		// 最大エントリー数の桁数
				digits['c'] = MAX(p->countc_digits, digits['c']);		// 最大エントリー数の桁数、comma 表記
				digits['d'] = MAX(p->date_digits,   digits['d']);		// 日付の桁数
				digits['p'] = MAX(p->path_digits,   digits['p']);		// path
				digits['u'] = MAX(p->unique_digits, digits['u']);		// ユニーク文字列
				digits['n'] = MAX(p->name_digits,   digits['n']);		// 名前の桁数
				digits['k'] = MAX(p->kind_digits,   digits['k']);		// 種類の桁数
				digits['l'] = MAX(p->linkname_digits,  digits['l']);	// linkname
				digits['e'] = MAX(p->errnostr_digits,  digits['e']);	// errnostr
				digits['x'] = MAX(p->extension_digits, digits['X']);	// 拡張子
#ifdef MD5
				digits['5'] = MAX(p->md5_digits,    digits['5']);		// md5
#endif
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
				printLong(fnamelist, p->nth, cfg, digits);

				// 次があれば \n する
				if (i != dirarg - 1) {
					k++;
				}
			}

		} else {
			// printShort() 向け
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
					strcpy(data[count].kind, fnamelist[j].kind);
					data[count].lowername = fnamelist[j].lowername;

					data[count].showlist = fnamelist[j].showlist;
					data[count].uniquebegin = fnamelist[j].uniquebegin;
					data[count].uniqueend = fnamelist[j].uniqueend;
					data[count].print_length = fnamelist[j].print_length;
					data[count].color = fnamelist[j].color;
					count++;

					// 次があれば \n する
					if (i != dirarg - 1) {
						k++;
					}
				}
			}

			printShort(data, count_is_file, cfg);
		}

#ifdef DEBUG
		printf("dirarg:%d\n", dirarg);

		for (int i=0; i<dirarg; i++) {
			printf("show order:%d, %d\n", showorder[i], dent[showorder[i]].is_file);
		}
#endif

		// is_file が 1 つ有った
		if (k  && dent[showorder[dirarg - 1]].is_file == 0) {
			printf("\n");
		}
	}

	// --------------------------------------------------------------------------------
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
		if (cfg.show_long) {
			digits['i'] = p->inode_digits;
			digits['h'] = p->nlink_digits;
			digits['m'] = p->mode_digits;
			digits['o'] = p->owner_digits;
			digits['g'] = p->group_digits;
			digits['S'] = p->size_digits;
			digits['s'] = p->sizec_digits;
			digits['C'] = p->count_digits;
			digits['c'] = p->countc_digits;
			digits['d'] = p->date_digits;
			digits['t'] = p->time_digits;
			digits['w'] = p->week_digits;
			digits['p'] = p->path_digits;
			digits['u'] = p->unique_digits;
			digits['n'] = p->name_digits;
			digits['k'] = p->kind_digits;
			digits['l'] = p->linkname_digits;
			digits['e'] = p->errnostr_digits;
			digits['x'] = p->extension_digits;
#ifdef MD5
			digits['5'] = p->md5_digits;
#endif
		}

		// ================================================================================
		// is_file 時の表示に合わせる
		if (dirarg > 1) {
			printStr(base, p->path);
			printStr(base, ":\n");
		}

		// --------------------------------------------------------------------------------
		// fnamelist の表示
		printLong( fnamelist, p->nth, cfg, digits);
		printShort(fnamelist, p->nth, cfg);

		// ================================================================================
		// 集計表示
		if (cfg.aggregate_results) {
			printAggregate(fnamelist, p->nth, cfg.aggregate_length);
		}

		if (cfg.do_extension) {
			printExtension(fnamelist, p->nth);
		}

		// --------------------------------------------------------------------------------
		if (i != dirarg - 1) {
			printf("\n");
		}
	}
}


// --------------------------------------------------------------------------------
int
main(int argc, char *argv[])
{
	char dirarglist[argc][FNAME_LENGTH];	// 引数の dir のリスト
	int dirarg = 0;
	int argverr[argc];						// argv のエラー記録

	// 引数のファイル指定の数を数える (ある/無しのフラグ)
	int count_is_file = 0;

#ifdef DEBUG
	printf("argument vector:\n");
	for (int i=0; i<argc; i++) {
		printf("%2d:[%s]\n", i, argv[i]);
	}
#endif

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
	// wcStrlen() 用の初期化
	if (setlocale(LC_ALL, "") == NULL) {
		perror("setlocale");
		printf(" =>setlocale(LC_ALL, \"\");\n");
		exit(EXIT_FAILURE);
	}

	// ================================================================================
	// mytolower() 用の初期化
#ifdef MYTOLOWER
	for (int i=0; i<=UCHAR_MAX; i++){
		map[i] = i;
	}

	{
		int len = strlen(upper);
		for (int i=0; i<len; i++){
			map[(unsigned char) upper[i]] = lower[i];
// 			map[lower[i]] = upper[i];
		}
	}
#endif

	// ================================================================================
	// makeDate() 用の初期化
	// プログラム開始直後 (大体今) を1回だけ
	time_t lt;
	lt = time(NULL);

	// ================================================================================
	// default 値の設定
	struct ALIST cfg = {
		.onlyPaintStr[0] = '\0',
		.formatListString = "mogcdPNkLE",

		// --------------------------------------------------------------------------------
		// Control Sequence Introducer
		// https://en.wikipedia.org/wiki/ANSI_escape_code
		// https://kmiya-culti.github.io/RLogin/ctrlcode.html			2.7.2 CSI

		// 属性 太=1, (細=2), イタリック=3, 下線=4, 点滅=5, (速い点滅=6), 文字/背景の反転=7, (隠す=8), 取消=9, 2重下線=21, 上線=53

		.color_txt = 
		   // 8 色
		   "base=37:normal=34:dir=36:fifo=33:socket=35:device=33:error=31:paint=32:"					// 文字色
// 		   "base=100:"																					// 背景色
// 		   "paint=95:paint=5:"	// 点滅させるためには、明るい色を指定する必要あり

		   // 256 色、3000 は fore, 4000 は back
// 		   "base=3007:normal=3012:dir=3014:fifo=3011:socket=3009:device=3011:error=3009:paint=3032"		// 文字色

// 		   "normal=1:dir=1:socket=1:device=1:label=1:error=1:paint=1:"									// 属性
		   "reset=0"
		   ,
		// --------------------------------------------------------------------------------

		.termlen = 0,
		.termhei = 0,

		.aggregate_length = 0,

		.do_uniquecheck = 1,												// uniqueCheck(), uniqueCheckFirstWord() を実行する
	};

	for (int i = 0; i < ListCount; i++) {
		colorlist[i][0] = '\0';
	}

	// --------------------------------------------------------------------------------
	int (*comparefunc) (const struct dirent **, const struct dirent **);	// scandir() 時のソート
	int (*sortfunc)(const void *, const void *);							// 表示時のソート

	// デフォルトは、uniqueCheck() 向けの設定
	comparefunc = compareNameLength;	// ファイル名の長い順で scandir() -> uniqueCheck(), uniqueCheckFirstWord() の比較回数が一番小さくなる
	printName = printUnique;			// uniqueCheck(), uniqueCheckFirstWord() 結果を表示する
	sortfunc = myAlphaSort;				// 最後にアルファベット順で表示

	// --------------------------------------------------------------------------------
	// default color text を格納
	strcpy(default_color_txt, cfg.color_txt);
	initColor(0, cfg.color_txt);

	// --------------------------------------------------------------------------------
	memset(dirarglist, '\0', sizeof(dirarglist));
	memset(argverr, 0, sizeof(argverr));
	paintString[0] = '\0';
	paintStringLen = 0;

	// ================================================================================
	// 引数の処理
	initAlist(argc, argv, &cfg, argverr);
	for (int i=1; i<argc; i++) {
		if (cfg.dirarg[i] == 1) {
			strcpy(dirarglist[dirarg], argv[i]);
			dirarg++;
		}
	}

	// ================================================================================
	// 出力先ターミナルサイズの取得
	if (cfg.show_simple || cfg.show_long == 0) {
		getTerminalSize(&cfg.termlen, &cfg.termhei);
	}

	// --------------------------------------------------------------------------------
	// 出力先の確認
	// terminal でない場合は無色、-always が指定された場合は color
	if (isatty(fileno(stdout)) == 0 && cfg.output_escape == 0) {
		cfg.no_color++;
	}

	// ================================================================================
	progressAlist(&cfg);

	// ================================================================================
	// 依存・関連する argv の処理
	// dirent 引数無しなので、./ をリストに加える
	if (dirarg == 0) {
		strcpy(dirarglist[0], "./");
		dirarg = 1;
	}

	// --------------------------------------------------------------------------------
	// -p 指定文字列で色付け
	if (cfg.paint_string) {
		comparefunc = compareNameAlphabet;
		printName = printMatchedString;
		sortfunc = NULL;					// compareNameAlphabet でソート済みだから、myAlphaSort しない
	}

	// --------------------------------------------------------------------------------
	// 下に行くほど強い (sortfunc を上書きするから)
	if (cfg.do_emacs) {
		comparefunc = compareNameAlphabet;		// alphabet 順で scandir()
		sortfunc = NULL;						// compareNameAlphabet でソート済みだから、myAlphaSort しない
	}

	// mtime 順にソートする
	if (cfg.mtime_sort) {
		// 新しい順
		sortfunc = myMtimeSort;
		if (cfg.mtime_sort > 1) {
			// 逆順
			sortfunc = myMtimeSortRev;
		}
	}

	// size 順にソートする
	if (cfg.size_sort) {
		// 小さい順
		sortfunc = mySizeSort;
		if (cfg.size_sort > 1) {
			// 逆順
			sortfunc = mySizeSortRev;
		}
	}

	// name 順にソートする
	if (cfg.name_sort) {
		// 逆順
		sortfunc = myAlphaSortRev;
		if (cfg.name_sort > 1) {
			// 通常
			sortfunc = myAlphaSort;
		}
	}

	// ソートしない
	if (cfg.no_sort) {
		comparefunc = NULL;
		sortfunc = NULL;
	}

	// ================================================================================
	// argv[i] は順不同なので、優先順位を実装
	if (cfg.no_color) {
		// 色の初期化
		for (int i=0; i<ListCount; i++) {
			colorlist[i][0] = '\0';
		}
	}

#ifdef DEBUG
	debug_showArgvswitch(cfg);
#endif

	// --------------------------------------------------------------------------------
	// 引数か、環境変数から色を指定する
	// 色について共通の処理
	if (cfg.no_color || cfg.default_color) {
	} else {
		if (cfg.argv_color) {
			initColor(cfg.default_color, cfg.color_txt);
		} else {
			initColor(0, "");
		}
	}

	// ================================================================================
	// 引数にエラーがあった
	for (int i=0; i<argc; i++) {
		if (argverr[i] == error) {
			showUsage(argv);

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

	if (cfg.show_help) {
		showUsage(argv);
		exit(EXIT_SUCCESS);
	}

	if (cfg.show_version) {
		showVersion(argv);
		exit(EXIT_SUCCESS);
	}

	// 色を採用した後に表示
	if (cfg.show_escape) {
		showEscapeList();
		exit(EXIT_SUCCESS);
	}

	// ================================================================================
	// 引数のディレクトリ分の準備、初期化
	struct DENT dent[dirarg];
	int showorder[dirarg];

	// 初期化
	for (int i=0; i<dirarg; i++) {
		dent[i].path = dirarglist[i];
		dent[i].nth = 0;

		dent[i].is_file = 0;
		memset(dent[i].is_filename, 0, sizeof(dent[i].is_filename));

		dent[i].inode_digits  = 0;
		dent[i].nlink_digits  = 0;
		dent[i].mode_digits   = 0;
		dent[i].owner_digits  = 0;
		dent[i].group_digits  = 0;
		dent[i].size_digits   = 0;
		dent[i].sizec_digits  = 0;
		dent[i].count_digits  = 0;
		dent[i].countc_digits = 0;
		dent[i].date_digits   = 0;
		dent[i].time_digits   = 0;
		dent[i].week_digits   = 0;
		dent[i].path_digits   = 0;
		dent[i].unique_digits = 0;
		dent[i].name_digits   = 0;
		dent[i].kind_digits   = 0;
		dent[i].linkname_digits  = 0;
		dent[i].errnostr_digits  = 0;
		dent[i].extension_digits = 0;
#ifdef MD5
		dent[i].md5_digits    = 0;
#endif

		// non-unique リストの初期化
		dent[i].duplist = mallocDuplist("", 0);

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

#ifndef COUNTFUNC
		ret = stat(dirarglist[i], &st);
#else
		ret = countstat(dirarglist[i], &st);
#endif

		// -1: 失敗した場合もファイルとして扱うため、最後の '/' を削除する
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

		// -l 指定時、最後に '/' がないと、ファイルとして扱う
		if (cfg.show_long) {
			if (dirarglist[i][strlen(dirarglist[i]) - 1] != '/') {
				p->is_file = 1;
			}
		}

		if (p->is_file) {
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
	// データの取得、リストへの登録
	for (int i=0; i<dirarg; i++) {
		struct DENT *p;
		p = &dent[i];

		// 多分パス、移動に失敗したら次のパス
		if (chdir(dirarglist[i]) != 0) {
// 			perror("chdir");
			debug printf("chdir: %s [%s]\n", strerror(errno), dirarglist[i]);
// 			printf("\n");

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

		if (p->fnamelist == NULL) {
			perror("malloc");
			printf(" =>size:%zu\n", sizeof(struct FNAME) * p->nth);
			// cwd を試みる
			if (chdir(cwd)) {
				printf("chdir: %s [%s]\n", strerror(errno), cwd);
			}
			exit(EXIT_FAILURE);
		}

		struct FNAME *fnamelist;
		fnamelist = p->fnamelist;

		struct dirent **direntlist;
		direntlist = p->direntlist;

		// fnamelist に登録
#ifdef OMP
// printf("default threads: %d\n", omp_get_num_threads());
// omp_set_num_threads(p->nth);
// #pragma omp parallel for
#pragma omp parallel for num_threads( (p->nth > 512) ? 32 : 4)
#endif
		for (int j=0; j<p->nth; j++) {
			// ファイル名の登録
			addFNamelist(&fnamelist[j], direntlist[j]->d_name);

			// -s はファイル名しか使用しない
			if (cfg.show_simple) {
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

				if (cfg.show_long) {
					strcpy(fnamelist[j].inode,  "-");
					strcpy(fnamelist[j].nlink,  "-");
					strcpy(fnamelist[j].mode,   "-");
					strcpy(fnamelist[j].owner,  "-");
					strcpy(fnamelist[j].group,  "-");
					strcpy(fnamelist[j].size,   "-");
					strcpy(fnamelist[j].sizec,  "-");
					strcpy(fnamelist[j].count,  "-");
					strcpy(fnamelist[j].countc, "-");
					strcpy(fnamelist[j].date,   "-");
					strcpy(fnamelist[j].time,   "-");
					strcpy(fnamelist[j].week,   "-");
#ifdef MD5
					strcpy(fnamelist[j].md5,    "-");
#endif

					fnamelist[j].showlist = SHOW_LONG;
				}

				continue;
			}

			// only_directory の IS_DIRECTORY() で使用
			makeMode(&fnamelist[j], cfg);		// mode data

#ifdef MD5
			if (cfg.format_md5) {
				if (IS_DIRECTORY(fnamelist[j]) != 1) {
					if (makeMD5(fnamelist[j].name, fnamelist[j].md5) == -1) {
						strcpy(fnamelist[j].md5,  "-");
					}
				} else {
					strcpy(fnamelist[j].md5,  "-");
				}
			}
#endif
		}

		// --------------------------------------------------------------------------------
		// cwd ディレクトリに戻る
		if (chdir(cwd)) {
// 			perror("chdir");
// 			printf(" =>chdir to %s fail.\n", cwd);
			printf("chdir: %s [%s]\n", strerror(errno), cwd);
			exit(EXIT_FAILURE);
		}
	}

#ifdef DEBUG
	for (int i=0; i<dirarg; i++) {
		printStr(label, "dent:\n");
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

		// ================================================================================
		// 初めに登録し、重複対象とする (unique 対象から外す)
		addDuplist(p->duplist, ".", 1, -1);
		addDuplist(p->duplist, "..", 2, -1);

		// ================================================================================
		// -f の内容によって、取得する項目を管理する
		// makeDate() は pickupString() より前に実施
		if (cfg.format_date) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}

				// !! 本当は、sb をチェックする
				if (fnamelist[j].mode[1] == '\0') {
					continue;
				}

				makeDate(&fnamelist[j], lt, cfg.readable_week);
				// ----------------------------------------
				// 時間を読みやすくする
				if (cfg.readable_date) {
					makeReadableDate(&fnamelist[j], lt);
				}
			}
		}

		// --------------------------------------------------------------------------------
		// -z size_sort は short でも使うので、SHOW_LONG とは別に
		if (cfg.format_size || cfg.size_sort) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}

				// !! 本当は、sb をチェックする
				if (fnamelist[j].mode[1] == '\0') {
					continue;
				}

				// filesize, count data
				sprintf(fnamelist[j].size, "%ld", fnamelist[j].sb.st_size);
				makeSize(fnamelist[j].sizec, fnamelist[j].sb.st_size);

				// countsize data
				if (IS_DIRECTORY(fnamelist[j])) {
					// ディレクトリの場合は、最大エントリー数を記載
					int ret = countEntry(fnamelist[j].name, dirarglist[i]);

					if (ret != -1) {
						sprintf(fnamelist[j].count, "%d", ret);
						makeSize(fnamelist[j].countc, ret);
					} else {
						// ディレクトリだけど、読めない
						strcpy(fnamelist[j].count, "-");
						strcpy(fnamelist[j].countc, "-");
					}
				} else {
					// ファイルの時は、.size の値をコピー
					strcpy(fnamelist[j].count, fnamelist[j].size);
					strcpy(fnamelist[j].countc, fnamelist[j].sizec);
				}

				makeSize(fnamelist[j].inode, fnamelist[j].sb.st_ino);
				makeSize(fnamelist[j].nlink, fnamelist[j].sb.st_nlink);

				// ----------------------------------------
				// サイズを読みやすくする
				if (cfg.readable_size) {
					makeReadableSize(fnamelist[j].sizec);
					makeReadableSize(fnamelist[j].countc);

					makeReadableSize(fnamelist[j].nlink);
				}
			}
		}

		if (cfg.format_owner) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}

				// !! 本当は、sb をチェックする
				if (fnamelist[j].mode[1] == '\0') {
					continue;
				}

				struct passwd *pw;
				pw = getpwuid(fnamelist[j].sb.st_uid);					// owner
				if (pw == NULL) {
					perror("getpwuid");
					printf(" =>Y%s\n", fnamelist[j].name);
					exit(EXIT_FAILURE);
				}
				strcpy(fnamelist[j].owner, pw->pw_name);
			}
		}

		if (cfg.format_group) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}

				// !! 本当は、sb をチェックする
				if (fnamelist[j].mode[1] == '\0') {
					continue;
				}

				struct group *gr;
				gr = getgrgid(fnamelist[j].sb.st_gid);					// group
				if (gr == NULL) {
					perror("getgrgid");
					printf(" =>Y%s\n", fnamelist[j].name);
					exit(EXIT_FAILURE);
				}
				strcpy(fnamelist[j].group, gr->gr_name);
			}
		}

		// --------------------------------------------------------------------------------
		if (cfg.format_extension || cfg.do_extension || cfg.do_emacs) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}

				// 最後の . 以降
				char *extension = strrchr(fnamelist[j].name, '.');
				if (extension) {
					if (extension != fnamelist[j].name) {
						strcpy(fnamelist[j].extension, extension + 1);
						debug printf("ext:%s, %s\n", fnamelist[j].extension, fnamelist[j].name);
					}
				}
			}
		}

#ifdef DEBUG
		printf("\n");
		printf("unique list: before\n");
		debug_displayAllQsortdata(fnamelist, p->nth, cfg);
#endif

		// ================================================================================
		// 表示しないデータを間引く
		// '.' から始まるファイル/ディレクトリは表示対象外
		if (cfg.show_dotfile == 0) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].name[0] == '.') {
					fnamelist[j].showlist = SHOW_NONE;		// 非表示設定
// 					fnamelist[j].sourcelist = -1;			// 検索候補からも除外
				}
			}
		}

		// --------------------------------------------------------------------------------
		// ディレクトリのみ表示する
		if (cfg.only_directory) {
			for (int j=0; j<p->nth; j++) {
				if (IS_DIRECTORY(fnamelist[j]) != 1) {
					fnamelist[j].showlist = SHOW_NONE;
// 					fnamelist[j].sourcelist = -1;		// 検索候補から除外
				}
			}
		}

		// ファイルのみ表示する
		if (cfg.only_file) {
			for (int j=0; j<p->nth; j++) {
				if (IS_DIRECTORY(fnamelist[j])) {
					fnamelist[j].showlist = SHOW_NONE;
// 					fnamelist[j].sourcelist = -1;		// 検索候補から除外
				}
			}
		}

		// --------------------------------------------------------------------------------
		// 文字列がマッチしたファイルのみ表示
		if (cfg.only_paint_string) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}

				if (pickupString(fnamelist[j], cfg.onlyPaintStr, cfg.formatListString, strstr) == 1) {
					continue;
				}

				// 該当しない
				fnamelist[j].showlist = SHOW_NONE;
			}
		}

		// ================================================================================
		// -l 表示を行う
		if (cfg.show_long) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}

				if (fnamelist[j].showlist == SHOW_SHORT) {
					fnamelist[j].showlist = SHOW_LONG;
				}
			}
		}

		// ================================================================================
		// 対象は、emacs やアーカイバのタイプ (画像や音楽ファイル、3D ファイルも)
		if (cfg.do_emacs) {
			debug printStr(label, "emacs:\n");

			struct DLIST *extensionduplist;
			extensionduplist = mallocDuplist("", 0);

			// 1 個目だけ特別対応
			fnamelist[0].targetlist = 1;
			fnamelist[0].sourcelist = 1;

			for (int j=1; j<p->nth; j++) {
				// 拡張子対応、.el, .elc, .zip, .xxx 等
				// 2 回以上おなじ拡張子がある場合、uniqueCheck() の対象にならないように、拡張子を登録する
				char *extension = fnamelist[j].extension;
				if (extension) {
					extension++;
					int len = strlen(extension);
					if (len) {
						if (searchDuplist(extensionduplist, extension, len, j) == 0) {
							addDuplist(extensionduplist, extension, len, j);
						} else {
							searchDuplist(p->duplist, extension + 1, len, -1);
// 							addDuplist(p->duplist, extension + 1, len, -1);
						}
					}
				}

				// --------------------------------------------------------------------------------
				// 2 つの名称を比較して、どの程度同じか判断する、、、、.el, .elc なら、[i] の方がファイル名が長いはず
				float m = matchPercent(fnamelist[j - 1], fnamelist[j]);

				debug printf(" %.2f: %s\n", m, fnamelist[j].name);

				// 0.75: 3/4 文字、4/5 文字以上同じ (x.el と、x.elc) で無ければ、別のファイルと考える
				if (m < 0.75) {
					fnamelist[j].targetlist = 1;
					fnamelist[j].sourcelist = 1;
				} else {
					fnamelist[j].targetlist = -1;
					fnamelist[j].sourcelist = -1;
// 長い方に色付け、unique になる確率が上がる
#if 1
					// 同じグループと判断して、paint 色にする
					fnamelist[j].color = paint;

					// 前のファイル名とほぼ同じだけど、こっちの方がファイル名が長い (x.el と、x.elc)
					if (fnamelist[j].length > fnamelist[j - 1].length) {
						fnamelist[j - 1].targetlist = -1;
						fnamelist[j].targetlist = 1;

						fnamelist[j - 1].sourcelist = -1;
						fnamelist[j].sourcelist = 1;
					}
#else
// 最初の方に色付け
					if (fnamelist[j].length > fnamelist[j - 1].length) {
						fnamelist[j - 1].color = paint;
					}
#endif
				}
			}
			freeDuplist(extensionduplist);

			runUniqueCheck(fnamelist, p->nth, p->duplist, uniqueCheckEmacs, cfg.deep_unique);
			refrectDuplist(p->duplist, fnamelist);
			cfg.do_uniquecheck = 0;
		}

		// ================================================================================
		// 指定文字列に着色する
		if (cfg.paint_string) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}

				if (pickupString(fnamelist[j], paintString, cfg.formatListString, strcasestr) == 1) {
					// hit した記録、aggregate_results でカウント
					fnamelist[j].uniqueend = 1;
				}
			}
		}

		// --------------------------------------------------------------------------------
		// unique 文字列に着色する
		if (cfg.do_uniquecheck) {
			if (cfg.beginning_word) {
				// uniqueCheckFirstWord() のみを行う
				runUniqueCheck(fnamelist, p->nth, p->duplist, uniqueCheckFirstWord, cfg.deep_unique);
			} else {
				// 対象を分けて、uniqueCheckFirstWord() の次に uniqueCheck() を行う
				// 今は、. から始まるファイル名と、そうでないファイル名が競合しないからこうしている
				// 例えば、.bash_history[y] と R[Y]OMA、uniqueCheck() だけにすると、R[YO]MA に変わる
#if 1
				// dotfile のみに uniqueCheckFirstWord()
				for (int j=0; j<p->nth; j++) {
					if (fnamelist[j].showlist == SHOW_NONE) {
						continue;
					}

					if (fnamelist[j].name[0] == '.') {
						fnamelist[j].targetlist = 1;
						fnamelist[j].sourcelist = 1;
					} else {
						fnamelist[j].targetlist = -1;
						fnamelist[j].sourcelist = -1;
					}
				}
				runUniqueCheck(fnamelist, p->nth, p->duplist, uniqueCheckFirstWord, cfg.deep_unique);

				// --------------------------------------------------------------------------------
				// dotfile 以外に uniqueCheck() を行う
				for (int j=0; j<p->nth; j++) {
					if (fnamelist[j].showlist == SHOW_NONE) {
						continue;
					}

					if (fnamelist[j].name[0] != '.') {
						fnamelist[j].targetlist = 1;
						fnamelist[j].sourcelist = 1;
					} else {
						fnamelist[j].targetlist = -1;
						fnamelist[j].sourcelist = -1;
					}
				}
#endif
				runUniqueCheck(fnamelist, p->nth, p->duplist, uniqueCheck, cfg.deep_unique);
			}

			refrectDuplist(p->duplist, fnamelist);
		}

		// --------------------------------------------------------------------------------
		// ユニーク文字列
		// !! エスケープ文字列も表示、該当なしと、' ' の差がわからないから
		if (cfg.format_unique) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}

				if (fnamelist[j].uniquebegin != -1) {
					int len = fnamelist[j].uniqueend - fnamelist[j].uniquebegin + 1;
					strncpy(fnamelist[j].unique, fnamelist[j].name + fnamelist[j].uniquebegin, len);
					fnamelist[j].unique[len] = '\0';
				}
			}
		}

		// --------------------------------------------------------------------------------
		// 集計結果の該当長さの文字列を paint 色で表示
		if (cfg.aggregate_length) {
			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}

				if (fnamelist[j].uniquebegin != -1) {
					int len = fnamelist[j].uniqueend - fnamelist[j].uniquebegin + 1;
					if (len == cfg.aggregate_length) {
						fnamelist[j].color = paint;
					}
				}
			}
		}

		// --------------------------------------------------------------------------------
		// 共通、printShort() でも、printLong() でも print_length は必要
		for (int j=0; j<p->nth; j++) {
			if (fnamelist[j].showlist == SHOW_NONE) {
				continue;
			}

			// 何文字エスケープされるか、printLength: の計算値、printUnique() と合わせる
			// !! printMatchedString() とも合わせる必用あり
			int count = 0;
			// cfg.do_uniquecheck (uniqueCheck()) が終わっている必要あり
			if (fnamelist[j].uniquebegin != -1) {
				for (int k=fnamelist[j].uniquebegin; k<=fnamelist[j].uniqueend; k++) {
					if (strchr(ESCAPECHARACTER, fnamelist[j].name[k])) {
						count++;
					}
				}
				count += cfg.tlen;
			}
			fnamelist[j].print_length = wcStrlen(fnamelist[j]) + count;
		}

		// --------------------------------------------------------------------------------
		// 各項目の最大表示長さ
		if (cfg.show_long) {
			debug printStr(label, "fnameLength:\n");

			for (int j=0; j<p->nth; j++) {
				if (fnamelist[j].showlist == SHOW_NONE) {
					continue;
				}

				// 各項目の長さ計算
				fnameLength(&fnamelist[j]);

				// 各項目の最大幅数の確定
				p->name_digits =  MAX(p->name_digits, countMatchedString(fnamelist[j].name) * cfg.tlen + fnamelist[j].print_length);

				p->inode_digits  = MAX(p->inode_digits,  countMatchedString(fnamelist[j].inode)  * cfg.tlen + fnamelist[j].inodel);
				p->nlink_digits  = MAX(p->nlink_digits,  countMatchedString(fnamelist[j].nlink)  * cfg.tlen + fnamelist[j].nlinkl);
				p->mode_digits   = MAX(p->mode_digits,   countMatchedString(fnamelist[j].mode)   * cfg.tlen + fnamelist[j].model);
				p->owner_digits  = MAX(p->owner_digits,  countMatchedString(fnamelist[j].owner)  * cfg.tlen + fnamelist[j].ownerl);
				p->group_digits  = MAX(p->group_digits,  countMatchedString(fnamelist[j].group)  * cfg.tlen + fnamelist[j].groupl);
				p->size_digits   = MAX(p->size_digits,   countMatchedString(fnamelist[j].size)   * cfg.tlen + fnamelist[j].sizel);
				p->sizec_digits  = MAX(p->sizec_digits,  countMatchedString(fnamelist[j].sizec)  * cfg.tlen + fnamelist[j].sizecl);
				p->count_digits  = MAX(p->count_digits,  countMatchedString(fnamelist[j].count)  * cfg.tlen + fnamelist[j].countl);
				p->countc_digits = MAX(p->countc_digits, countMatchedString(fnamelist[j].countc) * cfg.tlen + fnamelist[j].countcl);
				p->date_digits   = MAX(p->date_digits,   countMatchedString(fnamelist[j].date)   * cfg.tlen + fnamelist[j].datel);
				p->time_digits   = MAX(p->time_digits,   countMatchedString(fnamelist[j].time)   * cfg.tlen + fnamelist[j].timel);
				p->week_digits   = MAX(p->week_digits,   countMatchedString(fnamelist[j].week)   * cfg.tlen + fnamelist[j].weekl);
				p->kind_digits   = MAX(p->kind_digits,   countMatchedString(fnamelist[j].kind)   * cfg.tlen + fnamelist[j].kindl);
				p->path_digits   = MAX(p->path_digits,   countMatchedString(fnamelist[j].path)   * cfg.tlen + fnamelist[j].pathl);
				p->unique_digits    = MAX(p->unique_digits,    countMatchedString(fnamelist[j].unique)    * cfg.tlen + fnamelist[j].uniquel);
				p->linkname_digits  = MAX(p->linkname_digits,  countMatchedString(fnamelist[j].linkname)  * cfg.tlen + fnamelist[j].linknamel);
				p->errnostr_digits  = MAX(p->errnostr_digits,  countMatchedString(fnamelist[j].errnostr)  * cfg.tlen + fnamelist[j].errnostrl);
				p->extension_digits = MAX(p->extension_digits, countMatchedString(fnamelist[j].extension) * cfg.tlen + fnamelist[j].extensionl);
#ifdef MD5
				p->md5_digits    = MAX(p->md5_digits,    countMatchedString(fnamelist[j].md5)    * cfg.tlen + fnamelist[j].md5l);
#endif
			}
		}

		// --------------------------------------------------------------------------------
#ifdef DEBUG
		debug printStr(label, "scandir/qsort:\n");
		printf(" scandir: ");
		if (comparefunc == NULL)                { printf("NULL\n");                }
		if (comparefunc == compareNameLength)   { printf("compareNameLength\n");   }
		if (comparefunc == compareNameAlphabet) { printf("compareNameAlphabet\n"); }

		printf(" qsort:   ");
		if (sortfunc == NULL)           { printf("NULL\n");           }
		if (sortfunc == myAlphaSort)    { printf("myAlphaSort\n");    }
		if (sortfunc == myAlphaSortRev) { printf("myAlphaSortRev\n"); }
		if (sortfunc == mySizeSort)     { printf("mySizeSort\n");     }
		if (sortfunc == mySizeSortRev)  { printf("mySizeSortRev\n");  }
		if (sortfunc == myMtimeSort)    { printf("myMtimeSort\n");    }
		if (sortfunc == myMtimeSortRev) { printf("myMtimeSortRev\n"); }
#endif

		// ================================================================================
		// 全データに対して行う処理

		// 表示用にソートする
		if (sortfunc) {
			qsort(fnamelist, p->nth, sizeof(struct FNAME), sortfunc);
		}

#ifdef DEBUG
		printf("\n");
		printf("unique list: after\n");
		debug_displayAllQsortdata(fnamelist, p->nth, cfg);
#endif
	}

	// --------------------------------------------------------------------------------
	// 表示順変更、ディレクトリ表示があと
	orderSort(showorder, dent, dirarg);

	// ================================================================================
	// 全データに対して行う
	for (int i=0; i<dirarg; i++) {
		struct DENT *p;
		p = &dent[i];

		struct FNAME *fnamelist;
		fnamelist = p->fnamelist;

		// printLong() で無ければ使用しない
		if (cfg.show_long) {
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
				info_pointers['S']                      = fnamelist[j].size;
				info_pointers['s']                      = fnamelist[j].sizec;
				info_pointers['C']                      = fnamelist[j].count;
				info_pointers['c']                      = fnamelist[j].countc;
				info_pointers['d'] = info_pointers['D'] = fnamelist[j].date;
				info_pointers['t'] = info_pointers['T'] = fnamelist[j].time;
				info_pointers['w'] = info_pointers['W'] = fnamelist[j].week;
				info_pointers['p'] = info_pointers['P'] = fnamelist[j].path;
				info_pointers['u'] = info_pointers['U'] = fnamelist[j].unique;
				info_pointers['n'] = info_pointers['N'] = fnamelist[j].name;
				info_pointers['k'] = info_pointers['K'] = fnamelist[j].kind;
				info_pointers['l'] = info_pointers['L'] = fnamelist[j].linkname;
				info_pointers['e'] = info_pointers['E'] = fnamelist[j].errnostr;
				info_pointers['x'] = info_pointers['X'] = fnamelist[j].extension;
#ifdef MD5
				info_pointers['5'] =                      fnamelist[j].md5;
#endif
				// --------------------------------------------------------------------------------
				info_pointers['['] = "[";
				info_pointers[']'] = "]";
				info_pointers['|'] = "|";
				info_pointers[','] = ",";

				for (int k=0; cfg.formatListString[k] != '\0'; k++) {
					fnamelist[j].info[k] = info_pointers[(unsigned char) cfg.formatListString[k]];
				}
			}
		}
	}

	// --------------------------------------------------------------------------------
	// データの表示 (パス指定対応)
	doOUTPUT(dent, showorder, dirarg, cfg, count_is_file);

	// ================================================================================
	// 表示終了後 に free
	debug printf("----------\n");
	freeDENT(dent, dirarg);

	// --------------------------------------------------------------------------------
	// 標準関数のカウント数の表示
#ifdef COUNTFUNC
	printf("\n");
	showCountFunc();
#endif

	return (EXIT_SUCCESS);
}
