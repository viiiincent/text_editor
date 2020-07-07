#ifndef SYNTAX_HIGHLIGHT_H_
#define SYNTAX_HIGHLIGHT_H_

#include <ctype.h>
#include <stdlib.h>
#include <strings.h>
#include "editor.h"

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)
#define HL_DB_ENTRIES (sizeof(HL_DB) / sizeof(HL_DB[0]))

#ifdef  ORIGIN_FILE
int global;
#else
extern int global;
#endif

enum highlight {
	HL_NORMAL = 0,
	HL_COMMENT,
	HL_MLCOMMENT,
	HL_KEYWORD1,
	HL_KEYWORD2,
	HL_NUMBER,
	HL_STRING,
	HL_MATCH
};

struct editor_syntax
{
	char* filetype;
	char** filematch;
	char** keywords;
	char* single_line_comment_start;
	char* multiline_comment_start;
	char* multiline_comment_end;
	int flags;
};
extern struct editor_syntax HL_DB[];

void editor_select_syntax_highlight();
void editor_update_syntax();
int editor_syntax_to_color(int hl);
int is_separator(int c);

#endif
