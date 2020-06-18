#include "editor.h"

static void enable_raw_mode();
static void disable_raw_mode();

void init()
{
    enable_raw_mode();
}

void die(const char *s)
{
	write(STDIN_FILENO, "\x1b[2J", 4); // clear entire screen
	write(STDIN_FILENO, "\x1b[H", 3);  // move cursor to col:1, row:1
	perror(s);
	exit(1);
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
