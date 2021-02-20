#include "headers/main.h"
#include "headers/util.h"
#include "headers/fileio.h"
#include "headers/parse.h"

/*
*TODO:*
- lots of bug fixes
- unit tests
- handle tabs
- saving
- mips keyword syntax highlighting
- cutting and pasting lines
- scrolling long programs
- check return value of load_doc for errors

*MAYBE TODO:*
- undo/redo functions?
- increase line limit from 80
- look into using gap buffers to make editing text more efficient
- regex/word search in document
*/

/*
todo: 
- why is there a 4 in the top corner is using xterm?
- start refactoring now
- delete/backspace become same function
- simple menu including saving files
- longer files and scrolling 
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

char* current_filename = NULL;

int main(int argc, char** argv)
{
	initscr();
	int cur_col = 1;

	if (!has_colors())
	{
		printw("No color terminal found, exiting.");
		printw("Press a key to exit...");
		getch();
		goto cleanup_and_end;	
	}

	use_default_colors();
	start_color();

	init_pair(cur_col, COLOR_RED, COLOR_BLACK);
	init_pair(QUOTE_PAIR, COLOR_CYAN, -1);
	init_pair(BAR_PAIR, -1, COLOR_MAGENTA);
	init_pair(COMMENT_PAIR, COLOR_GREEN, - 1);
	init_pair(PUNC_PAIR, COLOR_BLUE, -1);
	init_pair(REG_PAIR, COLOR_YELLOW, -1);
	init_pair(SECTION_PAIR, COLOR_MAGENTA, -1);
	init_pair(KEYWORD_PAIR, COLOR_WHITE, -1);

	mvprintw(0, 0, "eztxt %d.%d - Tyler Weston - F12 exits - %s", 
		MAJOR_VERSION, MINOR_VERSION, curses_version());
	mvchgat(0, 0, -1, A_BOLD, BAR_PAIR, NULL);
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
	bool had_input = false, clear_once = true;
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
		draw_lines(head, height - 1, screen_clean);
	}

	mvchgat(ypos + 1, xpos, 1, A_REVERSE, COLOR_PAIR(cur_col), NULL);
	while(!exitFlag)
	{

		screen_clean = true;
		ch = getch();
		if (ch!=ERR) {
			had_input = true;
			screen_clean = false;
		} 

		switch (ch)
		{

		// case CTRL('s'):	// this doesn' work, actually freezes
		// 	goto cleanup_and_end;
		// 	save_doc(head);
		// 	break;

		case ERR:
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
			clear_once = true;
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
			if ((size_t)xpos > strlen(currline->line) && currline->nextline)
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

		if (!screen_clean)
		{
			clear();
			draw_lines(head, height - 1, screen_clean);
			mvchgat(ypos + 1, xpos, 1, A_REVERSE, COLOR_PAIR(cur_col), NULL);

			if (had_input)
			{
				mvprintw(0 , 0, "%d, %d  ", xpos, ypos);
				mvprintw(0, width - 6, "%02d, %02d", height, width);
				
				if (current_filename)
					mvprintw(0, (width - strlen(current_filename) + 6) / 2, "eztxt - %s", current_filename);
				else
					mvprintw(0, (width - 5) / 2, "eztxt");
				mvchgat(0, 0, -1, A_BOLD, BAR_PAIR, NULL);
			}
		}

		mvprintw(height - 1, 0, "cpu: %.2fMHz", avg_cpu_mhz);

		// screen update
		if (clock() - now > 500000)
		{
			now = clock();
			avg_cpu_mhz = cpu_info();
			scrolltick = 0;
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
	// clear();
	// TODO: Figure out how scrolling for long documents is going to happen here
	// just to test out quote highlights
	char ch;
	do 
	{
		move(yline, 0);
		clrtoeol();
		int clrval = 0;
		parse_line(cur);
		bool first = true;
		for (int x = 0; x < strlen(cur->line); ++x)
		{
			ch = cur->line[x];
			if (!first)
			{
				attroff(clrval);
			}
			first = false;
			clrval = cur->formatting[x];
			attron(clrval);
			mvprintw(yline, x, "%c", ch);
		}
		attroff(clrval);
		yline++;
		cur = cur->nextline;
	} while (cur != NULL && yline < max_lines);
	refresh();
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



void do_menu()
{
	printw("MENU...");
	getch();
}