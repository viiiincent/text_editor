#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define YOLO_VERSION "0.0.1"
#define TAB_LEN 8

#define CTRL_KEY(k) ((k) & 0x1f)

enum keys
{
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	PAGE_UP,
	PAGE_DOWN,
	HOME_KEY,
	END_KEY,
	DEL_KEY
};

typedef struct erow
{
	int size;
	int rsize;
	char* chars;
	char* render;
} erow;

struct termios original_term_config;
struct editor_config
{
	int key_pressed; // debug
	int cx, cy;
	int rowoff;
	int coloff;
	int screen_rows;
	int screen_cols;
	struct termios orig_termios;
	int numrows;
	erow* row;
};
struct editor_config E;

// terminal
void die(const char *s)
{
	write(STDIN_FILENO, "\x1b[2J", 4); // clear entire screen
	write(STDIN_FILENO, "\x1b[H", 3);  // move cursor to col:1, row:1
	perror(s);
	exit(1);
}

void disable_raw_mode()
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
		die("tcsetattr");
}

void enable_raw_mode()
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

int read_key()
{
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if (nread == -1 && nread != EAGAIN)
			die("read");
	}

	if (c == '\x1b') // escape character
	{
		char seq[3];
		if (read(STDIN_FILENO, &seq[0], 1) == -1) return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) == -1) return '\x1b';
		if (seq[0] == '[')
		{
			if (seq[1] >= '0' && seq[1] <= '9')
			{
				if (read(STDIN_FILENO, &seq[2], 1) == -1) return '\x1b';
				if (seq[2] == '~')
				{
					switch (seq[1])
					{
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
			}
			else
			{
				switch (seq[1])
				{
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;
				}
			}
		}
		else if (seq[0] == 'O')
		{
			switch (seq[1])
			{
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
			}
		}
		return '\x1b';
	}
	else
	{
		return c;
	}
}

// helper for plan b to get the screensize
int get_cursor_position(int* rows, int* cols)
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

int get_window_size(int *rows, int *cols)
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

// row operations

void editor_update_row(erow* row)
{
	int tabs = 0;
	int j;
	for (j=0; j < row->size; ++j)
		if (row->chars[j] == '\t') tabs++;

	free(row->render);
	row->render = malloc(row->size + (tabs*(TAB_LEN-1)) + 1); // row->size already count 1 for each tab

	int idx = 0;
	for (j = 0; j < row->size; ++j)
	{
		if (row->chars[j] == '\t')
		{
			row->render[idx++] = ' ';
			while (idx % TAB_LEN != 0) row->render[idx++] = ' '; // the next tab stop is the first column multiple of TAB_LEN
		}
		else
		{
			row->render[idx++] = row->chars[j];
		}
	}

	row->render[idx] = '\0';
	row->rsize = idx;
}

void editor_append_row(char* s, size_t len)
{
	E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

	int at = E.numrows;
	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);
	memcpy(E.row[at].chars, s, len);
	E.row[at].chars[len] = '\0';

	E.row[at].rsize = 0;
	E.row[at].render = NULL;
	editor_update_row(&E.row[at]);

	++E.numrows;
}

// file i/o

void editor_open(char* filename)
{
	FILE* fp = fopen(filename, "r");
	if (!fp) die("fopen");

	char* line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	while ((linelen = getline(&line, &linecap, fp)) != -1)
	{
		while (linelen > 0 && (line[linelen-1] == '\n' || line[linelen-1] == '\r'))
			linelen--;
		editor_append_row(line, linelen);
	}
	free(line);
	fclose(fp);
}

// append buffer
struct abuf
{
	char *b;
	int  len;
};

#define ABUF_INIT {NULL, 0}

void ab_append(struct abuf *ab, const char *s, int len)
{
	char *new = realloc(ab->b, ab->len+len);

	if (new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void ab_free(struct abuf *ab)
{
	free(ab->b);
}

// output
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
			ab_append(ab, &E.row[filerow].render[E.coloff], len);
		}

		// // debug
		// if (y == E.screen_rows - 2)
		// {
		// 	char key_pressed[20];
		// 	int key_pressed_len = snprintf(key_pressed, sizeof(key_pressed), "key: %d", E.key_pressed);
		// 	int padding = E.screen_cols - key_pressed_len - 2;
		// 	while (padding--)
		// 		ab_append(ab, " ", 1);
		// 	ab_append(ab, key_pressed, 8);
		// }
		// if (y == E.screen_rows - 1)
		// {
		// 	char cursor_pos[20];
		// 	int cursor_pos_len = snprintf(cursor_pos, sizeof(cursor_pos), "cy: %d, cx: %d", E.cy, E.cx);
		// 	int padding = E.screen_cols - cursor_pos_len - 2;
		// 	while (padding--)
		// 		ab_append(ab, " ", 1);
		// 	ab_append(ab, cursor_pos, cursor_pos_len);
		// }
		// // !debug

		ab_append(ab, "\x1b[K", 3); // clear line
		if (y < E.screen_rows-1)
			ab_append(ab, "\r\n", 2);
	}

	write(STDIN_FILENO, "\x1b[H", 3);
}

void editor_scroll()
{
	if (E.cy < E.rowoff)
	{
		E.rowoff = E.cy;
	}
	if (E.cy >= E.rowoff + E.screen_rows)
	{
		E.rowoff = E.cy - E.screen_rows + 1;
	}

	if (E.cx < E.coloff)
	{
		E.coloff = E.cx;
	}
	if (E.cx >= E.coloff + E.screen_cols)
	{
		E.coloff = E.cx - E.screen_cols + 1;
	}
}

void refresh_screen()
{
	editor_scroll();

	struct abuf ab = ABUF_INIT;

	ab_append(&ab, "\x1b[?25l", 6);  // hide cursor
	ab_append(&ab, "\x1b[H", 3);     // move cursor to col:1, row:1

	draw_rows(&ab);

	// move cursor
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy-E.rowoff)+1, (E.cx-E.coloff)+1);
	ab_append(&ab, buf, strlen(buf));

	ab_append(&ab, "\x1b[?25h", 6); // show cursor
	write(STDIN_FILENO, ab.b, ab.len);
	ab_free(&ab);
}

// input

void move_cursor(int key)
{
	erow* row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

	switch (key)
	{
		case ARROW_UP:
			if (E.cy > 0) E.cy--;
			break;
		case ARROW_LEFT:
			if (E.cx != 0)
			{
				E.cx--;
			}
			else if (E.cy > 0) // first column and not first line? go to previous line end
			{
				E.cy--;
				E.cx = E.row[E.cy].size;
			}
			break;
		case ARROW_DOWN:
			if (E.cy < E.numrows) E.cy++;
			break;
		case ARROW_RIGHT:
			if (row && E.cx < row->size)
			{
				E.cx++;
			}
			else if (row && E.cx == row->size) // end of line? go to next column start
			{
				E.cx = 0;
				E.cy++;
			}
			break;
	}

	row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
	int rowlen = row ? row->size : 0;
	if (E.cx > rowlen) E.cx = rowlen;
}

void process_key_press()
{
	int c = read_key();
	E.key_pressed = c;
	
	switch (c)
	{
		case CTRL_KEY('q'):
			write(STDIN_FILENO, "\x1b[2J", 4);
			write(STDIN_FILENO, "\x1b[H", 3);
			exit(0);
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			{
				int times = E.screen_rows;
				while (times--)
					c == PAGE_UP ? move_cursor(ARROW_UP) : move_cursor(ARROW_DOWN);
			}
			break;

		case HOME_KEY:
			E.cx = 0;
			break;
		case END_KEY:
			E.cx = E.screen_cols - 1;
			break;

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_RIGHT:
		case ARROW_LEFT:
			move_cursor(c);
			break;
	}
}

// init
void init_editor()
{
	E.cx = 0;
	E.cy = 0;
	E.rowoff = 0;
	E.coloff = 0;
	E.numrows = 0;
	E.row = NULL;
	if (get_window_size(&E.screen_rows, &E.screen_cols) == -1)
		die("get_window_size");
}

int main(int argc, char** argv)
{
	enable_raw_mode();
	init_editor();
	if (argc >= 2)
	{
		editor_open(argv[1]);
	}

	while (1)
	{
		refresh_screen();
		process_key_press();
	}

	return 0;
}
