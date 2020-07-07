#include "editor.h"

static void enable_raw_mode();
static void disable_raw_mode();
static int get_window_size(int *rows, int *cols);
static int get_cursor_position(int* rows, int* cols);
static void init_editor();

void init()
{
	enable_raw_mode();
	init_editor();
}

int editor_row_cx_to_rx(erow *row, int cx)
{
	int i, rx = 0;

	for (i=0; i < cx; ++i)
	{
		if (row->chars[i] == '\t')
			rx += (TAB_LEN - 1) - (rx % TAB_LEN);
		++rx;
	}
	return rx;
}

void editor_scroll()
{
	E.rx = 0;
	if (E.cy < E.numrows)
		E.rx = editor_row_cx_to_rx(&E.row[E.cy], E.cx);

	if (E.cy < E.rowoff)
	{
		E.rowoff = E.cy;
	}
	if (E.cy >= E.rowoff + E.screen_rows)
	{
		E.rowoff = E.cy - E.screen_rows + 1;
	}

	if (E.rx < E.coloff)
	{
		E.coloff = E.rx;
	}
	if (E.rx >= E.coloff + E.screen_cols)
	{
		E.coloff = E.rx - E.screen_cols + 1;
	}
}

void refresh_screen()
{
	editor_scroll();

	struct abuf ab = ABUF_INIT;

	ab_append(&ab, "\x1b[?25l", 6);  // hide cursor
	ab_append(&ab, "\x1b[H", 3);     // move cursor to col:1, row:1

	draw_rows(&ab);
	draw_status_bar(&ab);
	draw_message_bar(&ab);

	// move cursor
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy-E.rowoff)+1, (E.rx-E.coloff)+1);
	ab_append(&ab, buf, strlen(buf));

	ab_append(&ab, "\x1b[?25h", 6); // show cursor
	write(STDIN_FILENO, ab.b, ab.len);
	ab_free(&ab);
}

void draw_rows(struct abuf *ab)
{
	int y;
	for (y = 0; y < E.screen_rows; ++y)
	{
		int filerow = y + E.rowoff;
		if (filerow >= E.numrows)
		{
			if (E.numrows == 0 && y == E.screen_rows / 3)
			{
				char welcome[80];
				int welcome_len = snprintf(welcome, sizeof(welcome), "Yolo editor -- version %s", YOLO_VERSION);
				if (welcome_len > E.screen_cols)
					welcome_len = E.screen_cols;
				int padding = (E.screen_cols - welcome_len) / 2;
				if (padding)
				{
					ab_append(ab, "~", 1);
					--padding;
				}
				while (padding--)
					ab_append(ab, " ", 1);
				ab_append(ab, welcome, welcome_len);
			}
			else
			{
				ab_append(ab, "~", 1);
			}
		}
		else
		{
			int len = E.row[filerow].rsize - E.coloff;
			if (len < 0) len = 0;
			if (len > E.screen_cols) len = E.screen_cols;

			char* c = &E.row[filerow].render[E.coloff];
			unsigned char* hl = &E.row[filerow].hl[E.coloff];
			int current_color = -1;
			int j;
			for (j = 0; j < len; ++j)
			{
				if (iscntrl(c[j]))
				{
					char sym = (c[j] < 26) ? '@' + c[j] : '?';
					ab_append(ab, "\x1b[7m", 4);
					ab_append(ab, &sym, 1);
					ab_append(ab, "\x1b[m", 3);
					if (current_color != -1)
					{
						char buf[16];
						int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
						ab_append(ab, buf, clen);
					}
				}
				else if (hl[j] == HL_NORMAL)
				{
					if (current_color != -1)
					{
						ab_append(ab, "\x1b[39m", 5);
						current_color = -1;
					}
					ab_append(ab, &c[j], 1);
				}
				else
				{
					int color = editor_syntax_to_color(hl[j]);
					if (color != current_color)
					{
						current_color = color;
						char buf[16];
						int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
						ab_append(ab, buf, clen);
					}
					ab_append(ab, &c[j], 1);
				}
			}
			ab_append(ab, "\x1b[39m", 5);
		}
		ab_append(ab, "\x1b[K", 3); // clear line
		ab_append(ab, "\r\n", 2);
	}
}


void draw_status_bar(struct abuf* ab)
{
	ab_append(ab, "\x1b[7m", 4);

	char status[80], rstatus[80];
	int len = snprintf(
		status,
		sizeof(status),
		"%.20s - %d lines %s",
		E.filename ? E.filename : "[No Name]",
		E.numrows,
		E.is_dirty ? "(modified)": "");
	int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d",
						E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows);


	if (len > E.screen_cols) len = E.screen_cols;
	ab_append(ab, status, len);

	// int rlen = snprintf(
	// 	rstatus,
	// 	sizeof(rstatus),
	// 	"%d/%d", E.cy + 1, E.numrows);

	while (len < E.screen_cols)
	{
		if (E.screen_cols - len == rlen)
		{
			ab_append(ab, rstatus, rlen);
			break;
		}
		else
		{
			ab_append(ab, " ", 1);
			++len;
		}
	}
	ab_append(ab, "\x1b[m", 3);
	ab_append(ab, "\r\n", 2);
}

void draw_message_bar(struct abuf* ab)
{
	ab_append(ab, "\x1b[K", 3);
	int msg_len = strlen(E.status_msg);
	if (msg_len > E.screen_cols) msg_len = E.screen_cols;
	if (msg_len && time(NULL) - E.status_msg_time < 5)
		ab_append(ab, E.status_msg, msg_len);
}

void set_status_message(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(E.status_msg, sizeof(E.status_msg), fmt, ap);
	va_end(ap);
	E.status_msg_time = time(NULL);
}

static void enable_raw_mode()
{
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
		die("tcgetattr");
	atexit(disable_raw_mode);

	struct termios config = E.orig_termios;
	config.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	config.c_oflag &= ~OPOST;
	config.c_cflag |= CS8;
	config.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	config.c_cc[VMIN] = 0;
	config.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &config) == -1)
		die("tcsetattr");
}

static void disable_raw_mode()
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
		die("tcsetattr");
}

static int get_window_size(int *rows, int *cols)
{
	struct winsize ws;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
	{
		// plan b to get the screensize
		if (write(STDIN_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
			return -1;
		return get_cursor_position(rows, cols);
	}
	else
	{
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

// helper for plan b to get the screensize
static int get_cursor_position(int* rows, int* cols)
{
	if (write(STDIN_FILENO, "\x1b[6n", 4) != 4)
		return -1;

	char buf[32];
	unsigned int i = 0;

	while (i < sizeof(buf)-1)
	{
		if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if (buf[i] == 'R') break;
		++i;
	}
	buf[i] = '\0';
	printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);

	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

	return 0;
}

static void init_editor()
{
	E.cx = 0;
	E.cy = 0;
	E.rx = 0;
	E.rowoff = 0;
	E.coloff = 0;
	E.numrows = 0;
	E.row = NULL;
	if (get_window_size(&E.screen_rows, &E.screen_cols) == -1)
		die("get_window_size");
	E.screen_rows -= 2; // status bar height
	E.filename = NULL;
	E.status_msg[0] = '\0';
	E.status_msg_time = 0;
	E.is_dirty = 0;
	E.syntax = NULL;
}

void die(const char *s)
{
	write(STDIN_FILENO, "\x1b[2J", 4); // clear entire screen
	write(STDIN_FILENO, "\x1b[H", 3);  // move cursor to col:1, row:1
	perror(s);
	exit(1);
}
