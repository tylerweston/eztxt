#include <curses.h>	// include stdio
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <time.h>

#define MAJOR_VERSION 0
#define MINOR_VERSION 1

#define LINE_LENGTH 80
#define BANNER_WIDTH 20
#define MAX_DEBUG_MSG 16
#define DISPLAY_DEBUG_TIME 10

#define HALF_BANNER_WIDTH (BANNER_WIDTH / 2)
#ifndef CTRL
#define CTRL(c) ((c) & 037)
#endif

#define min(a,b)               \
	({ __typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b);     \
	_a < _b ? _a : _b; }          )

typedef struct docline 
{
	char line[LINE_LENGTH];
	attr_t formatting[LINE_LENGTH];	// use this to store color
	struct docline* prevline;
	struct docline* nextline;
} docline;

extern char* current_filename;

// make our colors DEFINEs in this bigger scope?
// use an enum for this?
#define CUR_PAIR 1
#define QUOTE_PAIR 2
#define BAR_PAIR 3
#define COMMENT_PAIR 4
#define PUNC_PAIR 5
#define REG_PAIR 6
#define SECTION_PAIR 7
#define KEYWORD_PAIR 8
#define LABEL_PAIR 9