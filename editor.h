#ifndef EDITOR_H_
#define EDITOR_H_

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "syntax_highlight.h"
#include "abuff.h"

#define YOLO_VERSION "0.0.1"
#define TAB_LEN 8
#define QUIT_TIMES 3
#define CTRL_KEY(k) ((k) & 0x1f)

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
extern struct editor_config E;

void init();
void editor_scroll();
void refresh_screen();
void draw_rows(struct abuf* ab);
void draw_status_bar(struct abuf* ab);
void draw_message_bar(struct abuf* ab);
void set_status_message(const char* fmt, ...);

int editor_row_cx_to_rx(erow* row, int cx);

// utils
void die(const char *s);

#endif
