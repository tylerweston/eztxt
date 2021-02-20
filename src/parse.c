//parse - syntax parse
#include "headers/main.h"
#include "headers/parse.h"

void parse_line(docline* line)
{
	char ch;
	bool in_quotes = false;
	bool in_comment = false;
	bool in_register = false;
	bool in_section = false;
	for (int i = 0; i < strlen(line->line); ++i)
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

		if (ch == '.')
			in_section = true;

		if (in_section)
		{
			line->formatting[i] = COLOR_PAIR(SECTION_PAIR)  | A_BOLD;
			continue;
		}

		if (ch == '$')
			in_register = true;

		if (in_register)
		{
			line->formatting[i] = COLOR_PAIR(REG_PAIR) | A_BOLD;
			continue;
		}

		if (ch == ' ')
		{
			line->formatting[i] = 0;
			in_register = false;
			in_section = false;
		}

		// sometimes we need to pick out a token
		line->formatting[i] = COLOR_PAIR(KEYWORD_PAIR) | A_BOLD;

		if (ch == ',' || ch == '(' || ch == ')')
		{
			line->formatting[i] = COLOR_PAIR(PUNC_PAIR);
			in_register = false;
			in_section = false;
		}
		

	}
}
