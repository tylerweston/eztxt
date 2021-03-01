// fileio.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "headers/main.h"
#include "headers/fileio.h"

extern char* current_filename;
static const char* get_filename_from_path(const char* filename);

static const char* get_filename_from_path(const char* filename)
{
	// if there is a path attached to this file,
	// just return the filename, other do nothing
	if (strchr(filename, '/') == NULL)
		return filename;
	const char* p = filename + strlen(filename);
	while (*--p != '/');
	return ++p;
}

// this should only take a filename and a doc, since we're
// just filling in information in the doc, such as head and tail
int load_doc(const char* filename, doc* document)
{
	int num_lines = 1;
	FILE *fptr = fopen(filename, "r");
	if (fptr == NULL)
	{
		return -1;
	}
	if (current_filename)
		free(current_filename);
	current_filename = strdup(get_filename_from_path(filename));
	docline* lastline = NULL;
	char ch;
	int linelen = 0;
	document->number_of_lines = 1;
	document->number_of_chars = 0;
	docline* currline = calloc(1, sizeof(docline));
	if (currline == NULL)
	{
		// problem with calloc'ing
		return -1;
	}
	currline->prevline = NULL;
	currline->nextline = NULL;
	document->head = currline;

	for (;;)
	{
		ch = fgetc(fptr);
		if (feof(fptr))
		{
			// end of document
			document->tail = currline;
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
			++num_lines;
			++document->number_of_lines;
		}
		else if (ch == '\t')
		{
			int tab_target = TAB_DISTANCE - (linelen % TAB_DISTANCE);
			for (int i = 0; i < tab_target; ++i)
			{
				currline->line[linelen] = ' ';
				++linelen;
				++document->number_of_chars;
			}
		}
		else
		{
			currline->line[linelen] = ch;
			++document->number_of_chars;
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

void save_doc(const char* filename, doc* document)
{
	// save a document
	// go through line by line and write them out to a filename
	FILE* fptr = fopen(filename, "w");
	docline* cur = document->head;
	bool first = true;
	do
	{
		if (!first)
			fprintf(fptr, "\n");
		first = false;
		fprintf(fptr, "%s", cur->line);
		cur = cur->nextline;
	} while (cur != NULL);
	fclose(fptr);
}
