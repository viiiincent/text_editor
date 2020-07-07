#ifndef ABUFF_H_
#define ABUFF_H_

#include <stdlib.h>
#include <strings.h>

struct abuf
{
	char *b;
	int  len;
};

#define ABUF_INIT {NULL, 0}

void ab_append(struct abuf *ab, const char *s, int len);
void ab_free(struct abuf *ab);

#endif
