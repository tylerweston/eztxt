#include "headers/main.h"
#include "headers/util.h"
#include "headers/fileio.h"
#include "headers/parse.h"

/*
*TODO:*
- try to load file with unsaved changes, prompt for save first
- insert action (action type, location, character?)
- left/right scrolling
- command line options? -line nums, syntax highlighting
- lots of bug fixes
- unit tests
- add menu in addition to shortcut keys?
- pressing CTRL or ALT breaks things?!
- start writing unit tests using check!
- there is an extra newline at the end of saved files
*MAYBE TODO:*
- undo/redo functions?
- increase line limit from 80 to an arbitrary amount?
- look into using gap buffers to make editing text more efficient
- regex/word search in document (https://en.wikipedia.org/wiki/Boyer%E2%80%93Moore_string-search_algorithm)?
- fuzzy-word search? (hard?)
- open multiple documents?
- mouse support? (https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/mouse.html)?
*/



void set_debug_msg(const char* msg, ...);
void draw_lines(docline*, int, bool);
void remove_line(docline* line, docline** head, docline** tail);
void clear_doc(docline* head);
void do_menu();
void parse_line(docline* line);
int align_tab(int num);
bool get_string(char* prompt, char* start, char* response, size_t max_size);
void check_unsaved_changes();

void load_document();
void save_document();

void draw_cursors();

// cursor movements
void cursor_left(cursor_pos* cursor);
void cursor_right(cursor_pos* cursor);
void cursor_up(cursor_pos* cursor);
void cursor_down(cursor_pos* cursor);

// cursor actions
void remove_char(cursor_pos* cursor);
void insert_newline(cursor_pos* cursor);
void insert_tab(cursor_pos* cursor);
void insert_character(cursor_pos* cursor, char ch);

// GLOBALS: TODO: Get these outta here! 
size_t width, height;
size_t absy = 0;			// absolute-y position in document
char fname[MAX_FILE_NAME];
docline* currline;
docline* head;
docline* tail;
bool unsaved_changes = false;

// these will become command line options
bool show_line_no = true;
bool syntax_highlighting = true;

// top line to display for scrolling
docline* topline;
size_t toplineno = 1;

// cursors
cursor_pos cursors[MAX_CURSORS] = {0};
int num_cursors = 1;

char* current_filename = NULL;
char debug_msg[MAX_DEBUG_MSG];
int debug_countdown = 0;

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

	cursors[0].xpos = 0;
	cursors[0].ypos = 0;

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
	init_pair(MACRO_PARAM_PAIR, COLOR_YELLOW, -1);

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
	cursors[0].currline = firstline;

	clock_t now = clock();

	// TODO: Parse command line here
	if (argc != 1 && check_file_exists(argv[1]))
	{
		// we can move all this stuff to a load file function
		clear_doc(head);
		load_doc(argv[1], &head, &tail);
		cursors[0].currline = head;
		find_labels(head);
	}

	// set topline here in case we loaded a file above
	topline = head;

	// initial cursor
	int absx = cursors[0].xpos;
	if (show_line_no)
		absx += 5;
	
	draw_lines(head, height - 1, screen_clean);
	mvchgat(cursors[0].ypos + 1, absx, 1, A_REVERSE, COLOR_PAIR(CUR_PAIR), NULL);

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
			check_unsaved_changes();
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
			set_debug_msg("multicarat up");
			if (num_cursors == MAX_CURSORS)
				break;
			cursors[num_cursors].xpos = cursors[0].xpos;
			cursors[num_cursors].ypos = cursors[0].ypos;
			set_debug_msg("nx: %d ny: %d", cursors[num_cursors].xpos, cursors[num_cursors].ypos);
			cursors[num_cursors].currline = cursors[0].currline;
			cursor_up(&cursors[0]);
			if (cursors[num_cursors].xpos == cursors[0].xpos && 
				cursors[num_cursors].ypos == cursors[0].ypos)
			{
				set_debug_msg("nvm carat up");
				// nvm, we couldn't actually make a new cursor
			}
			else
			{
				++num_cursors;
				set_debug_msg("carated up! %d", num_cursors);
			}
			break;

		case CTRL('h'):	// new carat down
			set_debug_msg("multicarat down");
			if (num_cursors == MAX_CURSORS)
				break;
			cursors[num_cursors].xpos = cursors[0].xpos;
			cursors[num_cursors].ypos = cursors[0].ypos;
			cursors[num_cursors].currline = cursors[0].currline;
			cursor_down(&cursors[0]);
			if (cursors[num_cursors].xpos == cursors[0].xpos && 
				cursors[num_cursors].ypos == cursors[0].ypos)
			{
				// nvm, we couldn't actually make a new cursor
				set_debug_msg("nvm carat down");
			}
			else
			{
				++num_cursors;
			}
			break;

		case CTRL('u'): // remove extra carats
			set_debug_msg("remove extra carats");
			num_cursors = 1;
			break;



		case KEY_SLEFT:
		{
			for (int i = 0; i < num_cursors; ++i)
			{
				while (cursors[i].xpos > 0 && cursors[i].currline->line[cursors[i].xpos] == ' ') --cursors[i].xpos;
				while (cursors[i].xpos > 0 && cursors[i].currline->line[cursors[i].xpos] != ' ') --cursors[i].xpos;
			}
			break;
		}

		case KEY_SRIGHT:
		{
			for (int i = 0; i < num_cursors; ++i)
			{
				linelen = strlen(cursors[i].currline->line);
				while (cursors[i].xpos++ < linelen && cursors[i].currline->line[cursors[i].xpos] == ' ');
				while (cursors[i].xpos++ < linelen && cursors[i].currline->line[cursors[i].xpos] != ' ');
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
				cursor_up(&cursors[i]);
			break;
		}

		case KEY_DOWN:
		{
			for (int i = 0; i < num_cursors; ++i)
				cursor_down(&cursors[i]);
			break;
		}

		case KEY_LEFT:
		{
			for (int i = 0; i < num_cursors; ++i)
				cursor_left(&cursors[i]);
			break;
		}

		case KEY_RIGHT:
		{
			for (int i = 0; i < num_cursors; ++i)
				cursor_right(&cursors[i]);
			break;
		}

		case ESC:		// exit, check for unsaved changes
		case KEY_F(12):
		{
			// if (unsaved_changes)
			// {
			// char sure[2];
			// get_string ("You have unsaved changes, save?(Y/n)", NULL, sure, 1);
			// if ((sure[0] == 'Y' || sure[0] == 'y'))
			// 	save_document();			
			// }
			check_unsaved_changes();
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
			// // insert TAB_DISTANCE spaces
			// int tab_target = TAB_DISTANCE - (main_cursor.xpos % TAB_DISTANCE);
			// if (main_cursor.xpos + tab_target < LINE_LENGTH)
			// {
			// 	for (size_t i = LINE_LENGTH; i > main_cursor.xpos + tab_target; --i)
			// 	{
			// 		main_cursor.currline->line[i] = main_cursor.currline->line[i - tab_target];
			// 	}
			// 	for (size_t i = main_cursor.xpos; i < main_cursor.xpos + tab_target; ++i)
			// 	{
			// 		main_cursor.currline->line[i] = ' ';
			// 	}
			// 	main_cursor.xpos += tab_target;
			// 	unsaved_changes = true;
			// }
			for (int i = 0; i < num_cursors; ++i)
				insert_tab(&cursors[i]);
			break;
		}

		default:	// a typable character
		{
			// // if we're not inserting at back of line, make room
			// if (main_cursor.xpos != strlen(main_cursor.currline->line))
			// {
			// 	for (size_t i = LINE_LENGTH; i > main_cursor.xpos; --i)
			// 	{
			// 		main_cursor.currline->line[i] = main_cursor.currline->line[i - 1];
			// 	}

			// }
			// main_cursor.currline->line[main_cursor.xpos] = (char) ch;
			// main_cursor.xpos++;
			// unsaved_changes = true;
			for (int i = 0; i < num_cursors; ++i)
				insert_character(&cursors[i], (char) ch);
			break;
		}
		}

		// // these are debug messages to check for underflow since we're using unsigned int
		// // for cursor position, if we ever have an enormous x or ypos, assume we've done
		// // something naughty
		// if (main_cursor.xpos > 1000)
		// 	set_debug_msg("XPOS is big, error!");
		// if (main_cursor.xpos >= strlen(main_cursor.currline->line))
		// 	main_cursor.xpos = strlen(main_cursor.currline->line);
		// if (main_cursor.ypos > 1000)
		// 	set_debug_msg("YPOS is big, error!");

		if (menuFlag)
		{
			// not implemented yet
			// do_menu();
		}

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
			draw_lines(topline, height - 1, screen_clean);
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
					mvprintw(0, (width - strlen(current_filename) - 6) / 2, "eztxt - %s", current_filename);
				else
					mvprintw(0, (width - 5) / 2, "eztxt");
				mvchgat(0, 0, -1, A_BOLD, BAR_PAIR, NULL);
			}
		}

		if (debug_countdown > 1)
		{
			mvprintw(height - 1, 0, "%s", debug_msg);
		} 
		else if (debug_countdown == 1)
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

void draw_cursors()
{
	int absx;
	for (int i = 0; i < num_cursors; ++i)
	{
		absx = cursors[i].xpos;
		if (show_line_no)
			absx += 5;
		mvchgat(cursors[i].ypos + 1, absx, 1, A_REVERSE, COLOR_PAIR(CUR_PAIR), NULL);
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
			if (topline == head)	// hmm, having to do this is obnoxious
				topline = newline;
			// this is working
			head = newline;
		}
		newline->prevline = cursor->currline->prevline;
		newline->nextline = cursor->currline;
		if (cursor->currline->prevline)
			cursor->currline->prevline->nextline = newline;
		cursor->currline->prevline = newline;

		unsaved_changes = true;
		++cursor->ypos;
		++absy;
		return;
	}
	// if we get here, we aren't special case head of line
	// so we have to move some chars down
	int nindex = 0;
	for (int i = cursor->xpos; i < strlen(cursor->currline->line); ++i)
	{
		newline->line[nindex] = cursor->currline->line[i];
		nindex++;
	}
	newline->line[nindex] = '\0';
	for (int i = strlen(cursor->currline->line); i >= cursor->xpos ; --i)
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
	++cursor->ypos;
	++absy;
	unsaved_changes = true;

}

void remove_char(cursor_pos* cursor)
{
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
			// todo: make %03 dynamic so it won't print leading 0s unless it needs to?
			mvprintw(yline, 0, "%03d:", toplineno + yline - 1);	// -1 due to 0 indexed arrays
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
	cursors[0].currline = head;
	topline = head;
	cursors[0].xpos = 0;
	cursors[0].ypos = 0;\
	num_cursors = 1;
	draw_lines(head, height - 1, true);
	set_debug_msg("Loaded %s", fname);
	unsaved_changes = false;
}

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

