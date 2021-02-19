#include <curses.h>
#include <stdlib.h>
#include <wchar.h>

#define LINE_LENGTH 80

 #define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

typedef struct docline 
{
	char line[LINE_LENGTH];
	struct docline* prevline;
	struct docline* nextline;
} docline;



void draw_lines(docline*, int);


int main(int argc, char** argv)
{
	initscr();
	int cur_col = 1;
	init_pair(cur_col, COLOR_RED, COLOR_BLACK);
	printw("eztxt - F12 exits\n");
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);
	refresh();
	wchar_t ch;
	bool exitFlag = false;
	int xpos = 0, ypos = 0;
	int topline = 0;
	bool screenScrolled = false;
	int changedLines = 0;

	docline* firstline = malloc(sizeof(docline));
	firstline->prevline = NULL;
	firstline->nextline = NULL;

	docline* currline = firstline;
	docline* head;
	docline* tail;
	head = firstline;
	tail = firstline;
	while(!exitFlag)
	{
		ch = getch();

		switch (ch)
		{

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
			xpos--;
			for (int i = xpos; i < LINE_LENGTH; ++i)
			{
				currline->line[i] = currline->line[i + 1];
			}
			break;

		case KEY_DC:
			if (xpos == 0 && currline->line[0] == '\0')
			{
				mvprintw(0, 20, "deleting a line");
				// delete this line
				if (currline == head)
				{
					currline->nextline->prevline = NULL;
					head = currline->nextline;
					docline* tmp = currline->nextline;
					free(currline);
					currline = tmp;
				}
				else if (currline = tail)
				{
					currline->prevline->nextline = NULL;
					tail = currline->prevline;
					docline* tmp = currline->prevline;
					free(currline);
					currline = tmp;
				}
				else
				{
					// TODO: This isn't right, fix this
					docline* tmp = currline;
					docline* tmp2 = currline->prevline;
					currline->prevline->nextline = tmp->nextline;
					currline->nextline->prevline = tmp2;
					free(currline);
					currline = tmp->nextline;
				}
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
		if (xpos >= LINE_LENGTH)
			xpos = LINE_LENGTH;
		if (ypos <= 0)
			ypos = 0;

		draw_lines(head, 20);
		mvchgat(ypos + 1, xpos, 1, A_BLINK, COLOR_PAIR(cur_col), NULL);
		mvprintw(0 , 0, "%d, %d", xpos, ypos);
	}

	endwin();
	exit(EXIT_SUCCESS);
}

void draw_lines(docline* top, int max_lines)
{
	docline* cur = top;
	int yline = 1;
	do 
	{
		move(yline, 0);
		clrtoeol();
		mvprintw(yline, 0, "%s", cur->line);
		yline++;
		cur = cur->nextline;
	} while (cur != NULL && yline < max_lines);
	refresh();
}