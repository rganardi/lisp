#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <bsd/string.h>
#include <stdlib.h>

//#include "strlcat.c"

#define BUFFER_SIZE 256
#define SEXP_BEGIN "("
#define SEXP_END ")"
#define SEXP_DELIM " \t"

enum s_type {
	OBJ_NULL,	/* scheme '() */
	OBJ_ATOM,
	OBJ_PAIR
};

struct Sexp {
	enum s_type type;
	union {
		char *atom;
		struct Sexp *pair;
	};
	struct Sexp *next;
};

static const struct Sexp s_null = {
	.type = OBJ_NULL,
	.next = NULL
};

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
		p = tok(cp, SEXP_DELIM);
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

int check_if_sexp(char *str) {
	while (match(*str, SEXP_DELIM)) {
		str++;
	}
	if (match(*str, SEXP_BEGIN)) {
		return 1;
	} else {
		return 0;
	}
}

int print_sexp(struct Sexp *s) {
	struct Sexp *p = s;
	while (p != NULL) {
		switch (p->type) {
			case OBJ_NULL:
				printf("(null)");
				break;
			case OBJ_ATOM:
				printf("{%s}", p->atom);
				break;
			case OBJ_PAIR:
				printf("(");
				print_sexp(p->pair);
				printf(")");
				break;
			default:
				printf("unknown sexp type %d\n", p->type);
				return 1;
		}

		p = p->next;
		//printf("sleep print\n");
		//sleep(1);
	}

	return 0;
}

int append_sexp(struct Sexp **dest, struct Sexp *src) {
	struct Sexp *dst = *dest;
	struct Sexp *p = dst;

	printf("appending sexps\n");
	print_sexp(src);
	printf("\nto\n");
	print_sexp(dst);
	printf("\n");


	if (p->type == OBJ_NULL) {
		src->next = dst;
		dst = src;
	} else {
		while ((p->next)->type != OBJ_NULL) {
			p = p->next;
		}

		src->next = p->next;
		p->next = src;
	}

	printf("after append\n");
	print_sexp(dst);
	printf("exit append\n");
	*dest = dst;

	return 0;
}

int parse(char *str, struct Sexp *s) {
//int parse(char *str, struct Sexp **tree) {
	//struct Sexp *s = *tree;
	char *end = NULL;
	//char *t = NULL;
	int err = -1;
	struct Sexp c;

	printf("parsing %s\n", str);

	err = sexp_end(str, &end);
	if (err) {
		fprintf(stderr, "can't parse the sexp\n%s", str);
	}

	while (match(*str, SEXP_DELIM)) {
		str++;	/* eat whitespace */
	}

	if (check_if_sexp(str)) {
		str++;
		/* parse the sexp one level down */
	} else {
		/* append the current atom to the sexp */
		c.type = OBJ_ATOM;
		c.atom = str;
		c.next = NULL;
		//c.next = s;
		append_sexp(&s, &c);
		printf("in parse\n");
		print_sexp(s);
		//*tree = s;
	}

	printf("begin %s .. %c end\n", str, *(end - 3));
	printf("again in parse\n");
	print_sexp(s);

	/*
	do {
		t = tok(str, SEXP_DELIM);
		printf("tok: %s\n", str);
		str = t;
	} while (*t != '\0');
	*/

	printf("done parse\n");

	return 0;
}

int eval(struct Sexp *s) {
	printf("in eval\n");
	print_sexp(s);
	printf("exit eval\n");
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
	char *str = NULL;
#if PARSE
	char *p = NULL;
	int i = 0;
#endif
	int *level = NULL;
	struct Sexp *input = NULL;

	if (!(level = calloc(1, sizeof(int)))) {
		fprintf(stderr, "can't initialize level\n");
	}

	if (!(str = malloc(sizeof(char)))) {
		fprintf(stderr, "can't initialize str\n");
		exit(1);
	}

	if (!(input = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "can't initialize input tree\n");
		exit(1);
	}

	*level = 0;
	*str = '\0';
	//input->type = OBJ_NULL;
	//input->next = NULL;
	*input = s_null;

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
			parse(str, input);
			eval(input);
			free(str);
			//free(input);

			//if (!(input = malloc(sizeof(struct Sexp)))) {
			//	fprintf(stderr, "can't initialize input tree\n");
			//	exit(1);
			//}

			if (!(str = malloc(sizeof(char)))) {
				fprintf(stderr, "can't initialize str\n");
				exit(1);
			}

			*str = '\0';
			//input->type = OBJ_NULL;
			//input->next = NULL;
			*input = s_null;
		}
	}

	free(str);
	free(level);
	//free(input);
	return 0;
}

int main() {
	repl();
	return 0;
}
