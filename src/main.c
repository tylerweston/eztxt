#include "headers/main.h"
#include "headers/util.h"
#include "headers/fileio.h"
#include "headers/parse.h"

/*
*TODO:*
- occasional random segfaults when scrolling around
- insert action (action type, location, character?)
- left/right scrolling
- command line options?
- lots of bug fixes
- unit tests
- command line param to turn off syntax highlighting?
- start refactoring now
- delete/backspace become same function
- add menu in addition to shortcut keys
- pressing CTRL or ALT breaks things?!
- start writing unit tests using check!
- change cursor into a cursorpos struct, then we can have multi-carat + wide selections
*MAYBE TODO:*
- undo/redo functions?
- increase line limit from 80 to an arbitrary amount?
- look into using gap buffers to make editing text more efficient
- regex/word search in document (https://en.wikipedia.org/wiki/Boyer%E2%80%93Moore_string-search_algorithm)?
- fuzzy-word search? (hard?)
- open multiple documents?
- mouse support? (https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/mouse.html)?
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
bool get_string(char* prompt, char* start, char* response, size_t max_size);

void load_document();
void save_document();

// cursor movements
void cursor_left(cursor_pos* cursor);
void cursor_right(cursor_pos* cursor);
void cursor_up(cursor_pos* cursor);
void cursor_down(cursor_pos* cursor);

// cursor actions
void remove_char(cursor_pos* cursor);

// GLOBALS: TODO: Get these outta here! 
size_t width, height;
size_t absy = 0;			// absolute-y position in document
char fname[MAX_FILE_NAME];
docline* currline;
docline* head;
docline* tail;
bool unsaved_changes = false;

// these willbecome command line options
bool show_line_no = true;
bool syntax_highlighting = true;

// top line to display for scrolling
docline* topline;
size_t toplineno = 1;

cursor_pos main_cursor;

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
	//size_t xpos = 0, ypos = 0;	// screen position
	main_cursor.xpos = 0;
	main_cursor.ypos = 0;

	bool had_input = false;

	size_t linelen;
	bool screen_clean = true;

	char changes;


	docline* copy_line = NULL;
	docline* cut_line = NULL;

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
	init_pair(LINE_NO_PAIR, COLOR_WHITE, COLOR_BLACK);

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

	// document lines doubly linked list
	docline* firstline = calloc(1, sizeof(docline));
	firstline->prevline = NULL;
	firstline->nextline = NULL;

	head = firstline;
	tail = firstline;
	main_cursor.currline = firstline;

	clock_t now = clock();

	// TODO: Parse command line here
	if (argc != 1 && check_file_exists(argv[1]))
	{
		// we can move all this stuff to a load file function
		clear_doc(head);
		load_doc(argv[1], &head, &tail);
		main_cursor.currline = head;
		find_labels(head);
	}

	// set topline here in case we loaded a file above
	topline = head;

	// initial cursor
	int absx = main_cursor.xpos;
	if (show_line_no)
		absx += 5;
	
	draw_lines(head, height - 1, screen_clean);
	mvchgat(main_cursor.ypos + 1, absx, 1, A_REVERSE, COLOR_PAIR(CUR_PAIR), NULL);

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
			load_document();
			break;
		}

		// Cut/paste
		case CTRL('k'):		// cut line
		{
			if (cut_line)
				free(cut_line);
			if (copy_line)
				copy_line = NULL;
			cut_line = main_cursor.currline;
			docline* tmp = main_cursor.currline->nextline;
			remove_line(main_cursor.currline, &head, &tail);
			main_cursor.currline = tmp;
			break;
		}

		case CTRL('x'):		// copy line
		{
			copy_line = main_cursor.currline;
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

			newline->nextline = main_cursor.currline;
			newline->prevline = main_cursor.currline->prevline;
			if (main_cursor.currline->prevline)
				main_cursor.currline->prevline->nextline = newline;
			main_cursor.currline->prevline = newline;
			if (main_cursor.currline == head)
				head = newline;
			main_cursor.currline = newline;
			break;
		}

		// ctrl c breaks, so don't use as copy
		// case CTRL('c'):
		// 	set_debug_msg("ctrl c");
		// 	break;

		case KEY_SLEFT:
		{
			size_t oldxpos = main_cursor.xpos;
			while (main_cursor.xpos > 0 && main_cursor.currline->line[main_cursor.xpos] == ' ') --main_cursor.xpos;
			while (main_cursor.xpos > 0 && main_cursor.currline->line[main_cursor.xpos] != ' ') --main_cursor.xpos;
			break;
		}

		case KEY_SRIGHT:
		{
			size_t oldxpos = main_cursor.xpos;
			linelen = strlen(main_cursor.currline->line);
			while (main_cursor.xpos++ < linelen && main_cursor.currline->line[main_cursor.xpos] == ' ');
			while (main_cursor.xpos++ < linelen && main_cursor.currline->line[main_cursor.xpos] != ' ');
			break;
		}

		case KEY_RESIZE:
		{
			getmaxyx(stdscr, height, width);
			break;
		}

		case '\n':
		{	// ENTER key, KEY_ENTER doesn't work?
			docline* newline = calloc(1, sizeof(docline));
			// Handle special case where we are at first character
			// of the line, then we can just insert a new blank line before this
			// one and not worry about the copy we're doing below.
			// if (main_cursor.xpos == 0)
			// {
			// 	// insert before
			// 	newline->prevline = main_cursor.currline->prevline;
			// 	newline->nextline = main_cursor.currline;
			// 	if (main_cursor.currline->prevline)
			// 		main_cursor.currline->prevline->nextline = newline;
			// 	main_cursor.currline->prevline = newline;
			// 	if (head == main_cursor.currline)
			// 		head = newline;
			// 	unsaved_changes = true;
			// 	++main_cursor.ypos;
			// 	++absy;
			// 	break;
			// }
			// if we get here, we aren't special case head of line
			// so we have to move some chars down
			int nindex = 0;
			for (int i = main_cursor.xpos; i < strlen(main_cursor.currline->line); ++i)
			{
				newline->line[nindex] = main_cursor.currline->line[i];
				nindex++;
			}
			newline->line[nindex] = '\0';
			for (int i = strlen(main_cursor.currline->line); i >= main_cursor.xpos ; --i)
			{
				main_cursor.currline->line[i] = '\0';
			}
			newline->prevline = main_cursor.currline;
			newline->nextline = main_cursor.currline->nextline;
			main_cursor.currline->nextline = newline;
			if (tail == main_cursor.currline)
				tail = newline;
			main_cursor.currline = newline;
			
			main_cursor.xpos = 0;
			++main_cursor.ypos;
			++absy;
			unsaved_changes = true;
			break;
		}

		case KEY_UP:
		{
			cursor_up(&main_cursor);
			break;
		}

		case KEY_DOWN:
		{
			cursor_down(&main_cursor);
			break;
		}

		case KEY_LEFT:
		{
			cursor_left(&main_cursor);
			break;
		}

		case KEY_RIGHT:
		{
			cursor_right(&main_cursor);
			break;
		}

		case ESC:
		case KEY_F(12):
		{
			if (unsaved_changes)
			{
			char sure[2];
			get_string ("You have unsaved changes, save?(Y/n)", NULL, sure, 1);
			if ((sure[0] == 'Y' || sure[0] == 'y'))
				save_document();			
			}
			exitFlag = true;
			break;
		}

		case KEY_F(2):
		{
			menuFlag = true;
			break;
		}

		case KEY_BACKSPACE:
		{
			cursor_left(&main_cursor);
			remove_char(&main_cursor);
			break;
		}

		case KEY_DC:
		{
			remove_char(&main_cursor);
			break;
		}

		case '\t':
		{
			// insert TAB_DISTANCE spaces
			int tab_target = TAB_DISTANCE - (main_cursor.xpos % TAB_DISTANCE);
			if (main_cursor.xpos + tab_target < LINE_LENGTH)
			{
				for (size_t i = LINE_LENGTH; i > main_cursor.xpos + tab_target; --i)
				{
					main_cursor.currline->line[i] = main_cursor.currline->line[i - tab_target];
				}
				for (size_t i = main_cursor.xpos; i < main_cursor.xpos + tab_target; ++i)
				{
					main_cursor.currline->line[i] = ' ';
				}
				main_cursor.xpos += tab_target;
				unsaved_changes = true;
			}
			break;
		}

		default:	// a typable character
		{
			// if we're not inserting at back of line, make room
			if (main_cursor.xpos != strlen(main_cursor.currline->line))
			{
				for (size_t i = LINE_LENGTH; i > main_cursor.xpos; --i)
				{
					main_cursor.currline->line[i] = main_cursor.currline->line[i - 1];
				}

			}
			main_cursor.currline->line[main_cursor.xpos] = (char) ch;
			main_cursor.xpos++;
			unsaved_changes = true;
			break;
		}
		}

		if (main_cursor.xpos > 1000)
			set_debug_msg("XPOS is big, error!");
		if (main_cursor.xpos >= strlen(main_cursor.currline->line))
			main_cursor.xpos = strlen(main_cursor.currline->line);
		if (main_cursor.ypos > 1000)
			set_debug_msg("YPOS is big, error!");

		if (menuFlag)
		{
			// not implemented yet
			do_menu();
		}

		if (!screen_clean)
		{
			// this will go somewhere else!
			if (syntax_highlighting)
			{
				clear_labels();
				find_labels(head);
			}

			clear();
			draw_lines(topline, height - 1, screen_clean);
			absx = main_cursor.xpos;
			if (show_line_no)
				absx += 5;
			mvchgat(main_cursor.ypos + 1, absx, 1, A_REVERSE, COLOR_PAIR(CUR_PAIR), NULL);

			if (had_input)
			{
				if (unsaved_changes)
					changes = 'U';
				else
					changes = ' ';
				mvprintw(0 , 0, "%d, %d %c ", (main_cursor.xpos + 1), (absy + 1), changes);
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
	clear_doc(head);
	endwin();
	exit(EXIT_SUCCESS);
}

// functions that we need
// delete 
// insert char

void remove_char(cursor_pos* cursor)
{
	// a few cases to consider when we delete a character
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
	else if (cursor->xpos == strlen(cursor->currline) && cursor->currline->nextline != NULL)
	{

		int lineindex = strlen(cursor->currline->line);
		int newxpos = lineindex;
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
	if (cursor->xpos == 0 && cursor->currline->prevline)
	{
		cursor->currline = cursor->currline->prevline;
		cursor->ypos -= 1;
		--absy;
		cursor->xpos = strlen(cursor->currline->line);
	}
	else if (cursor->xpos > 0)
	{
		cursor->xpos -= 1;	
	}
}

void cursor_right(cursor_pos* cursor)
{
	cursor->xpos++;
	if (cursor->xpos > strlen(cursor->currline->line) && cursor->currline->nextline)
	{
		cursor->currline = cursor->currline->nextline;
		++cursor->ypos;
		++absy;
		cursor->xpos = 0;
	}
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
			// TODO: scrolling not supported yet for line numbers!
			mvprintw(yline, 0, "%03d:", toplineno + yline - 1);	// eww. get rid of -1
			attroff(COLOR_PAIR(LINE_NO_PAIR));
		}
		for (size_t x = 0; x < strlen(cur->line); ++x)
		{
			ch = cur->line[x];
			if (syntax_highlighting)
			{
				if (!first)
				{
					attroff(clrval);
				}
				first = false;
				clrval = cur->formatting[x];
				attron(clrval);
			}
			if (show_line_no)
				absx = x + 5;
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
	if (load_doc(fname, &tmp_head, &tail) == -1)
	{
		set_debug_msg("Error loading %s", fname);
		return;
	}
	clear_doc(head);
	head = tmp_head;
	find_labels(head);
	main_cursor.currline = head;
	topline = head;
	main_cursor.xpos = 0;
	main_cursor.ypos = 0;
	draw_lines(head, height - 1, true);
	set_debug_msg("Loaded %s", fname);
	unsaved_changes = false;
}

// void get_text()
// {
// 	// here, we want to display a prompt
// 	// and fill in a variable
// }

// void insert_char()
// {

// }

// void remove_char()
// {

// }

// void remove_line()
// {

// }

// void add_line()
// {
// 	// ??
// }


// todo: move out of here?
bool get_string(char* prompt, char* start, char* response, size_t max_size)
{
	// display prompt, with default text, and store reponse
	// in response. return false is user presses escape, ie. no input
	// clear any existing debug message
	set_debug_msg("");
	move(height-1, 0);
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
			move(height-1, strlen(prompt) + 2);
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
			if (!(isdigit(ch) || isalpha(ch) || ispunct(ch)))
				break;
			if (resp_index == max_size)
				break;
			resp[resp_index] = ch;
			++resp_index;
			display = true;
			break;
		}
	} while (!done_flag);
	strncpy(response, resp, max_size);
	return true;
}

void clear_doc(docline* head)
{
	// free all memory used by a document
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
	set_debug_msg("Menu...");
	getch();
}

