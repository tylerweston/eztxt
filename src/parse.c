//parse - syntax parse
#include "headers/main.h"
#include "headers/parse.h"
#include <stdio.h>
#include <ctype.h>

#define MAX_TOKEN_LENGTH 12
#define NUM_KEYWORDS 500
#define NUM_PSEUDOINSTRUCTIONS 20

// alphabetize and use binary search?
char kwords[NUM_KEYWORDS][MAX_TOKEN_LENGTH] = {0};
char pinstrs[NUM_PSEUDOINSTRUCTIONS][MAX_TOKEN_LENGTH] = {0};
int num_kwords = 0, num_pinstrs = 0;

bool is_keyword(const char* token);
bool is_pseudoinstruction(const char* token);
int read_file(const char* filename, char arr[][MAX_TOKEN_LENGTH]);

int init_parser()
{
	// init kwords
	num_kwords = read_file("dat/keywords.dat", kwords);
	if (num_kwords == -1)
		return -1;
	num_pinstrs = read_file("dat/pinstrs.dat", pinstrs);
	if (num_pinstrs == -1)
		return -2;
	return 0;
}

int read_file(const char* filename, char arr[][MAX_TOKEN_LENGTH])
{
	FILE* fptr = fopen(filename, "r");
	if (!fptr)
		return -1;
	char line_buf[MAX_TOKEN_LENGTH];
	int line_count = 0;
	for(;;)
	{
		if (fgets(line_buf, MAX_TOKEN_LENGTH, fptr) == NULL)
			break;
		if (line_buf[0] == '#') continue;
		// trim newline
		line_buf[strlen(line_buf) - 1] = '\0';
		strncpy(arr[line_count], line_buf, MAX_TOKEN_LENGTH);
		line_count++;
	}
	fclose(fptr);
	return line_count;
}

// void read_pseudoinstructions(const char* filename)
// {

// }

void parse_line(docline* line)
{
	char token[80];
	int start_index = -1, char_index = 0;
	char ch;
	bool in_quotes = false;
	bool in_comment = false;
	bool in_register = false;
	bool in_section = false;
	for (size_t i = 0; i <= strlen(line->line); ++i)
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
			line->formatting[i] = COLOR_PAIR(COMMENT_PAIR);
			continue;
		}

		// this area is quoted
		if (ch == '\"' && !in_quotes)
		{
			in_quotes = true;
		}
		else if (ch == '\"' && in_quotes)
		{
			line->formatting[i] = COLOR_PAIR(QUOTE_PAIR);
			in_quotes = false;
		}

		if (in_quotes)
		{
			line->formatting[i] = COLOR_PAIR(QUOTE_PAIR);
			continue;
		}

		if (ch == '.' && start_index == -1)
			in_section = true;

		if (ch == '$')
			in_register = true;

		if (ch == ' ' || ch == '\t' || ch == '\0' || ch == '\n')
		{
			line->formatting[i] = 0;
			in_register = false;
			in_section = false;
			goto got_token;
		}

		if (ch == ',' || ch == '(' || ch == ')')
		{
			line->formatting[i] = COLOR_PAIR(PUNC_PAIR);
			in_register = false;
			in_section = false;
			goto got_token;
		}

		if (in_section)
		{
			line->formatting[i] = COLOR_PAIR(SECTION_PAIR)  | A_BOLD;
			continue;
		}

		if (in_register)
		{
			line->formatting[i] = COLOR_PAIR(REG_PAIR) | A_BOLD;
			continue;
		}

		if (start_index == - 1)
			start_index = i;

		// sometimes we need to pick out a token
		token[char_index++] = ch;
		continue;

got_token:
		token[char_index] = '\0';
		// todo: assign attr here and then use them in a
		// loop below instead of writing this loop three times
		if (char_index > 0 && token[char_index - 1] == ':')
		{
			for (size_t j = start_index; j < i; ++j)
			{
				line->formatting[j] = COLOR_PAIR(LABEL_PAIR) | A_BOLD;
			}
		}
		else if (is_pseudoinstruction(token))
		{
			for (size_t j = start_index; j < i; ++j)
			{
				line->formatting[j] = A_BOLD | A_UNDERLINE;
			}
		}
		else if (is_keyword(token))
		{
			for (size_t j = start_index; j < i; ++j)
			{
				line->formatting[j] = A_BOLD;
			}
		}
		// reset everything
		start_index = -1;
		char_index = 0;
		// token will now contain our word
		int s = strlen(token);
		for (int j = s; j > 0; --j)
		{
			// memclear or whatever?
			token[j] = '\0';
		}
	}
}

bool is_keyword(const char* token)
{
	char* uppertoken = strdup(token);
	char ch;
	for (size_t i = 0; i < strlen(uppertoken); ++i)
	{
		ch = uppertoken[i];
		if (isalpha(ch))
			uppertoken[i] = toupper(ch);
	}
	for (int i = 0; i < num_kwords; ++i)
	{
		if (strcmp(uppertoken, kwords[i]) == 0)
		{
			free(uppertoken);
			return true;
		}
	}
	free(uppertoken);
	return false;
}

bool is_pseudoinstruction(const char* token)
{
	char* uppertoken = strdup(token);
	char ch;
	for (size_t i = 0; i < strlen(uppertoken); ++i)
	{
		ch = uppertoken[i];
		if (isalpha(ch))
			uppertoken[i] = toupper(ch);
	}
	for (int i = 0; i < num_pinstrs; ++i)
	{
		if (strcmp(uppertoken, pinstrs[i]) == 0)
		{
			free(uppertoken);
			return true;
		}
	}
	free(uppertoken);
	return false;
}
