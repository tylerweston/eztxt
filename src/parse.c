//parse - syntax parse
#include "headers/main.h"
#include "headers/parse.h"
#include <stdio.h>
#include <ctype.h>

#define MAX_TOKEN_LENGTH 12
#define NUM_KEYWORDS 500
#define NUM_PSEUDOINSTRUCTIONS 20

#define MAX_LABEL_LENGTH 36
#define MAX_LABELS 100

#define MAX_MACRO_LENGTH 36
#define MAX_MACROS 100

// TODO:
// - hex coloring
// - character literals


// for storing macro names
char seen_macros[MAX_MACROS][MAX_MACRO_LENGTH] = {0};
int macros_seen = 0;

// for storing labels
char seen_labels[MAX_LABELS][MAX_LABEL_LENGTH] = {0};
int labels_seen = 0;

// alphabetize and use binary search?
char kwords[NUM_KEYWORDS][MAX_TOKEN_LENGTH] = {0};
char pinstrs[NUM_PSEUDOINSTRUCTIONS][MAX_TOKEN_LENGTH] = {0};
int num_kwords = 0, num_pinstrs = 0;

bool is_keyword(const char* token);
bool is_pseudoinstruction(const char* token);
int read_file(const char* filename, char arr[][MAX_TOKEN_LENGTH]);
bool is_num(const char* token);
void add_label(const char* token);
void add_macro(const char* token);
bool is_label(const char* token);
bool is_macro(const char* token);
bool binarySearch(const char* search_token, char arr[][MAX_TOKEN_LENGTH], int arr_size);

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

void clear_labels()
{
	labels_seen = 0;
}

void clear_macros()
{
	macros_seen = 0;
}

void find_labels(docline* line)
{
	// should we clear here?
	bool grab_macro_name = false;
	char maybe_label[MAX_LABEL_LENGTH] = {0};
	while (line)
	{
		maybe_label[0] = '\0';
		size_t curindex = 0;
		char ch;
		for (size_t i = 0; i < strlen(line->line); ++i)
		{
			ch = line->line[i];
			if (ch == ' ' || ch == '\n' ||
			        ch == '\t' || ch == '(' ||
			        ch == ')' ||
			        ch == '\0')
			{
				// twice, clean it
				if (maybe_label[curindex] != '\0')
					maybe_label[curindex] = '\0';
				if (maybe_label[curindex - 1] == ':')
				{
					add_label(maybe_label);
				}
				if (grab_macro_name)
				{
					add_macro(maybe_label);
					grab_macro_name = false;
				}
				if (strcmp(maybe_label, ".macro") == 0)
				{
					grab_macro_name = true;
				}
				curindex = 0;
				maybe_label[0] = '\0';
			}
			else
			{
				maybe_label[curindex++] = ch;
			}
		}
		// twice, clean it
		if (maybe_label[curindex] != '\0')
			maybe_label[curindex] = '\0';
		if (maybe_label[curindex - 1] == ':')
		{
			add_label(maybe_label);
		}
		if (grab_macro_name && maybe_label[0] != '\0')
		{
			add_macro(maybe_label);
			grab_macro_name = false;
		}
		line = line->nextline;
	}
}

void parse_line(docline* line)
{
	char token[80];
	int start_index = -1, char_index = 0;
	char ch;
	bool in_quotes = false;
	bool in_single_quotes = false;
	bool in_comment = false;
	bool in_register = false;
	bool in_section = false;
	bool in_macro_param = false;
	attr_t to_assign;
	for (size_t i = 0; i <= strlen(line->line); ++i)
	{
		line->formatting[i] = COLOR_PAIR(ERROR_PAIR);	// line formatting 1 = error
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

		// this are is single quotes
		// todo, maybe check that we only have a single char in here
		// othewise, signal an error?
		// give double quotes priority so we can use single quotes
		// inside double quotes
		if (ch == '\'' && !in_quotes && !in_single_quotes)
		{
			in_single_quotes = true;
		}
		else if (ch == '\'' && !in_quotes && in_single_quotes)
		{
			line->formatting[i] = COLOR_PAIR(QUOTE_PAIR);
			in_single_quotes = false;
		}

		if (in_quotes || in_single_quotes)
		{
			if (i == strlen(line->line) - 1)
			{
				line->formatting[i] = COLOR_PAIR(ERROR_BLOCK_PAIR);
			}
			else
			{
				line->formatting[i] = COLOR_PAIR(QUOTE_PAIR);
			}
			continue;
		}

		if (ch == '.' && start_index == -1)
			in_section = true;

		if (ch == '$')
			in_register = true;

		if (ch == '%')
			in_macro_param = true;

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

		if (in_macro_param)
		{
			line->formatting[i] = COLOR_PAIR(MACRO_PARAM_PAIR);
			continue;
		}

		if (in_section)
		{
			line->formatting[i] = COLOR_PAIR(SECTION_PAIR);
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
		to_assign = 0;
		if (char_index > 0 && token[char_index - 1] == ':')
		{
			to_assign = COLOR_PAIR(LABEL_PAIR) | A_BOLD;;
		}
		else if (is_pseudoinstruction(token))
		{
			to_assign = A_BOLD /*| A_UNDERLINE*/;
		}
		else if (is_keyword(token))
		{
			to_assign = COLOR_PAIR(KEYWORD_PAIR);
		}
		else if (is_num(token))
		{
			to_assign = COLOR_PAIR(NUM_PAIR);
		}
		else if (is_label(token))
		{
			to_assign = COLOR_PAIR(LABEL_PAIR);
		}
		else if (is_macro(token))
		{
			to_assign = COLOR_PAIR(MACRO_PARAM_PAIR);
		}

		if (to_assign != 0)
		{
			for (size_t j = start_index; j < i; ++j)
			{
				line->formatting[j] = to_assign;
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

void add_label(const char* token)
{
	strncpy( seen_labels[labels_seen], token, strlen(token) - 1);
	labels_seen++;
}

void add_macro(const char* token)
{
	strncpy( seen_macros[macros_seen], token, strlen(token));
	macros_seen++;
}

bool is_label(const char* token)
{
	for (int i = 0; i < labels_seen; ++i)
	{
		// segfault is here
		if (strcmp(token, seen_labels[i]) == 0)
		{
			return true;
		}
	}
	return false;
}

bool is_macro(const char* token)
{
	for (int i = 0; i < macros_seen; ++i)
	{
		// segfault is here
		if (strcmp(token, seen_macros[i]) == 0)
		{
			return true;
		}
	}
	return false;
}

bool is_keyword(const char* token)
{
	// make char is upper function
	char* uppertoken = strdup(token);
	char ch;
	for (size_t i = 0; i < strlen(uppertoken); ++i)
	{
		ch = uppertoken[i];
		if (isalpha(ch))
			uppertoken[i] = toupper(ch);
	}
	bool results = binarySearch(uppertoken, kwords, num_kwords);
	free(uppertoken);
	return results;
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
	bool results = binarySearch(uppertoken, pinstrs, num_pinstrs);
	free(uppertoken);
	return results;
}

bool is_num(const char* token)
{
	// failsafe
	if (strlen(token) == 0)
		return false;
	// base 0 = check for decimal, octal, or hex numbers
	// 0 prefix = octal, 0x = hex, otherwise, decimal
	char* p;
	strtol(token, &p, 0);
	return *p == 0;
}

bool binarySearch(const char* search_token, char arr[][MAX_TOKEN_LENGTH], int arr_size)
{
	int left = 0, right = arr_size;
	int mid;
	int strcmpresults;
	while (left <= right)
	{
		mid = left + (right - left) / 2;
		strcmpresults = strcmp(arr[mid], search_token);
		if (strcmpresults == 0)
		{
			return true;
		}
		else if (strcmpresults < 0)
		{
			// arr char is LESS than search_toke char
			// ie: arr -> aa search_toke -> ab
			left = mid + 1;
		}
		else
		{
			right = mid - 1;
		}
	}
	return false;
}
