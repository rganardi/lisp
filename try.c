#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <bsd/string.h>
#include <stdlib.h>

//#include "strlcat.c"

#define BUFFER_SIZE 256
#define SEXP_BEGIN "("
#define SEXP_END ")"

int match(char c, const char *bytes) {
	const char *p = NULL;
	for (p = bytes; *p != '\0'; p++) {
		if (*p == c) {
			return 1;
		}
	}
	return 0;
}

//static char *lastchar(char *str, const char *delim) {
//	/* find lastchar before delim
//	 *
//	 * return NULL if first char is delim or no delim is found
//	 */
//	char *p = str;
//	while (*p != '\0') {
//		if (match(*p, delim)) {
//			p--;
//			break;
//		}
//		p++;
//	}
//	if (p == str) {
//		return NULL;
//	} else {
//		return p;
//	}
//}

static char *tok(char *str, const char *delim) {
	char *p = NULL;
	//char *t = NULL;

	p = str;
	//t = str;

	while (*p != '\0') {
		if (match(*p, delim)) {
			while (match(*p, delim)) {
				*(p++) = '\0';
			}
			/* p points to the next token */
			break;
		}
		p++;
	}

	return p;
}

static int count_open(char *str) {
	/* count the number of SEXP_BEGIN at the beginning of str */
	int i = 0;
	char *c = str;

	while (match(*c, SEXP_BEGIN) && *c != '\0') {
		i++;
		c++;
	}

	return i;
}

static int count_close(char *str) {
	/* count the number of SEXP_END at the end of str */
	int i = 0;
	char *c = &str[strlen(str) - 1];

#ifdef VDEBUG
	printf("count from %s ", c);
#endif

	while (match(*c, SEXP_END)) {
		i++;

		if (c == str) {
			break;
		}

		c--;
	}

	return i;
}


static int sexp_end(char *str, char **end) {
	/* find the end of the current sexp
	 *
	 * if the end is not found, return 1
	 *
	 * */
	int level = 0;
	char *p = *end;
	char *cp = NULL;
	char *save = NULL;

#ifdef PARSE
	printf("sexp_end(%s)\n", str);
#endif

	cp = strdup(str);
	save = cp;

	do {
		p = tok(cp, " \t");
		level += count_open(cp);
		level -= count_close(cp);

#ifdef DEBUG
		printf("{%lu}%s %d\n", strlen(cp), cp, level);
#endif
		if (level < 1) {
			free(save);
			*end = (p + level);
			return 0;
		}

		cp = p;
	} while (*p != '\0');

	free(save);

	return 1;
}

int parse(char *str) {
	char *end = NULL;
	char *t = NULL;

	printf("parsing %s\n", str);

	sexp_end(str, &end);
	printf("begin %s .. %c end\n", str, *(end - 3));

	do {
		t = tok(str, " \t");
		printf("tok: %s\n", str);
		str = t;
	} while (*t != '\0');

	return 0;
}

static int read_line(int fd, char *buf, size_t bufsiz) {
	/* read at most bufsize chars from fd into buf (including '\0') */
	size_t i = 0;
	char c = '\0';

	do {
		if (read(fd, &c, sizeof(char)) != sizeof(char))
			return -1;
		buf[i++] = c;
	} while (c != '\n' && i < (bufsiz - 1));

	if (buf[i - 1] == '\n') {
		buf[i - 1] = ' '; /* eliminates '\n' */
	}
	buf[i] = '\0';
	return 0;
}

size_t append_string(char **dest, const char *src) {
	char *dst = *dest;
	size_t n = strlen(dst) + strlen(src) + 1;

#ifdef VDEBUG
	printf("pre strlen(dst) %lu, strlen(src) %lu\n", strlen(dst), strlen(src));
#endif

	dst = realloc(dst, n);
	if (!dst) {
		fprintf(stderr, "can't resize dst\n");
	}

	if (strlcat(dst, src, n) >= n) {
		fprintf(stderr, "cat failed\n");
	}


#ifdef VDEBUG
	printf("post strlen(dst) %lu, strlen(src) %lu n %lu \n", strlen(dst), strlen(src), n);
#endif
	*dest = dst;
	return n;
}



int repl() {
	char buf[BUFFER_SIZE];
	char *str;
#if PARSE
	char *p = NULL;
	int i = 0;
#endif
	//int err = 0;
	int *level;

	if (!(level = calloc(1, sizeof(int)))) {
		fprintf(stderr, "can't initialize level\n");
	}

	//if (!(buf = calloc(BUFFER_SIZE, sizeof(buf)))) {
	//	fprintf(stderr, "can't initialize buf\n");
	//	exit(1);
	//}
	if (!(str = malloc(sizeof(char)))) {
		fprintf(stderr, "can't initialize str\n");
		exit(1);
	}

	*str = '\0';
	*level = 0;

	while (read_line(STDIN_FILENO, buf, BUFFER_SIZE) == 0) {
#if VDEBUG
		fprintf(stdout, "saved %s, read %s\n", str, buf);
		printf("{%lu} in buf, {%lu} in str\n", strlen(buf), strlen(str));
#endif
		append_string(&str, buf);
#if DEBUG
		printf("read %s\n", str);
#endif
#if PARSE
		i = sexp_end(str, &p);
		printf("p %s\n", p);
#endif

#if PARSE
		if (!i) {
#else
		if (0) {
#endif
			parse(str);
			free(str);
			str = malloc(sizeof(char));
			*str = '\0';
		}
	}

	free(str);
	free(level);
	//free(buf);
	return 0;
}

int main() {
	repl();
	return 0;
}
