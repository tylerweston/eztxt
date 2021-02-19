#include <curses.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <time.h>

/*
todo: 
- start refactoring now
- delete/backspace become same function
- simple menu including saving/loading files
- longer files and scrolling 
- header
- pressing CTRL or ALT breaks things
- add commands for jumping between words?
- add cutline + paste line (or lines?) option
 (this will just be moving things around in
  the linked list!)
- start writing unit tests using check!
*/

#define LINE_LENGTH 80
#define BANNER_WIDTH 20
#define HALF_BANNER_WIDTH (BANNER_WIDTH / 2)
#define CTRL(x) ((x) & 0x1f)

#define min(a,b) \
	({ __typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a < _b ? _a : _b; })

typedef struct docline 
{
	char line[LINE_LENGTH];
	char formatting[LINE_LENGTH];	// use this to store color
	struct docline* prevline;
	struct docline* nextline;
} docline;

void draw_lines(docline*, int, bool);
void remove_line(docline* line, docline** head, docline** tail);

int main(int argc, char** argv)
{
	const char* const banner = "--- eztxt - 2020 - Tyler Weston --- a simple text editor written in c using ncurses --- ";
	char curbanner[BANNER_WIDTH];
	int bannerindex = 0;
	initscr();
	int cur_col = 1;
	if (has_colors())
	{
		use_default_colors();
		start_color();
		init_pair(cur_col, -1, COLOR_BLACK);
		//init_pair(2, -1, COLOR_BLUE);
	}
	//bkgd(COLOR_PAIR(2));

	printw("eztxt - Tyler Weston - F12 exits - curses version: %s", curses_version());
	cbreak();
	noecho();
	nodelay(stdscr, TRUE);

	keypad(stdscr, TRUE);
	curs_set(0);
	refresh();
	wchar_t ch;
	bool exitFlag = false;
	int xpos = 0, ypos = 0;
	int scrolltick = 0;
	int width, height;
	getmaxyx(stdscr, height, width);

	// int topline = 0;
	// bool screenScrolled = false;
	// int changedLines = 0;
	bool screenClean = true;

	docline* firstline = malloc(sizeof(docline));
	firstline->prevline = NULL;
	firstline->nextline = NULL;

	docline* currline = firstline;
	docline* head;
	docline* tail;
	head = firstline;
	tail = firstline;

	clock_t now = clock();

	while(!exitFlag)
	{

		screenClean = false;
		ch = getch();

		switch (ch)
		{
		case ERR:
			screenClean = true;
			break;
			// SLEFT, SRIGHT for shift left, shift right
		case CTRL(KEY_LEFT):
			mvprintw(0, 45, "pressed CTRL left");
			break;

		case KEY_RESIZE:
			getmaxyx(stdscr, height, width);
			break;

		case '\n':	// ENTER key, KEY_ENTER doesn't work?
		;
			docline* newline = malloc(sizeof(docline));
			if (xpos == 0)
			{
				// insert before
				newline->prevline = currline->prevline;
				newline->nextline = currline;
				currline->prevline = newline;
				if (head == currline)
					head = newline;
				currline = newline;
			}
			else
			{
				// here, we'll take the rest of the chars to the
				// next line
				// insert after
				newline->prevline = currline;
				newline->nextline = currline->nextline;
				currline->nextline = newline;

				if (tail == currline)
					tail = newline;
				currline = newline;
			}
			xpos = 0;
			++ypos;
			break;

		case KEY_UP:
			if (currline->prevline)
			{
				currline = currline->prevline;
				xpos = min(xpos, strlen(currline->line));
				--ypos;
			}
			break;

		case KEY_DOWN:
			if (currline->nextline)
			{
				currline = currline->nextline;
				xpos = min(xpos, strlen(currline->line));
				++ypos;
			}
			break;

		case KEY_LEFT:
			xpos--;
			break;

		case KEY_RIGHT:
			xpos++;
			break;

		case KEY_F(12):
			exitFlag = true;
			break;

		case KEY_BACKSPACE:
			if (xpos == 0)
			{
				// bring the line onto the one above
				if (currline->prevline != NULL)
				{

				}
				// otherwise, we're the first line
			}
			else
			{
				xpos--;
				for (int i = xpos; i < LINE_LENGTH; ++i)
				{
					currline->line[i] = currline->line[i + 1];
				}
			}
			break;

		case KEY_DC:
			if (xpos == 0 && currline->line[0] == '\0')
			{
				docline* tmp;
				if (currline == tail)
				{
					tmp = currline->prevline;
					--ypos;
				}
				else if (currline == head && currline->nextline == NULL)
				{
					// is this the problematic case?
				}
				else
				{
					tmp = currline->nextline;
				}
				remove_line(currline, &head, &tail);
				currline = tmp;
			}
			else
			{
				for (int i = xpos; i < LINE_LENGTH; ++i)
				{
					currline->line[i] = currline->line[i + 1];
				}
			}
			break;

		default:
			// if we're not inserting at back of line, make room
			if (xpos != strlen(currline->line))
			{
				for (int i = LINE_LENGTH; i > xpos; --i)
				{
					currline->line[i] = currline->line[i - 1];
				}

			}
			currline->line[xpos] = (char) ch;
			xpos++;
			break;
		}

		if (xpos <= 0)
			xpos = 0;
		if (xpos >= strlen(currline->line))
			xpos = strlen(currline->line);
		if (ypos <= 0)
			ypos = 0;

		if (clock() - now > 10000)
		{
			scrolltick++;
			now = clock();
			if (scrolltick >= 10)
			{
				scrolltick = 0;
				++bannerindex;
			}
			bannerindex %= strlen(banner);
			for (int i = 0; i < BANNER_WIDTH; ++i)
			{
				curbanner[i] = banner[(bannerindex + i) % strlen(banner)];
			}
			curbanner[BANNER_WIDTH] = '\0';
			draw_lines(head, 20, screenClean);
			mvchgat(ypos + 1, xpos, 1, A_BLINK, COLOR_PAIR(cur_col), NULL);

			mvprintw(0 , 0, "%d, %d", xpos, ypos);
			mvprintw(0, width - 6, "%02d, %02d", height, width);
			mvprintw(0, (width/2)-HALF_BANNER_WIDTH, "%s", curbanner);
		}

	}

	endwin();
	exit(EXIT_SUCCESS);
}

void remove_line(docline* line, docline** head, docline** tail)
{
	if (*head == *tail && *head == line)
		return;
	if (*head == line)
		*head = line->nextline;
	if (*tail == line)
		*tail = line->prevline;
	if (line->nextline != NULL)
		line->nextline->prevline = line->prevline;
	if (line->prevline != NULL)
		line->prevline->nextline = line->nextline;
}

void draw_lines(docline* top, int max_lines, bool clean)
{
	// if (clean) 
	// 	return;
	docline* cur = top;
	int yline = 1;
	// super cheap just to clear entire screen?
	clear();
	do 
	{
		move(yline, 0);
		mvprintw(yline, 0, "%s", cur->line);
		yline++;
		cur = cur->nextline;
	} while (cur != NULL && yline < max_lines);
	refresh();
}