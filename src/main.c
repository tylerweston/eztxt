#include <curses.h>	// include stdio
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <time.h>


/*
todo: 
- start refactoring now
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

char* current_filename = NULL;

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
	else
	{
		printw("No color terminal found, exiting.");
		printw("Press a key to exit...");
		getch();
		goto cleanup_and_end;
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

	docline* firstline = malloc(sizeof(docline));
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
				int nindex = 0;
				for (int i = xpos; i < strlen(currline->line); ++i)
				{
					newline->line[nindex] = currline->line[i];
					nindex++;
				}
				newline->line[nindex] = '\0';
				currline->line[xpos] = '\0';
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

		if (menuFlag)
		{
			do_menu();
		}

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
			draw_lines(head, 20, screen_clean);
			mvchgat(ypos + 1, xpos, 1, A_BLINK, COLOR_PAIR(cur_col), NULL);

			mvprintw(0 , 0, "%d, %d", xpos, ypos);
			mvprintw(0, width - 6, "%02d, %02d", height, width);
			if (current_filename)
				mvprintw(0, (width - strlen(current_filename)) / 2, current_filename);
			mvprintw(height - 1, (width/2)-HALF_BANNER_WIDTH, "%s", curbanner);
		
		}

	}
cleanup_and_end:
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

void load_doc(const char* filename, docline** head, docline** tail)
{
	FILE *fptr = fopen(filename, "r");
	if (fptr == NULL)
	{
		printw("Cannot open %s", filename);
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
	docline* currline = malloc(sizeof(docline));
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
			currline = malloc(sizeof(docline));
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