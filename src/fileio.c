// fileio.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "headers/main.h"
#include "headers/fileio.h"

int load_doc(const char* filename, docline** head, docline** tail)
{
	FILE *fptr = fopen(filename, "r");
	if (fptr == NULL)
	{
		return -1;
	}
	if (current_filename)
		free(current_filename);
	current_filename = strdup(filename);
	docline* lastline = NULL;
	char ch;
	int linelen = 0;
	docline* currline = calloc(1, sizeof(docline));
	if (currline== NULL)
	{
		return -1;
	}
	*head = currline;
	for(;;)
	{
		ch = fgetc(fptr);
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
	return 0;
}

int check_file_exists(const char* filename)
{
	// Return codes:
	// 0: file does not exist
	// -1: file exists and is writable
	// -2: file exists but isn't writable
	if (access (filename, F_OK) == 0)
	{
		// file exists
		if (access (filename, W_OK) == 0)
		{
			// file exists and we can write to it
			return -1;
		}
		return -2;
	}
	else
	{
		return 0;
	}
}


void save_doc(const char* filename, docline* head)
{
	// todo: check if document exists already and ask
	// about overwrite? no, check if doc exists somewhere
	// else?
	// saving a document goes here
	// go through line by line and write them out to a filename
	FILE* fptr = fopen(filename, "w");
	docline* cur = head;
	do
	{
		fprintf(fptr, "%s\n", cur->line);
		cur = cur->nextline;
	} while (cur != NULL);
	fclose(fptr);
}
