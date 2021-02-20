#include <curses.h>	// include stdio
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <time.h>

#include "headers/util.h"


/*
todo: 
- start refactoring now
- make left and right keys skip forwards and backwards lines
- delete/backspace become same function
- simple menu including saving files
- longer files and scrolling 
- header
- pressing CTRL or ALT breaks things
- add cutline + paste line (or lines?) option
 (this will just be moving things around in
  the linked list!)
- start writing unit tests using check!
- handle tabs properly
- store coloring information in a parallel array
  so we only calculate it when a line changes?
- should quotes carry over lines? probably not
- since this is only for cheesy little assembly progs?
- remove the silly scrolling bar, it's distracting and useless
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
void clear_doc(docline* head);
void load_doc(const char*, docline** head, docline** tail);
void save_doc(docline* head);
void do_menu();
void parse_line(docline* line);

char* current_filename = NULL;

// make our colors DEFINEs in this bigger scope?
// use an enum for this?
#define QUOTE_PAIR 2
#define BAR_PAIR 3
#define COMMENT_PAIR 4
#define PUNC_PAIR 5
#define REG_PAIR 6
#define SECTION_PAIR 7

/*
COLOR_BLACK
COLOR_RED
COLOR_GREEN
COLOR_YELLOW
COLOR_BLUE
COLOR_MAGENTA
COLOR_CYAN
COLOR_WHITE
*/

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
		init_pair(cur_col, COLOR_RED, COLOR_BLACK);
		init_pair(QUOTE_PAIR, COLOR_CYAN, -1);
		init_pair(BAR_PAIR, -1, COLOR_MAGENTA);
		init_pair(COMMENT_PAIR, COLOR_GREEN, - 1);
		init_pair(PUNC_PAIR, COLOR_BLUE, -1);
		init_pair(REG_PAIR, COLOR_YELLOW, -1);
		init_pair(SECTION_PAIR, COLOR_MAGENTA, -1);
	}
	else
	{
		printw("No color terminal found, exiting.");
		printw("Press a key to exit...");
		getch();
		goto cleanup_and_end;
	}

	printw("eztxt - Tyler Weston - F12 exits - curses version: %s", curses_version());
	cbreak();
	noecho();
	nodelay(stdscr, TRUE);

	keypad(stdscr, TRUE);
	curs_set(0);
	refresh();
	wchar_t ch;
	double avg_cpu_mhz = 0.0;
	bool exitFlag = false;
	bool menuFlag = false;
	int xpos = 0, ypos = 0;
	int scrolltick = 0;
	int width, height;
	int linelen;
	getmaxyx(stdscr, height, width);

	// int topline = 0;
	// bool screenScrolled = false;
	// int changedLines = 0;
	bool screen_clean = true;
	bool unsaved_changes;

	docline* firstline = calloc(1, sizeof(docline));
	firstline->prevline = NULL;
	firstline->nextline = NULL;

	docline* currline = firstline;
	docline* head;
	docline* tail;
	head = firstline;
	tail = firstline;

	clock_t now = clock();

	if (argc != 1)
	{
		clear_doc(head);
		load_doc(argv[1], &head, &tail);
		currline = head;
	}


	while(!exitFlag)
	{

		screen_clean = false;
		ch = getch();

		switch (ch)
		{

		case CTRL('s'):
			save_doc(head);
			break;

		case ERR:
			screen_clean = true;
			break;

		case KEY_SLEFT:
			while (xpos-- >=0 && currline->line[xpos] == ' ');
			while (xpos-- >=0 && currline->line[xpos] != ' ');
			break;

		case KEY_SRIGHT:
			//mvprintw(0, 45, "pressed SHIFT right");
			linelen = strlen(currline->line);
			while (xpos++ <= linelen && currline->line[xpos] == ' ');
			while (xpos++ <= linelen && currline->line[xpos] != ' ');
			break;

		case KEY_RESIZE:
			getmaxyx(stdscr, height, width);
			break;

		case '\n':	// ENTER key, KEY_ENTER doesn't work?
		;
			docline* newline = calloc(1, sizeof(docline));
			// TODO: Handle special case where we are at first character
			// of the line, then we can just insert a new blank line before this
			// one and not worry about the copy we're doing below.
			// if (xpos == 0)
			// {
			// 	// insert before
			// 	newline->prevline = currline->prevline;
			// 	newline->nextline = currline;
			// 	currline->prevline = newline;
			// 	if (head == currline)
			// 		head = newline;
			// 	currline = newline;
			// }
			// else
			
			// here, we'll take the rest of the chars to the
			// next line
			// insert after
			int nindex = 0;
			for (int i = xpos; i < strlen(currline->line); ++i)
			{
				newline->line[nindex] = currline->line[i];
				nindex++;
			}
			newline->line[nindex] = '\0';
			for (int i = strlen(currline->line); i >= xpos ; --i)
			{
				currline->line[i] = '\0';
			}
			newline->prevline = currline;
			newline->nextline = currline->nextline;
			currline->nextline = newline;
			if (tail == currline)
				tail = newline;
			currline = newline;
			
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
			if (xpos == -1 && currline->prevline)
			{
				currline = currline->prevline;
				ypos--;
				xpos = strlen(currline->line);
			}
			break;

		case KEY_RIGHT:
			xpos++;
			if (xpos > strlen(currline->line) && currline->nextline)
			{
				currline = currline->nextline;
				ypos++;
				xpos = 0;
			}
			break;

		case KEY_F(12):
			exitFlag = true;
			break;

		case KEY_F(2):
			menuFlag = true;
			break;

		case KEY_BACKSPACE:
			if (xpos == 0)
			{
				// bring the line onto the one above
				if (currline->prevline != NULL)
				{
					int lineindex = strlen(currline->prevline->line);
					int newxpos = lineindex;
					for (int i = 0; i < strlen(currline->line); ++i)
					{
						// error check this
						currline->prevline->line[lineindex++] = currline->line[i];
						if (lineindex >= 80) lineindex = 80;
					}
					docline* tmp = currline->prevline;
					remove_line(currline, &head, &tail);
					currline = tmp;
					ypos--;
					xpos = newxpos;

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
				if (currline == head && currline->nextline == NULL)
				{
					// empty document, nothing to do here
					break;
				}
				docline* tmp;
				if (currline == tail)
				{
					tmp = currline->prevline;
					--ypos;
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

		if (menuFlag)
		{
			do_menu();
		}

		// screen update
		if (clock() - now > 10000)
		{
			scrolltick++;
			now = clock();
			if (scrolltick >= 10)
			{
				scrolltick = 0;
				++bannerindex;
				avg_cpu_mhz = cpu_info();
			}
			bannerindex %= strlen(banner);
			for (int i = 0; i < BANNER_WIDTH; ++i)
			{
				curbanner[i] = banner[(bannerindex + i) % strlen(banner)];
			}
			curbanner[BANNER_WIDTH] = '\0';
			draw_lines(head, height - 1, screen_clean);
			mvchgat(ypos + 1, xpos, 1, A_BLINK, COLOR_PAIR(cur_col), NULL);

			mvprintw(0 , 0, "%d, %d", xpos, ypos);
			mvprintw(0, width - 6, "%02d, %02d", height, width);
			if (current_filename)
				mvprintw(0, (width - strlen(current_filename) + 6) / 2, "eztxt - %s", current_filename);
			else
				mvprintw(0, (width - 5) / 2, "eztxt");
			mvchgat(0, 0, -1, A_BOLD, BAR_PAIR, NULL);
			mvprintw(height - 1, (width/2)-HALF_BANNER_WIDTH, "%s", curbanner);
			mvprintw(height - 1, 0, "cpu: %.2fMHz", avg_cpu_mhz);
		}

	}
cleanup_and_end:
	endwin();
	exit(EXIT_SUCCESS);
}

void parse_line(docline* line)
{
	char ch;
	bool in_quotes = false;
	bool in_comment = false;
	bool in_register = false;
	bool in_section = false;
	for (int i = 0; i < strlen(line->line); ++i)
	{
		line->formatting[i] = 0;	// line formatting 1 = error
		ch = line->line[i];
		// comments take precedence
		if (ch == '#')
		{
			in_comment = true;
		}
		if (in_comment)
		{
			line->formatting[i] = COMMENT_PAIR;
			continue;
		}

		// this area is quoted
		if (ch == '\"' && !in_quotes)
		{
			in_quotes = true;
		}
		else if (ch == '\"' && in_quotes)
		{
			line->formatting[i] = QUOTE_PAIR;
			in_quotes = false;
		}

		if (in_quotes)
		{
			line->formatting[i] = QUOTE_PAIR;
			continue;
		}

		if (ch == '.')
			in_section = true;

		if (in_section)
			line->formatting[i] = SECTION_PAIR;

		if (ch == '$')
			in_register = true;

		if (in_register)
			line->formatting[i] = REG_PAIR;

		if (ch == ' ')
		{
			line->formatting[i] = 0;
			in_register = false;
			in_section = false;
		}

		if (ch == ',')
		{
			line->formatting[i] = PUNC_PAIR;
			in_register = false;
			in_section = false;
		}
	}
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
	// TODO: Figure out how scrolling for long documents is going to happen here
	// just to test out quote highlights
	bool in_quotes = false;
	bool used_quote_color = false;
	char ch;
	do 
	{
		// TODO: read parallel color data here and do syntax highlighting if necessary
		// that will mean probably going char by char instead of line by line!
		move(yline, 0);
		// mvprintw(yline, 0, "%s", cur->line);
		bool endflag = false;
		int clrval = 0;
		// int clrcolor = 0;
		// int clrattr = 0 ;
		parse_line(cur);
		bool first = true;
		for (int x = 0; x < strlen(cur->line); ++x)
		{
			ch = cur->line[x];
			if (!first)
			{
				// attroff(COLOR_PAIR(clrcolor));
				// attroff(clrattr);
				// attroff(clrval);
				attroff(COLOR_PAIR(clrval));
			}
			first = false;
			clrval = cur->formatting[x];
			attron(COLOR_PAIR(clrval));
			// clrcolor = clrval & 0x0F;
			// clrattr = (clrval & 0xF0) >> 4;
			// attron(COLOR_PAIR(clrcolor));
			// attron(clrattr);
			mvprintw(yline, x, "%c", ch);
		}
		attroff(COLOR_PAIR(clrval));
		// attroff(COLOR_PAIR(clrattr));
		yline++;
		cur = cur->nextline;
	} while (cur != NULL && yline < max_lines);
	// failsafe for now
	if (used_quote_color)
		attroff(COLOR_PAIR(QUOTE_PAIR));
	refresh();
}

void load_doc(const char* filename, docline** head, docline** tail)
{
	FILE *fptr = fopen(filename, "r");
	if (fptr == NULL)
	{
		// should we have some sort of better error-handling?
		// or just kick out on error?
		printw("\nCannot open %s\n", filename);
		printw("Press any key...\n");
		nodelay(stdscr, false);
		getch();
		endwin();
		exit(EXIT_FAILURE);
	}
	if (current_filename)
		free(current_filename);
	current_filename = strdup(filename);
	printw("Opening file %s", filename);
	refresh();
	docline* lastline = NULL;
	char ch;
	int linelen = 0;
	docline* currline = calloc(1, sizeof(docline));
	if (currline == NULL)
	{
		printw("\nOut of memory, calloc failed.\n");
		printw("Press any key...");
		nodelay(stdscr, false);
		getch();
		endwin();
		exit(EXIT_FAILURE);
	}
	*head = currline;
	for(;;)
	{
		ch = fgetc(fptr);
		printw("%c", ch);
		refresh();
		if (feof(fptr))
		{
			// end of document
			*tail = currline;
			break;
		}
		else if (ch == '\n' || linelen >= 79)
		{
			// create a newline
			linelen = 0;
			lastline = currline;
			currline = calloc(1, sizeof(docline));
			currline->prevline = lastline;
			lastline->nextline = currline;
		}
		else
		{
			currline->line[linelen] = ch;
			++linelen;
		}
	}
	fclose(fptr);
	return;
}

void clear_doc(docline* head)
{
	// free all memoy used by a document
	docline* tmp;
	while (head != NULL)
	{
		tmp = head->nextline;
		free(head);
		head = tmp;
	}
}

void save_doc(docline* head)
{
	printw("SAVING DOC...");
	getch();
}

void do_menu()
{
	printw("MENU...");
	getch();
}