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
