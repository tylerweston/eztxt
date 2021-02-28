#include <curses.h>	// include stdio
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include <getopt.h>

#define MAJOR_VERSION 0
#define MINOR_VERSION 1
#define BUILD_VERSION 3

#define TAB_DISTANCE 4
#define LINE_LENGTH 80
#define MAX_DEBUG_MSG 24
#define DISPLAY_DEBUG_TIME 10
#define MAX_RESPONSE_SIZE 36
#define MAX_FILE_NAME 36
#define MAX_CURSORS 8

// maybe a system has already defined these?
#ifndef CTRL
#define CTRL(c) ((c) & 037)
#endif

#ifndef ESC
#define ESC 27
#endif

#define min(a,b)               \
	({ __typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b);     \
	_a < _b ? _a : _b; }          )

 // #define min(X,Y) ((X) < (Y) ? : (X) : (Y))

typedef struct docline 
{
	char line[LINE_LENGTH];				// text content of a line
	attr_t formatting[LINE_LENGTH];		// use this to store color
	struct docline* prevline;
	struct docline* nextline;
} docline;

typedef struct
{
	size_t xpos;
	size_t ypos;
	signed int width;
	docline* currline;
	// todo: each cursor will keep track of it's own current line!
} cursor_pos;

typedef struct 
{
	size_t number_of_lines;
	size_t number_of_chars;
} docinfo;



#define CUR_PAIR 1
#define QUOTE_PAIR 2
#define BAR_PAIR 3
#define COMMENT_PAIR 4
#define PUNC_PAIR 5
#define REG_PAIR 6
#define SECTION_PAIR 7
#define KEYWORD_PAIR 8
#define LABEL_PAIR 9
#define NUM_PAIR 10
#define ERROR_PAIR 11
#define ERROR_BLOCK_PAIR 12
#define LINE_NO_PAIR 13
#define MACRO_PARAM_PAIR 14

// allow other function to set debug messages
void set_debug_msg(const char* msg, ...);
void wait_for_keypress();;