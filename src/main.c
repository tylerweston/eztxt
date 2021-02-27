/*
mipsze, text editor with syntax highlighting for mips assembly.

Copyright 2021 Tyler Weston

Permission is hereby granted, free of charge, to any person obtaining a copy 
of this software and associated documentation files (the "Software"), to deal 
in the Software without restriction, including without limitation the rights 
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
of the Software, and to permit persons to whom the Software is furnished to do 
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION 
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
*TODO:*
- Fix warning at 766!
comparison of unsigned expression in ‘>= 0’ is always true (if (cursor->width + cursor->xpos - 1 >= 0))
- but cursor->width should be signed? it is just an int? hmmm.

- create some sort of 'screen' data structure that holds things like width, height, etc. etc?
	- then we might be able to do things like split/load multiple docs
- multicarat over a spot that alreay has a carat shouldn't add a new one!
	- we'll need to "compress" the carats here? or maybe have an "active" flag or something?
- problem with byobu terminal and width of line number printing
- move the OTHER carat when multi-carating (The WIDE line should stay where it was!)
- moving topline up/down always does the same two things
	let's roll them into one function
- newline on last line should scroll down
- deleting on first line should scroll up
- insert action (action type, location, character?)
- lots of bug fixes
- unit tests
- add menu in addition to shortcut keys?
- pressing CTRL or ALT breaks things?!
- start writing unit tests using check!
*MAYBE TODO:*
- undo/redo functions?
- increase line limit from 80 to an arbitrary amount?
- look into using gap buffers to make editing text more efficient?
- regex/word search in document (https://en.wikipedia.org/wiki/Boyer%E2%80%93Moore_string-search_algorithm)?
- fuzzy-word search? (hard?)
- open multiple documents?
- mouse support? (https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/mouse.html)?
*/

#include "headers/main.h"
#include "headers/util.h"
#include "headers/fileio.h"
#include "headers/parse.h"


static void parse_args(int argc, char* argv[], char ** filename);
static bool get_string(char* prompt, char* start, char* response, size_t max_size);
static void set_leading_zeros();
static void cleanup_and_end();
static void show_version_msg();

// document functions
static void check_unsaved_changes();
static void load_document();
static void save_document();
static void initialize_doc();

// line editing
static void draw_lines(docline*);
static void remove_line(docline* line, docline** head, docline** tail);
static void clear_doc(docline* head);

static void draw_cursors();

// cursor movements
static void cursor_left(cursor_pos* cursor);
static void cursor_right(cursor_pos* cursor);
static void cursor_up(cursor_pos* cursor);
static void cursor_down(cursor_pos* cursor);

// cursor actions
static void remove_char(cursor_pos* cursor);
static void insert_newline(cursor_pos* cursor);
static void insert_tab(cursor_pos* cursor);
static void insert_character(cursor_pos* cursor, char ch);
static void extend_cursor_left(cursor_pos* cursor);
static void extend_cursor_right(cursor_pos* cursor);


// GLOBALS: TODO: Get these outta here!
size_t width, height;
size_t absy = 0;			// absolute-y position in document

char fname[MAX_FILE_NAME];
char* current_filename = NULL;
char* to_load = NULL;

docline* currline = NULL;
docline* head = NULL;
docline* tail = NULL;
docline* copy_line = NULL;
docline* cut_line = NULL;
bool unsaved_changes = false;

int est_number_lines = 1;
int leading_zeros = 0;

// these will become command line options
bool show_line_no = true;
bool syntax_highlighting = true;
bool show_help = false;
bool show_version = false;

// top line to display for scrolling
docline* topline;
size_t toplineno = 1;
size_t leftmostdigitno = 0;

// cursors
cursor_pos cursors[MAX_CURSORS] = {0};
int num_cursors = 1;

char debug_msg[MAX_DEBUG_MSG];
int debug_countdown = 0;

bool had_input = false;
bool exitFlag = false;

int main(int argc, char** argv)
{
	initscr();

	if (!has_colors())
	{
		printw("No color terminal found, exiting.");
		printw("Press a key to exit...");
		getch();
		cleanup_and_end();
	}

	wchar_t ch;
	double avg_cpu_mhz = 0.0;

	bool screen_clean = true;
	char changes;

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
	init_pair(ERROR_BLOCK_PAIR, -1, COLOR_RED);
	init_pair(LINE_NO_PAIR, COLOR_WHITE, COLOR_BLACK);
	init_pair(MACRO_PARAM_PAIR, COLOR_YELLOW, -1);

	if (argc > 1)
		parse_args(argc, argv, &to_load);

	if (syntax_highlighting)
	{
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
			cleanup_and_end();
		}
	}

	mvprintw(0, 0, "mipze ver.%d.%d.%d - Tyler Weston - F12 exits - %s",
	         MAJOR_VERSION, MINOR_VERSION, BUILD_VERSION, curses_version());
	mvchgat(0, 0, -1, 0, BAR_PAIR, NULL);
	refresh();

	getmaxyx(stdscr, height, width);

	// Start our timer clock
	clock_t now = clock();

	initialize_doc();

	if (to_load != NULL && check_file_exists(to_load))
	{
		// we can move all this stuff to a load file function
		// clear_doc(head);
		load_doc(to_load, &head, &tail);
		cursors[0].currline = head;
		find_labels(head);
		free(to_load);
	}

	// set topline here in case we loaded a file above
	topline = head;

	draw_lines(head);
	draw_cursors();

	while (!exitFlag)
	{

		screen_clean = true;
		ch = getch();
		if (ch != ERR) {
			had_input = true;
			screen_clean = false;
		}

		switch (ch)
		{

		case ERR:
			break;

		// Document contols
		case CTRL('a'):		// save file
		{
			save_document();
			break;
		}

		case CTRL('l'):		// load a file
		{
			check_unsaved_changes();
			load_document();
			break;
		}

		case CTRL('n'):		// new document
		{
			check_unsaved_changes();
			clear_doc(head);
			initialize_doc();
			break;
		}

		// Cut/paste
		case CTRL('k'):		// cut line
		{
			if (cut_line)
				free(cut_line);
			if (copy_line)
				copy_line = NULL;
			cut_line = cursors[0].currline;
			docline* tmp = cursors[0].currline->nextline;
			remove_line(cursors[0].currline, &head, &tail);
			cursors[0].currline = tmp;
			break;
		}

		case CTRL('x'):		// copy line
		{
			copy_line = cursors[0].currline;
			if (cut_line)
				free(cut_line);
			break;
		}

		case CTRL('v'):		// paste line
		{
			if (!(cut_line || copy_line))
				break;
			set_debug_msg("paste line");
			docline* newline = calloc(1, sizeof(docline));
			docline* source;
			if (cut_line)
				source = cut_line;
			else
				source = copy_line;
			strncpy(newline->line, source->line, LINE_LENGTH);	// needs to be -1?

			newline->nextline = cursors[0].currline;
			newline->prevline = cursors[0].currline->prevline;
			if (cursors[0].currline->prevline)
				cursors[0].currline->prevline->nextline = newline;
			cursors[0].currline->prevline = newline;
			if (cursors[0].currline == head)
				head = newline;
			cursors[0].currline = newline;
			break;
		}

		// ctrl c breaks, so don't use as copy
		// case CTRL('c'):
		// 	set_debug_msg("ctrl c");
		// 	break;

		case CTRL('y'): // new carat up
			if (num_cursors == MAX_CURSORS)
				break;
			cursors[num_cursors].width = 1;
			cursors[num_cursors].xpos = cursors[0].xpos;
			cursors[num_cursors].ypos = cursors[0].ypos;
			cursors[num_cursors].currline = cursors[0].currline;
			cursor_up(&cursors[0]);
			if (!(cursors[num_cursors].xpos == cursors[0].xpos &&
			        cursors[num_cursors].ypos == cursors[0].ypos))
				++num_cursors;
			break;

		case CTRL('h'):	// new carat down
			if (num_cursors == MAX_CURSORS)
				break;
			cursors[num_cursors].width = 1;
			cursors[num_cursors].xpos = cursors[0].xpos;
			cursors[num_cursors].ypos = cursors[0].ypos;
			cursors[num_cursors].currline = cursors[0].currline;
			cursor_down(&cursors[0]);
			if (!(cursors[num_cursors].xpos == cursors[0].xpos &&
			        cursors[num_cursors].ypos == cursors[0].ypos))
				++num_cursors;
			break;

		case CTRL('u'): // remove extra carats
			num_cursors = 1;
			break;



		case KEY_SLEFT:
		{
			// for (int i = 0; i < num_cursors; ++i)
			// {
			// 	while (cursors[i].xpos > 0 && cursors[i].currline->line[cursors[i].xpos] == ' ') --cursors[i].xpos;
			// 	while (cursors[i].xpos > 0 && cursors[i].currline->line[cursors[i].xpos] != ' ') --cursors[i].xpos;
			// }
			for (int i = 0; i < num_cursors; ++i)
			{
				cursor_left(&cursors[i]);
				extend_cursor_right(&cursors[i]);
			}
			break;
		}

		case KEY_SRIGHT:
		{
			// for (int i = 0; i < num_cursors; ++i)
			// {
			// 	linelen = strlen(cursors[i].currline->line);
			// 	while (cursors[i].xpos < linelen && cursors[i].currline->line[cursors[i].xpos] == ' ') ++cursors[i].xpos;
			// 	while (cursors[i].xpos < linelen && cursors[i].currline->line[cursors[i].xpos] != ' ') ++cursors[i].xpos;
			// }
			for (int i = 0; i < num_cursors; ++i)
			{
				cursor_right(&cursors[i]);
				extend_cursor_left(&cursors[i]);
			}
			break;
		}

		case KEY_RESIZE:
		{
			getmaxyx(stdscr, height, width);
			break;
		}

		case '\n':
		{	// ENTER key, KEY_ENTER doesn't work?
			for (int i = 0; i < num_cursors; ++i)
				insert_newline(&cursors[i]);
			break;
		}

		case KEY_UP:
		{
			for (int i = 0; i < num_cursors; ++i)
			{
				cursors[i].width = 1;
				cursor_up(&cursors[i]);
			}
			break;
		}

		case KEY_DOWN:
		{
			for (int i = 0; i < num_cursors; ++i)
			{
				cursors[i].width = 1;
				cursor_down(&cursors[i]);
			}
			break;
		}

		case KEY_LEFT:
		{
			for (int i = 0; i < num_cursors; ++i)
			{
				cursors[i].width = 1;
				cursor_left(&cursors[i]);
			}
			break;
		}

		case KEY_RIGHT:
		{
			for (int i = 0; i < num_cursors; ++i)
			{
				cursors[i].width = 1;
				cursor_right(&cursors[i]);
			}
			break;
		}

		case ESC:		// exit, check for unsaved changes
		case KEY_F(12):
		{
			check_unsaved_changes();
			exitFlag = true;
			break;
		}

		// case KEY_F(2):
		// {
		// 	menuFlag = true;
		// 	break;
		// }

		case KEY_BACKSPACE:
		{
			for (int i = 0; i < num_cursors; ++i)
			{
				cursor_left(&cursors[i]);
				remove_char(&cursors[i]);
			}
			break;
		}

		case KEY_DC:
		{
			for (int i = 0; i < num_cursors; ++i)
				remove_char(&cursors[i]);
			break;
		}

		case '\t':
		{
			for (int i = 0; i < num_cursors; ++i)
				insert_tab(&cursors[i]);
			break;
		}

		default:	// a typable character
		{
			for (int i = 0; i < num_cursors; ++i)
				insert_character(&cursors[i], (char) ch);
			break;
		}
		}

		// if (menuFlag)
		// {
		// 	// not implemented yet
		// 	// do_menu();
		// }

		if (!screen_clean)
		{
			// this will go somewhere else!
			if (syntax_highlighting)
			{
				clear_macros();
				clear_labels();
				find_labels(head);
			}

			clear();
			draw_lines(topline);
			draw_cursors();

			if (had_input)	// just so we can show the title bar until a key is pressed? find a better way.
			{
				if (unsaved_changes)
					changes = 'U';
				else
					changes = ' ';
				mvprintw(0 , 0, "%d, %d %c ", (cursors[0].xpos + 1), (absy + 1), changes);
				mvprintw(0, width - 6, "%02d, %02d", height, width);

				if (current_filename)
					mvprintw(0, (width - strlen(current_filename) - 6) / 2, "mipsze - %s", current_filename);
				else
					mvprintw(0, (width - 5) / 2, "mipsze");
				mvchgat(0, 0, -1, A_BOLD, BAR_PAIR, NULL);
			}
		}

		if (debug_countdown > 1)
		{
			mvprintw(height - 1, 0, "%s", debug_msg);
		}
		else if (debug_countdown == 1)
		{
			move(height - 1, 0);
			clrtoeol();
		}

		mvprintw(height - 1, width - 15, "cpu: %.2fGHz", avg_cpu_mhz);

		// screen update
		if (clock() - now > 250000)
		{
			now = clock();
			avg_cpu_mhz = cpu_info();
			if (debug_countdown > 0)
			{
				--debug_countdown;
			}
		}
	}
	cleanup_and_end();
}

static void show_version_msg()
{
	// since this will be AFTER we've shut down curses stuff (?)
	// we'll just printf this info
	printf("mipze ver.%d.%d.%d - Tyler Weston\nUnder MIT License 2021\n",
	         MAJOR_VERSION, MINOR_VERSION, BUILD_VERSION);
}

static void cleanup_and_end()
{
	clear_doc(head);
	endwin();
	if (show_version)
		show_version_msg();
	exit(EXIT_SUCCESS);
}

void draw_cursors()
{
	int absx;
	for (int i = 0; i < num_cursors; ++i)
	{
		absx = cursors[i].xpos;
		if (show_line_no)
			absx += leading_zeros + 3;
		if (cursors[i].width > 0)
			mvchgat(cursors[i].ypos + 1, absx, cursors[i].width, A_REVERSE, COLOR_PAIR(CUR_PAIR), NULL);
		else if (cursors[i].width < 0)
			mvchgat(cursors[i].ypos + 1, absx + cursors[i].width, -cursors[i].width + 1, A_REVERSE, COLOR_PAIR(CUR_PAIR), NULL);

	}
}

void check_unsaved_changes()
{
	if (unsaved_changes)
	{
		char sure[2];
		get_string ("You have unsaved changes, save?(Y/n)", NULL, sure, 1);
		if ((sure[0] == 'Y' || sure[0] == 'y'))
			save_document();
	}
}

void insert_tab(cursor_pos* cursor)
{
	// insert TAB_DISTANCE spaces
	int tab_target = TAB_DISTANCE - (cursor->xpos % TAB_DISTANCE);
	if (cursor->xpos + tab_target < LINE_LENGTH)
	{
		for (size_t i = LINE_LENGTH; i > cursor->xpos + tab_target; --i)
		{
			cursor->currline->line[i] = cursor->currline->line[i - tab_target];
		}
		for (size_t i = cursor->xpos; i < cursor->xpos + tab_target; ++i)
		{
			cursor->currline->line[i] = ' ';
		}
		cursor->xpos += tab_target;
		unsaved_changes = true;
	}
}

void insert_character(cursor_pos* cursor, char ch)
{
	if (!(isalpha(ch) || isdigit(ch) || ispunct(ch) || ch == ' '))
		return;
	// if we're not inserting at back of line, make room
	if (cursor->xpos != strlen(cursor->currline->line))
	{
		for (size_t i = LINE_LENGTH; i > cursor->xpos; --i)
		{
			cursor->currline->line[i] = cursor->currline->line[i - 1];
		}

	}
	cursor->currline->line[cursor->xpos] = ch;
	cursor->xpos++;
	unsaved_changes = true;
}

void insert_newline(cursor_pos* cursor)
{
	docline* newline = calloc(1, sizeof(docline));
	// Handle special case where we are at first character
	// of the line, then we can just insert a new blank line before this
	// one and not worry about the copy we're doing below.
	if (cursor->xpos == 0)
	{
		// insert before
		if (head == cursor->currline)
		{
			if (topline == head)	// hmm, having to do this is obnoxious?
				topline = newline;
			head = newline;
		}
		newline->prevline = cursor->currline->prevline;
		newline->nextline = cursor->currline;
		if (cursor->currline->prevline)
			cursor->currline->prevline->nextline = newline;
		cursor->currline->prevline = newline;
		goto finish_scroll_down;
	}

	// if we get here, we aren't special case head of line, ie cursor->xpos!=0
	// so we have to move some chars down to the next line.
	int nindex = 0;
	for (size_t i = cursor->xpos; i < strlen(cursor->currline->line); ++i)
	{
		newline->line[nindex] = cursor->currline->line[i];
		nindex++;
	}
	newline->line[nindex] = '\0';
	for (size_t i = strlen(cursor->currline->line); i >= cursor->xpos ; --i)
	{
		cursor->currline->line[i] = '\0';
	}
	newline->prevline = cursor->currline;
	newline->nextline = cursor->currline->nextline;
	cursor->currline->nextline = newline;
	if (tail == cursor->currline)
		tail = newline;
	cursor->currline = newline;
	cursor->xpos = 0;

finish_scroll_down:
	// todo: scroll if we're at bottom of the screen!
	++cursor->ypos;
	++absy;
	// cursor_down(cursor);	// just calling this here DOES NOT WORK
	unsaved_changes = true;
	++est_number_lines;
	set_leading_zeros();
}

void remove_char(cursor_pos* cursor)
{
	// TODO: Consider deleting an entire BLOCK of text now if the width
	// of this cursor is greater than 1!
	// TODO: There is still some problems here somewhere, sometimes it becomes
	// unaligned, what are the cases when this happens?
	// a few cases to consider when we delete a character
	// TODO: What if we are a multicarat that is on a line that gets deleted?
	if (cursor->xpos == 0 && cursor->currline->line[0] == '\0')
	{
		if (cursor->currline == head && cursor->currline->nextline == NULL)
		{
			// empty document, nothing to do here
			return;
		}
		docline* tmp;
		if (cursor->currline == tail)
		{
			tmp = cursor->currline->prevline;
			--cursor->ypos;
			--absy;
		}
		else
		{
			tmp = cursor->currline->nextline;
		}
		remove_line(cursor->currline, &head, &tail);
		cursor->currline = tmp;
	}
	else if (cursor->xpos == strlen(cursor->currline->line) && cursor->currline->nextline != NULL)
	{

		int lineindex = strlen(cursor->currline->line);
		for (size_t i = 0; i < strlen(cursor->currline->nextline->line); ++i)
		{
			// error check this
			cursor->currline->line[lineindex++] = cursor->currline->nextline->line[i];
			if (lineindex >= 80) lineindex = 80;
		}
		remove_line(cursor->currline->nextline, &head, &tail);
	}
	else
	{
		for (int i = cursor->xpos; i < LINE_LENGTH; ++i)
		{
			cursor->currline->line[i] = cursor->currline->line[i + 1];
		}
	}
	unsaved_changes = true;
}

void cursor_left(cursor_pos* cursor)
{
	if (cursor->xpos == 0 && leftmostdigitno > 0)
	{
		--leftmostdigitno;
	}
	else if (cursor->xpos == 0 && cursor->currline->prevline && leftmostdigitno == 0)
	{
		cursor->currline = cursor->currline->prevline;
		if (cursor->ypos != 0)
		{
			--cursor->ypos;
			--absy;
		}
		else
		{
			topline = topline->prevline;
			--toplineno;
			--absy;
		}
		cursor->xpos = strlen(cursor->currline->line);
	}
	else if (cursor->xpos > 0)
	{
		cursor->xpos -= 1;
	}
}

void cursor_right(cursor_pos* cursor)
{
	// consider line numbers being used here
	// -4 seems icky and arbitrary, fix this with a notion of "screen" struct
	// that can track this stuff
	size_t scrolling_width = show_line_no ? width - leading_zeros - 4 : width;
	if (cursor->xpos == scrolling_width && cursor->xpos <= strlen(cursor->currline->line))
	{
		++leftmostdigitno;
	}
	else if (cursor->xpos <= strlen(cursor->currline->line))
	{
		cursor->xpos++;
	}
	else if (cursor->xpos > strlen(cursor->currline->line) && cursor->currline->nextline)
	{
		cursor->currline = cursor->currline->nextline;
		++cursor->ypos;
		++absy;
		cursor->xpos = 0;
	}
	if (leftmostdigitno > LINE_LENGTH - width)
		leftmostdigitno = LINE_LENGTH - width;
}

void extend_cursor_right(cursor_pos* cursor)
{
	if (cursor->xpos + cursor->width + 1 <= strlen(cursor->currline->line))
	{
		++cursor->width;
	}
	if (cursor->width == 0)
		cursor->width = 1;
}

void extend_cursor_left(cursor_pos* cursor)
{
	if (cursor->width + cursor->xpos - 1 >= 0)
	{
		--cursor->width;
	}
	if (cursor->width == 0)
		cursor->width = -1;	
}

void cursor_down(cursor_pos* cursor)
{
	if (cursor->ypos == height - 3)
	{
		if (cursor->currline->nextline)
		{
			topline = topline->nextline;
			cursor->currline = cursor->currline->nextline;
			cursor->xpos = min(cursor->xpos, strlen(cursor->currline->line));
			++absy;
			++toplineno;
		}
	}
	else if (cursor->currline->nextline)
	{
		cursor->currline = cursor->currline->nextline;
		cursor->xpos = min(cursor->xpos, strlen(cursor->currline->line));
		++cursor->ypos;
		++absy;
	}
}

void cursor_up(cursor_pos* cursor)
{
	if (cursor->ypos == 0)
	{
		if (cursor->currline->prevline)
		{
			topline = topline->prevline;
			cursor->currline = cursor->currline->prevline;
			cursor->xpos = min(cursor->xpos, strlen(cursor->currline->line));
			--toplineno;
			--absy;
		}
	}
	else if (cursor->currline->prevline)
	{
		cursor->currline = cursor->currline->prevline;
		cursor->xpos = min(cursor->xpos, strlen(cursor->currline->line));
		if (cursor->ypos > 0)
			--cursor->ypos;
		--absy;
	}
}

void set_debug_msg(const char* msg, ...)
{
	va_list args;
	va_start (args, msg);
	vsprintf(debug_msg, msg, args);
	va_end (args);
	debug_countdown = DISPLAY_DEBUG_TIME;
	mvprintw(height - 1, 0, "%s", debug_msg);
	refresh();
}


void remove_line(docline* line, docline** head, docline** tail)
{
	if (*head == *tail && *head == line)
		return;
	--est_number_lines;
	set_leading_zeros();
	if (*head == line)
		*head = line->nextline;
	if (*tail == line)
		*tail = line->prevline;
	if (line->nextline != NULL)
		line->nextline->prevline = line->prevline;
	if (line->prevline != NULL)
		line->prevline->nextline = line->nextline;
}

void draw_lines(docline* top)
{
	int max_lines = height - 1;
	docline* cur = top;
	int yline = 1;
	char ch;
	do
	{
		move(yline, 0);
		clrtoeol();
		int clrval = 0;
		if (syntax_highlighting)
			parse_line(cur);
		bool first = true;
		int absx = 0;
		if (show_line_no)
		{
			attron(COLOR_PAIR(LINE_NO_PAIR));
			mvprintw(yline, 0, "%*lu: ", (leading_zeros + 1), toplineno + yline - 1);
			attroff(COLOR_PAIR(LINE_NO_PAIR));
		}
		for (size_t x = 0; x < strlen(cur->line); ++x)
		{
			ch = cur->line[x + leftmostdigitno];
			if (syntax_highlighting)
			{
				if (!first)
				{
					attroff(clrval);
				}
				first = false;
				clrval = cur->formatting[x + leftmostdigitno];
				attron(clrval);
			}
			if (show_line_no)
				absx = x + leading_zeros + 3;
			else
				absx = x;
			mvprintw(yline, absx, "%c", ch);
		}
		if (syntax_highlighting)
			attroff(clrval);
		yline++;
		cur = cur->nextline;
	} while (cur != NULL && yline < max_lines);
	refresh();
}

void save_document()
{
	memset(fname, 0, MAX_FILE_NAME);
	if (!get_string("Save file", current_filename, fname, MAX_FILE_NAME))
		return;
	// error check saving
	if (check_file_exists(fname) != 0)
	{
		char sure[2];
		get_string ("File exists! Overwrite? (Y/n)", NULL, sure, 1);
		if (!(sure[0] == 'Y' || sure[0] == 'y'))
			return;
	}
	set_debug_msg("Saving %s", fname);
	save_doc(fname, head);
	if (current_filename)
		free(current_filename);
	current_filename = strdup(fname);
	set_debug_msg("Saved %s", fname);
	unsaved_changes = false;
}

void load_document()
{
	memset(fname, 0, MAX_FILE_NAME);
	if (!get_string("Load file", NULL, fname, MAX_FILE_NAME))
		return;
	set_debug_msg("Loading %s", fname);

	if (check_file_exists(fname) == 0)
	{
		set_debug_msg("File not found");
		return;
	}

	// TODO: Don't clear doc until we know
	// file has loaded succesfully?
	docline* tmp_head;

	// return a value from load_doc. make load_doc take a temp head and if
	// it succeeds, clear the old doc and set the new head to the temp head
	int est_number_lines = load_doc(fname, &tmp_head, &tail); 
	if (est_number_lines == -1)
	{
		set_debug_msg("Error loading %s", fname);
		est_number_lines = 1;
		return;
	}

	clear_doc(head);
	head = tmp_head;
	find_labels(head);
	cursors[0].currline = head;
	topline = head;
	cursors[0].xpos = 0;
	cursors[0].ypos = 0;
	num_cursors = 1;
	set_leading_zeros();
	draw_lines(head);
	set_debug_msg("Loaded %s", fname);
	unsaved_changes = false;
}

void wait_for_keypress()
{
	wchar_t ch;
	for (;;)
	{
		ch = getch();
		if (ch != ERR)
			break;
	}
}

// todo: move out of here?
static bool get_string(char* prompt, char* start, char* response, size_t max_size)
{
	// display prompt, with default text, and store reponse
	// in response. return false is user presses escape, ie. no input
	// clear any existing debug message
	set_debug_msg("");
	move(height - 1, 0);
	clrtoeol();
	printw("%s: ", prompt);
	char resp[max_size + 1];
	memset(resp, 0, max_size + 1);
	if (start)
		strcpy(resp, start);
	wchar_t ch;
	size_t resp_index;
	if (start)
		resp_index = strlen(start);
	else
		resp_index = 0;
	bool done_flag = false;
	bool display = true;
	do
	{
		if (display)
		{
			move(height - 1, strlen(prompt) + 2);
			clrtoeol();
			printw("%s_", resp);
			display = false;
		}
		ch = getch();
		switch (ch)
		{
		case ERR:
			break;
		case '\n':
			resp[resp_index] = '\0';
			done_flag = true;
			break;
		case KEY_BACKSPACE:
			if (resp_index == 0)
				break;
			--resp_index;
			resp[resp_index] = '\0';
			display = true;
			break;
		case ESC:
			*response = 0;
			return false;
		default:
			if (!(isdigit(ch) || isalpha(ch) || ispunct(ch) || ch == ' '))
				break;
			if (resp_index == max_size)
				break;
			resp[resp_index] = ch;
			++resp_index;
			display = true;
			if (max_size == 1 && 
				(resp[0] == 'Y' || resp[0] == 'y' || resp[0] == 'N' || resp[0] == 'n'))
					done_flag = true;	// immediate break if we're a single char
			break;
		}
	} while (!done_flag);
	strncpy(response, resp, max_size);
	return true;
}

static void initialize_doc()
{
	// create a new empty document
	exitFlag = false;

	copy_line = NULL;
	cut_line = NULL;

	// create a single empty line to begin with
	// document lines doubly linked list
	docline* firstline = calloc(1, sizeof(docline));
	firstline->prevline = NULL;
	firstline->nextline = NULL;

	head = firstline;
	tail = firstline;

	cursors[0].currline = head;
	topline = head;
	cursors[0].xpos = 0;
	cursors[0].ypos = 0;
	cursors[0].width = 1;
	num_cursors = 1;

	free(current_filename);
	current_filename = NULL;

	set_leading_zeros();
}

static void clear_doc(docline* head)
{
	// free all memory used by a document
	docline* tmp;
	while (head != NULL)
	{
		tmp = head->nextline;
		free(head);
		head = tmp;
	}
	num_cursors = 0;
}

static void set_leading_zeros()
{
	if (est_number_lines < 10)
		leading_zeros = 0;
	else if (est_number_lines < 100)
		leading_zeros = 1;
	else if (est_number_lines < 1000)
		leading_zeros = 2;
	else if (est_number_lines < 10000)
		leading_zeros = 3;
	else
		leading_zeros = 4;
	// if we have bigger docs, we're in trouble? We can just use log10 to figure this out
	// if we really want to? Tackle later.
}

static void parse_args(int argc, char* argv[], char ** filename)
{
	int opt;
	int option_index = 0;
	static const char* arg_flags = "hvns";
	static struct option long_options[] =
	{
		{"help",					no_argument,		0, 'h'},
		{"version",					no_argument,		0, 'v'},
		{"no-line-numbers",			no_argument,		0, 'n'},
		{"no-syntax-highlighting",	no_argument,		0, 's'},
		{0,							0,					0,	0}
	};

	while (true)
	{
		opt = getopt_long (argc, argv, arg_flags,
			long_options, &option_index);

		if (opt == -1)
			break;

		switch (opt)
		{
		case 'n':
			show_line_no = false;
			break;
		case 's':
			syntax_highlighting = false;
			break;
		case 'h':
			show_help = true;
			break;
		case 'v':
			show_version = true;
			break;
		default:
			break;
		}
	}

	*filename = strdup(argv[argc - 1]);
	if (filename == NULL || *filename[0] == '\0' ||
		*filename[0] == '-')
		filename = NULL;
}