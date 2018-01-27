#ifdef MTRACE
#define _XOPEN_SOURCE
#include <mcheck.h>
#endif

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

#ifdef DEBUGCOUNT
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


static int sexp_end(char *str, char **end, size_t n) {
	/* find the end of the current sexp
	 *
	 * if the end is not found, return 1
	 *
	 * */
	int level = 0;
	char *p = *end;
	char *cp = NULL;
	char *save = NULL;

#ifdef DEBUGSEXP_END
	printf("sexp_end(%s)\n", str);
#endif

	cp = malloc(n * sizeof(char));
	cp = memcpy(cp, str, n);
	save = cp;

	size_t i = 0;
	for (p = cp; i < n; i++, p++) {
		if (*p == '\0') {
			*p = ' ';
		}
	}

	do {
		p = tok(cp, SEXP_DELIM);
		level += count_open(cp);
		level -= count_close(cp);

#ifdef DEBUGSEXP_END
		printf("{%lu}%s %d\n", strlen(cp), cp, level);
#endif
		if (level < 1) {
			*end = (str + (p - save) + level);
			free(save);
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
	printf("(\n");
	while (p != NULL) {
		switch (p->type) {
			case OBJ_NULL:
				printf("%p (null)\n", (void *) p);
				break;
			case OBJ_ATOM:
				printf("%p {%s}\n", (void *) p, p->atom);
				break;
			case OBJ_PAIR:
				print_sexp(p->pair);
				break;
			default:
				printf("%p unknown sexp type\n", (void *) p);
				return 1;
		}

		p = p->next;
		//printf("sleep print\n");
		//sleep(1);
	}
	printf(")\n");

	return 0;
}

void free_sexp(struct Sexp *s) {
	/* free a (null) terminated Sexp */
	struct Sexp *p = s;
	struct Sexp *pp = s;

	do {
		pp = p;
		p = p->next;

#if DEBUGFREE_SEXP
		printf("freeing %p %s\n", pp, pp->atom);
#endif
		free(pp);
	} while (p);

	return;
}

int append_sexp(struct Sexp **dst, struct Sexp *src) {
	struct Sexp *p = *dst;
	struct Sexp *pp = *dst;

#if DEBUGAPPEND_SEXP
	printf("appending sexps\n");
	print_sexp(src);
	printf("\nto\n");
	print_sexp(*dst);
	printf("\n");
#endif


	if ((*dst)->type == OBJ_NULL) {
#if DEBUGAPPEND_SEXP
		printf("%p src\n", src);
		printf("%p src->next\n", src->next);
		printf("%p *dst\n", *dst);
		printf("%p (*dst)->next\n", (*dst)->next);
#endif

		src->next = *dst;
		*dst = src;

#if DEBUGAPPEND_SEXP
		printf("post\n");
		printf("%p src\n", src);
		printf("%p src->next\n", src->next);
		printf("%p *dst\n", *dst);
		printf("%p (*dst)->next\n", (*dst)->next);
#endif
	} else {
		while (p->type != OBJ_NULL) {
			pp = p;
			p = p->next;
		}
#if DEBUGAPPEND_SEXP
		printf("%p src\n", src);
		printf("%p src->next\n", src->next);
		printf("%p pp\n", pp);
		printf("%p pp->next\n", pp->next);
#endif

		src->next = pp->next;
		pp->next = src;

#if DEBUGAPPEND_SEXP
		printf("post\n");
		printf("%p src\n", src);
		printf("%p src->next\n", src->next);
		printf("%p pp\n", pp);
		printf("%p pp->next\n", pp->next);
#endif
	}

	return 0;
}

void print_string(char *str, char *end) {
	char *c = str;
	printf("%p c\t%p end\n", c, end);

	while (c != end) {
		printf("0x%x ", *c);
		c++;
	}
	printf("%p c\t%p end\n", c, end);
	return;
}

size_t chop_sexp(char **input, size_t n) {
	char *str = *input;
	int err = 0;
	char *end = NULL;
	char *p = str;
	size_t i = 0;

	err = sexp_end(str, &end, n);
	if (err) {
		fprintf(stderr, "can't parse the sexp inside %s\n", str);
		return 0;
	}
	str++;

	for (p = str; p != end; p++) {
		if (*p == '\0') {
			*p = ' ';
		}
		i++;
	}
	while (p != str) {
		if (*p == ')') {
			*p = ' ';
			break;
		}
		p--;
		i--;
	}

	*input = str;

	return end - str;
}

int parse_sexp(char *str, struct Sexp **tree, size_t n) {
	/* args:
	 *	char *str		= string to be parsed
	 *	struct Sexp **tree	= tree to store the parsed Sexp
	 *	size_t n		= length of string to be parsed (excluding last '\0')
	 * */
	char *end = str + n;
	//char *start = str;
	char *t = NULL;
	int err = -1;
	struct Sexp *c;

#if DEBUGPARSE
	printf("parsing %s of length %lu\n", str, n);
#endif

	/* while loop, parse str until end */
	err = sexp_end(str, &end, n);
	if (err) {
		fprintf(stderr, "can't parse the sexp %s\n", str);
	}

	size_t i = 0;
	while (match(*str, SEXP_DELIM)) {
		str++;	/* eat whitespace */
		i++;
	}


#if DEBUGPARSE
	//printf("begin %p .. %p end\n", str, end - 0);
	//printf("begin %s .. %s end\n", str, (end - 0));
	//printf("begin %x .. %x end\n", *str, *(end - 0));
#endif

	end = NULL;
	do {
		t = tok(str, SEXP_DELIM);
#if DEBUGPARSE
		printf("tok: %s\n", str);
#endif

		if (check_if_sexp(str)) {
			/* parse the sexp one level down */

			size_t sexp_len = 0;
			sexp_len = chop_sexp(&str, n);
#if DEBUGPARSE
			printf("recursive call parse\n");
			//print_string(str,end);
#endif
			struct Sexp *inside;
			inside = malloc(sizeof(struct Sexp));
			*inside = s_null;

			parse_sexp(str, &inside, sexp_len);

			c = malloc(sizeof(struct Sexp));
			c->type = OBJ_PAIR;
			c->pair = inside;
			c->next = NULL;

			append_sexp(tree, c);

			i += (sexp_len);
			str += sexp_len;
		} else {
			/* append the current atom to the sexp */

			c = malloc(sizeof(struct Sexp));
			c->type = OBJ_ATOM;
			c->atom = str;
			c->next = NULL;
#if DEBUGPARSE
			printf("%p appending c\n", (void *) c);
			printf("%p input pre parse\n", (void *) *tree);
#endif
			append_sexp(tree, c);
#if DEBUGPARSE
			printf("%p input post parse\n", (void *) *tree);
#endif
			i += (t - str);
			str = t;
		}
#if DEBUGPARSE
		printf("chars parsed %lu\n", i);
#endif

	} while (*t != '\0' && i < n);

#if DEBUGPARSE
	printf("done parse\n");
#endif

	return 0;
}

int eval(struct Sexp *s) {
#if DEBUGEVAL
	printf("in eval\n");
#endif
	print_sexp(s);
#if DEBUGEVAL
	printf("exit eval\n");
#endif
	return 0;
}

int parse_eval(char *str) {
	struct Sexp *input = NULL;

	if (!(input = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "can't initialize input tree\n");
		exit(1);
	}

	*input = s_null;

	parse_sexp(str, &input, strlen(str));
#if EVAL
	eval(input);
#endif
	free_sexp(input);

	return 0;
}

static int read_line(FILE *stream, char *buf, size_t bufsiz) {
	/* read at most bufsize chars from fd into buf (including '\0')
	 * */
	size_t i = 0;
	int c = '\0';

	do {
		c = fgetc(stream);
		if (c == EOF)
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

#ifdef DEBUGAPPEND
	printf("pre strlen(dst) %lu, strlen(src) %lu\n", strlen(dst), strlen(src));
#endif

	dst = realloc(dst, n);
	if (!dst) {
		fprintf(stderr, "can't resize dst\n");
	}

	if (strlcat(dst, src, n) >= n) {
		fprintf(stderr, "cat failed\n");
	}


#ifdef DEBUGAPPEND
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
#endif
#if PARSE || DEBUGPARSE
	int i = 0;
#endif
	int *level = NULL;

	if (!(level = calloc(1, sizeof(int)))) {
		fprintf(stderr, "can't initialize level\n");
	}

	if (!(str = malloc(sizeof(char)))) {
		fprintf(stderr, "can't initialize str\n");
		exit(1);
	}

	*level = 0;
	*str = '\0';

	while (read_line(stdin, buf, BUFFER_SIZE) == 0) {
#if DEBUGREAD
		fprintf(stdout, "saved %s, read %s\n", str, buf);
		printf("{%lu} in buf, {%lu} in str\n", strlen(buf), strlen(str));
#endif
		append_string(&str, buf);
#if DEBUGREAD
		printf("read %s\n", str);
#endif
#if PARSE
		i = sexp_end(str, &p, strlen(str));
#endif
#if DEBUGPARSE
		printf("p %s\n", p);
#endif

#if PARSE
		if (!i) {
#else
		if (0) {
#endif
			parse_eval(str);

			if (!(str = realloc(str, sizeof(char)))) {
				fprintf(stderr, "can't initialize str\n");
				exit(1);
			}

			*str = '\0';
		}
	}

	free(str);
	free(level);
	return 0;
}

int main() {
#ifdef MTRACE
	putenv("MALLOC_TRACE=mtrace.log");
	mtrace();
#endif
	repl();
	return 0;
}
