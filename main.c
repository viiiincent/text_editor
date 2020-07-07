#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <fcntl.h>

#include "editor.h"

struct editor_config E;

enum keys
{
	BACKSPACE = 127,
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

void set_status_message(const char* fmt, ...);
void refresh_screen();
char* editor_prompt(char* prompt, void (*callback) (char*, int));

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

// row operation

int editor_row_rx_to_cx(erow* row, int rx)
{
	int cur_rx = 0;
	int cx;

	for (cx = 0; cx < row->size; ++cx)
	{
		if (row->chars[cx] == '\t')
			cur_rx += (TAB_LEN - 1) - (cur_rx % TAB_LEN);
		++cur_rx;

		if (cur_rx > rx) return cx;
	}
	return cx;
}

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

	editor_update_syntax(row);
}

void editor_insert_row(int at, char* s, size_t len)
{
	if (at < 0 || at > E.numrows) return;

	E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
	memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
	for (int j = at + 1; j <= E.numrows; ++j) E.row[j].idx++;

	E.row[at].idx = at;

	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);
	memcpy(E.row[at].chars, s, len);
	E.row[at].chars[len] = '\0';

	E.row[at].rsize = 0;
	E.row[at].render = NULL;
	E.row[at].hl = NULL;
	E.row[at].hl_open_comment = 0;
	editor_update_row(&E.row[at]);

	++E.numrows;
	E.is_dirty = 1;
}

void editor_free_row(erow* row)
{
	free(row->render);
	free(row->chars);
	free(row->hl);
}

void editor_del_row(int at)
{
	if (at < 0 || at >= E.numrows) return;
	editor_free_row(&E.row[at]);
	memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
	for (int j = at; j < E.numrows - 1; ++j) E.row[j].idx--;
	--E.numrows;
	E.is_dirty = 1;
}

void editor_row_insert_char(erow* row, int at, int c)
{
	if (at == 0 || at > row->size) at = row->size;
	row->chars = realloc(row->chars, row->size + 2);
	memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
	++row->size;
	row->chars[at] = c;
	editor_update_row(row);
	E.is_dirty = 1;
}

void editor_row_append_string(erow* row, char* s, size_t len)
{
	row->chars = realloc(row->chars, row->size + len + 1);
	memcpy(&row->chars[row->size], s, len);
	row->size += len;
	row->chars[row->size] = '\0';
	editor_update_row(row);
	E.is_dirty = 1;
}

void editor_row_del_char(erow* row, int at)
{
	if (at < 0 || at > row->size) return;
	memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
	row->size--;
	editor_update_row(row);
	E.is_dirty = 1;
}

// operations

void insert_char(int c)
{
	if (E.cy == E.numrows)
		editor_insert_row(E.numrows, "", 0);
	editor_row_insert_char(&E.row[E.cy], E.cx, c);
	++E.cx;
}

void insert_new_line()
{
	if (E.cx == 0)
	{
		editor_insert_row(E.cy, "", 0);
	}
	else
	{
		erow* row = &E.row[E.cy];
		editor_insert_row(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
		row = &E.row[E.cy];
		row->size = E.cx;
		row->chars[row->size] = '\0';
		editor_update_row(row);
	}
	++E.cy;
	E.cx = 0;
}

void del_char()
{
	if (E.cy == E.numrows) return;
	if (E.cx == 0 && E.cy == 0) return;

	erow* row = &E.row[E.cy];
	if (E.cx > 0)
	{
		editor_row_del_char(row, E.cx);
		--E.cx;
	}
	else
	{
		E.cx = E.row[E.cy - 1].size;
		editor_row_append_string(&E.row[E.cy - 1], row->chars, row->size);
		editor_del_row(E.cy);
		--E.cy;
	}
}

// file i/o

char* editor_rows_to_string(int* buffer_len)
{
	int file_len = 0;
	int j;
	for (j = 0; j < E.numrows; ++j)
	{
		file_len += E.row[j].size + 1;
	}
	*buffer_len = file_len;

	char* buf = malloc(file_len);
	char* p = buf;

	for (j = 0; j < E.numrows; ++j)
	{
		memcpy(p, E.row[j].chars, E.row[j].size);
		p += E.row[j].size;
		*p = '\n';
		++p;
	}
	return buf;
}

void editor_open(char* filename)
{
	free(E.filename);
	E.filename = strdup(filename);

	editor_select_syntax_highlight();

	FILE* fp = fopen(filename, "r");
	if (!fp) die("fopen");

	char* line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	while ((linelen = getline(&line, &linecap, fp)) != -1)
	{
		while (linelen > 0 && (line[linelen-1] == '\n' || line[linelen-1] == '\r'))
			linelen--;
		editor_insert_row(E.numrows, line, linelen);
	}
	free(line);
	fclose(fp);
	E.is_dirty = 0;
}

void editor_save()
{
	if (E.filename == NULL)
	{
		E.filename = editor_prompt("Save as: %s", NULL);
		if (E.filename == NULL)
		{
			set_status_message("Save aborted");
			return;
		}
		editor_select_syntax_highlight();
	}

	int len;
	char* buf = editor_rows_to_string(&len);

	int fd = open(E.filename, O_RDWR | O_CREAT, 0644); // @todo use a temporary file and rename it after success write
	if (fd != -1)
	{
		if (ftruncate(fd, len) != -1)
		{
			if (write(fd, buf, len) != -1)
			{
				close(fd);
				free(buf);
				E.is_dirty = 0;
				set_status_message("%d bytes written to disk", len);
				return;
			}
		}
		close(fd);
	}
	free(buf);
	set_status_message("Can't save! I/O error: %s", strerror(errno));
}

void editor_find_callback(char* query, int key)
{
	static int last_match = -1;
	static int direction = 1;

	static int saved_hl_line;
	static char* saved_hl = NULL;

	if (saved_hl)
	{
		memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
		free(saved_hl);
		saved_hl = NULL;
	}

	if (key == '\r' || key == '\x1b')
	{
		last_match = -1;
		direction = 1;
		return;
	}
	else if (key == ARROW_RIGHT || key == ARROW_DOWN)
	{
		direction = 1;
	}
	else if (key == ARROW_LEFT || key == ARROW_UP)
	{
		direction = -1;
	}
	else
	{
		last_match = -1;
		direction = 1;
	}

	if (last_match == -1) direction = 1;
	int current = last_match;
	int i;
	for (i = 0; i < E.numrows; ++i)
	{
		current += direction;
		if (current == -1) current = E.numrows - 1;
		else if (current == E.numrows) current = 0;

		erow* row = &E.row[current];
		char* match = strstr(row->render, query);
		if (match)
		{
			last_match = current;
			E.cy = current;
			E.cx = editor_row_rx_to_cx(row, match - row->render);
			E.rowoff = E.numrows;

			saved_hl_line = current;
			saved_hl = malloc(row->rsize);
			memcpy(saved_hl, row->hl, row->rsize);
			memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
			break;
		}
	}
}

void editor_find()
{
	int saved_cx = E.cx;
	int saved_cy = E.cy;
	int saved_coloff = E.coloff;
	int saved_rowoff = E.rowoff;

	char* query = editor_prompt("Search: %s (Use ESC/Arrows/Enter)", editor_find_callback);
	if (query)
	{
		free(query);
	}
	else
	{
		E.cx = saved_cx;
		E.cy = saved_cy;
		E.coloff = saved_coloff;
		E.rowoff = saved_rowoff;
	}
}

// output
// input

char *editor_prompt(char *prompt, void (*callback) (char*, int))
{
	size_t bufsize = 128;
	char *buf = malloc(bufsize);

	size_t buflen = 0;
	buf[0] = '\0';

	while (1) {
		set_status_message(prompt, buf);
		refresh_screen();

		int c = read_key();
		if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE)
		{
			if (buflen != 0) buf[--buflen] = '\0';
		}
		else if (c == '\x1b')
		{
			set_status_message("");
			if (callback) callback(buf, c);
			free(buf);
			return NULL;
		}
		else if (c == '\r')
		{
			if (buflen != 0)
			{
				set_status_message("");
				if (callback) callback(buf, c);
				return buf;
			}
		}
		else if (!iscntrl(c) && c < 128)
		{
			if (buflen == bufsize - 1)
			{
				bufsize *= 2;
				buf = realloc(buf, bufsize);
			}
			buf[buflen++] = c;
			buf[buflen] = '\0';
		}

		if (callback) callback(buf, c);
	}
}

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
	static int quit_times = QUIT_TIMES;

	int c = read_key();
	E.key_pressed = c;
	
	switch (c)
	{
		case '\r':
			insert_new_line();
			break;

		case CTRL_KEY('q'):
			if (E.is_dirty && quit_times > 0)
			{
				set_status_message("WARNING!!! File has unsaved changes. Press Ctrl-Q %d more times to quit.", quit_times);
				--quit_times;
				return;
			}
			write(STDIN_FILENO, "\x1b[2J", 4);
			write(STDIN_FILENO, "\x1b[H", 3);
			exit(0);
			break;

		case CTRL_KEY('s'):
			editor_save();
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			{
				if (c == PAGE_UP)
				{
					E.cy = E.rowoff;
				}
				else if (c == PAGE_DOWN)
				{
					E.cy = E.rowoff + E.screen_rows - 1;
					if (E.cy > E.screen_rows) E.cy = E.numrows;
				}
				int times = E.screen_rows;
				while (times--)
					c == PAGE_UP ? move_cursor(ARROW_UP) : move_cursor(ARROW_DOWN);
			}
			break;

		case HOME_KEY:
			E.cx = 0;
			break;
		case END_KEY:
			if (E.cx < E.numrows)
				E.cx = E.row[E.cy].size;
			break;

		case CTRL_KEY('f'):
			editor_find();
			break;

		case BACKSPACE:
		case CTRL_KEY('h'):
		case DEL_KEY:
			if (c == DEL_KEY) move_cursor(ARROW_RIGHT);
			del_char();
			break;

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_RIGHT:
		case ARROW_LEFT:
			move_cursor(c);
			break;

		case CTRL_KEY('l'):
		case '\x1b':
			break;

		default:
			insert_char(c);
			break;
	}

	quit_times = QUIT_TIMES;
}

int main(int argc, char** argv)
{
	init();
	if (argc >= 2)
	{
		editor_open(argv[1]);
	}

	set_status_message("HELP: Ctrl-S = Save | Ctrl-Q = quit | Ctrl-F = Search");

	while (1)
	{
		refresh_screen();
		process_key_press();
	}

	return 0;
}
