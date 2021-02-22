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
- start refactoring now
- delete/backspace become same function
- add menu in addition to shortcut keys
- longer files and scrolling 
- pressing CTRL or ALT breaks things?!
- add cutline + paste line (or lines?) option
 (this will just be moving things around in
  the linked list!)
- start writing unit tests using check!
- handle tabs properly
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
char debug_msg[MAX_DEBUG_MSG];
int debug_countdown = 0;

void set_debug_msg(const char* msg, ...);
void draw_lines(docline*, int, bool);
void remove_line(docline* line, docline** head, docline** tail);
void clear_doc(docline* head);
void do_menu();
void parse_line(docline* line);
int align_tab(int num);

int main(int argc, char** argv)
{
	initscr();

	if (!has_colors())
	{
		printw("No color terminal found, exiting.");
		printw("Press a key to exit...");
		getch();
		goto cleanup_and_end;	
	}

	wchar_t ch;
	double avg_cpu_mhz = 0.0;
	bool exitFlag = false;
	bool menuFlag = false;
	int xpos = 0, ypos = 0;
	bool had_input = false;
	int width, height;
	int linelen;
	bool screen_clean = true;
	bool unsaved_changes;
	char changes;
	char fname[128];

	cbreak();
	noecho();
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);
	curs_set(0);

	use_default_colors();
	start_color();
	init_pair(CUR_PAIR, COLOR_RED, COLOR_BLACK);
	init_pair(QUOTE_PAIR, COLOR_CYAN, -1);
	init_pair(BAR_PAIR, -1, COLOR_MAGENTA);
	init_pair(COMMENT_PAIR, COLOR_GREEN, - 1);
	init_pair(PUNC_PAIR, COLOR_BLUE, -1);
	init_pair(REG_PAIR, COLOR_YELLOW, -1);
	init_pair(SECTION_PAIR, COLOR_MAGENTA, -1);
	init_pair(KEYWORD_PAIR, COLOR_WHITE, -1);
	init_pair(LABEL_PAIR, COLOR_BLUE, -1);
	init_pair(NUM_PAIR, COLOR_CYAN, -1);
	init_pair(ERROR_PAIR, COLOR_RED, -1);

	int parser_init = init_parser();
	if (parser_init < 0)
	{
		printw("Error initializing parser.\n");
		if (parser_init == -1)
			printw("Can't read keywords.dat\n");
		if (parser_init == -2)
			printw("Can't read pinstrs.dat\n");
		printw("Press a key to exit...");
		getch();
		goto cleanup_and_end;		
	}

	mvprintw(0, 0, "eztxt %d.%d - Tyler Weston - F12 exits - %s", 
		MAJOR_VERSION, MINOR_VERSION, curses_version());
	mvchgat(0, 0, -1, A_BOLD, BAR_PAIR, NULL);
	refresh();

	getmaxyx(stdscr, height, width);

	docline* firstline = calloc(1, sizeof(docline));
	firstline->prevline = NULL;
	firstline->nextline = NULL;
	docline* currline = firstline;
	docline* head;
	docline* tail;

	docline* topline;

	head = firstline;
	tail = firstline;


	clock_t now = clock();

	if (argc != 1)
	{
		clear_doc(head);
		load_doc(argv[1], &head, &tail);
		currline = head;
		draw_lines(head, height - 1, screen_clean);
		find_labels(head);
	}

	topline = head;
	mvchgat(ypos + 1, xpos, 1, A_REVERSE, COLOR_PAIR(CUR_PAIR), NULL);
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

		// ctrl s hangs, so don't use as save
		// case CTRL('s'):	// this doesn' work, actually freezes
		// 	// goto cleanup_and_end;
		// 	// save_doc(head);
		// 	set_debug_msg("ctrl s");
		// 	break;

		case ERR:
			break;

		case CTRL('a'):;
		// todo: roll custom filename getter
			fname[0] = '\0';
			move(height-1, 0);
			printw("Filename: ");
			echo();
			nodelay(stdscr, FALSE);
			getstr(fname);
			noecho();
			nodelay(stdscr, TRUE);
			set_debug_msg("Saving %s", fname);
			// error check saving
			save_doc(fname, head);
			set_debug_msg("Saved %s", fname);
			unsaved_changes = false;
			break;

		case CTRL('x'):
		// cutline
			set_debug_msg("ctrl x");
			break;

		case CTRL('v'):
			set_debug_msg("ctrl v");
			break;

		case CTRL('l'):
			fname[0] = '\0';
			move(height-1, 0);
			printw("Filename:");
			echo();
			nodelay(stdscr, FALSE);
			getstr(fname);
			noecho();
			nodelay(stdscr, TRUE);
			// todo: make set_debug_msg better
			set_debug_msg("Loading %s", fname);
			if (check_file_exists(fname) == 0)
			{
				set_debug_msg("File not found");
				break;
			}

			// TODO: Don't clear doc until we know
			// file has loaded succesfully!
			clear_doc(head);

			load_doc(fname, &head, &tail);
			currline = head;
			draw_lines(head, height - 1, screen_clean);
			set_debug_msg("Loaded %s", fname);
			unsaved_changes = false;
			break;

		// ctrl c breaks, so don't use as copy
		// case CTRL('c'):
		// 	set_debug_msg("ctrl c");
		// 	break;

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
			unsaved_changes = true;
			break;

		case KEY_UP:
			if (ypos == 0)
			{
				if (currline->prevline)
				{
					topline = topline->prevline;
					currline = currline->prevline;
					xpos = min(xpos, strlen(currline->line));					
				}
			}
			else if (currline->prevline)
			{
				currline = currline->prevline;
				xpos = min(xpos, strlen(currline->line));
				--ypos;
			}
			break;

		case KEY_DOWN:
			if (ypos == height - 3)
			{
				if (currline->nextline)
				{
					topline = topline->nextline;
					currline = currline->nextline;
					xpos = min(xpos, strlen(currline->line));
				}
			}
			else if (currline->nextline)
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
			unsaved_changes = true;
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
			unsaved_changes = true;
			break;

		case '\t':;
			// insert TAB_DISTANCE spaces
			int tab_target = TAB_DISTANCE - (xpos % TAB_DISTANCE);
			if (xpos + tab_target < LINE_LENGTH)
			{
				for (int i = LINE_LENGTH; i > xpos + tab_target; --i)
				{
					currline->line[i] = currline->line[i - tab_target];
				}
				for (int i = xpos; i < xpos + tab_target; ++i)
				{
					currline->line[i] = ' ';
				}
				xpos += tab_target;
				unsaved_changes = true;
			}
			break;

		default:	// a typable character
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
			unsaved_changes = true;
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
			// this will go somewhere else!
			clear_labels();
			find_labels(head);

			clear();
			draw_lines(topline, height - 1, screen_clean);
			mvchgat(ypos + 1, xpos, 1, A_REVERSE, COLOR_PAIR(CUR_PAIR), NULL);

			if (had_input)
			{
				if (unsaved_changes)
					changes = 'U';
				else
					changes = ' ';
				mvprintw(0 , 0, "%d, %d %c ", xpos, ypos, changes);
				mvprintw(0, width - 6, "%02d, %02d", height, width);
				
				if (current_filename)
					mvprintw(0, (width - strlen(current_filename) - 6) / 2, "eztxt - %s", current_filename);
				else
					mvprintw(0, (width - 5) / 2, "eztxt");
				mvchgat(0, 0, -1, A_BOLD, BAR_PAIR, NULL);
			}
		}

		if (debug_countdown > 0)
		{
			mvprintw(height - 1, 0, "%s", debug_msg);
		}
		else
		{
			move(height-1, 0);
			clrtoeol();
		}

		mvprintw(height - 1, width - 15, "cpu: %.2fGHz", avg_cpu_mhz);

		// screen update
		if (clock() - now > 500000)
		{
			now = clock();
			avg_cpu_mhz = cpu_info();
			if (debug_countdown > 0)
			{
				--debug_countdown;
			}
		}
	}

cleanup_and_end:
	endwin();
	exit(EXIT_SUCCESS);
}

void set_debug_msg(const char* msg, ...)
{
	//strncpy(debug_msg, msg, MAX_DEBUG_MSG);
	va_list args;
	va_start (args, msg);
	vsprintf(debug_msg, msg, args);
	va_end (args);
	debug_countdown = DISPLAY_DEBUG_TIME;
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

// inline int align(int num) {
// 	// return num rounded up to closest multiple of 4
// 	int remainder = num % TAB_DISTANCE;
//     if (remainder != 0) num += TAB_DISTANCE - remainder;
//     return num;
// }
