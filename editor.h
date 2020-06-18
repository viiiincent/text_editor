#ifndef EDITOR_H_
#define EDITOR_H_

#include <stdlib.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>

typedef struct erow
{
	int idx;
	int size;
	int rsize;
	char* chars;
	char* render;
	unsigned char* hl;
	int hl_open_comment;

} erow;

struct termios original_term_config;
struct editor_config
{
	int key_pressed; // debug
	int cx, cy;
	int rx;
	int rowoff;
	int coloff;
	int screen_rows;
	int screen_cols;
	struct editor_syntax* syntax;
	struct termios orig_termios;
	int numrows;
	erow* row;
	int is_dirty;
	char* filename;
	char status_msg[80];
	time_t status_msg_time;
};
struct editor_config E;

void die(const char *s);

#endif
