#ifndef MIPSZE_PARSE
#define MIPSZE_PARSE

int init_parser();
void parse_line(docline* line);
void clear_labels();
void clear_macros();
void find_labels(docline* line);

#endif