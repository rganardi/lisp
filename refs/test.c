#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <bsd/string.h>
#include <stdlib.h>

#define SEXP_BEGIN "("
#define SEXP_END ")"
#define SEXP_DELIM " \t"

static enum s_type {
	OBJ_NULL,	/* scheme '() */
	OBJ_ATOM,
	OBJ_PAIR
};

static struct Sexp {
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

static int dump_string(char *str, size_t n) {
	size_t i = 0;
	char *p = str;

	for (p = str; i < n; p++, i++) {
		printf("%p\t%#x\t%c\n", (void *) p, *p, *p);
	}

	return i;
}

static int match(char c, const char *bytes) {
	const char *p = NULL;
	for (p = bytes; *p != '\0'; p++) {
		if (*p == c) {
			return 1;
		}
	}
	return 0;
}

static int tok(char *str, char **start, char **end, char **next, size_t n) {
	char *p = str;
	size_t i = 0;

	while (match(*p, SEXP_DELIM) && (i <= n)) {
		p++;
		i++;
	}
	*start = p;

	for (; (i <= n); p++, i++) {
		if ((*p) == '\0' || i == n) {
			if (p != *start) {
				*end = p - 1;
				*next = p;
				return 0;
			} else {
				*end = p;
				*next = p;
				return 1;
			}
		} else if (match(*p, SEXP_DELIM)) {
			*end = p - 1;

			while (match(*p, SEXP_DELIM) && (i <= n)) {
				p++;
				i++;
			}
			*next = p;
			return 0;
		}
	}

	*next = p;
	*end = *start;
	return 1;
}

static int check_if_sexp(char *str) {
	while (match(*str, SEXP_DELIM)) {
		str++;
	}
	if (match(*str, SEXP_BEGIN)) {
		return 1;
	} else {
		return 0;
	}
}

static int sexp_end(char *str, char **end, size_t n) {
	/* find the end of the sexp at the start of str */
	char *p = str; //scan through the string
	char *w = str; //scan through words
	int level = 0; //nesting level in sexp
	size_t i = 0; //char count

	if (!(check_if_sexp(str))) {
		return 1;
	}

	while (match(*p, SEXP_DELIM)) {
		p++;
		i++;
	}

	w = p;

	while (*p != '\0' && i < n) {
#if DEBUG_SEXP_END
		printf("i %lu\tlevel %d\tp\t", i, level);
		dump_string(p, 1);
#endif
		if (match(*p, SEXP_DELIM) || (i == (n - 1))) {
#if DEBUG_SEXP_END
			printf("w\t");
			dump_string(w, 1);
#endif
			while (match(*w, SEXP_BEGIN)) {
					w++;
					level++;
			}
			if (match(*p, SEXP_DELIM)) {
				w = p - 1;
			} else if (i == (n - 1)) {
				w = str + (n - 1);
			}

#if DEBUG_SEXP_END
			printf("w here\t");
			dump_string(w, 1);
#endif
			while (match(*w, SEXP_END)) {
				w--;
				level--;
			}

			if (level <= 0) {
				if (match(*p, SEXP_DELIM)) {
					*end = (p-1) + level;
				} else if (i == (n - 1)) {
					*end = str + (n - 1) + level;
				}
#if DEBUG_SEXP_END
				printf("sexp_end: ends at ");
				dump_string(*end, 1);
#endif
				return 0;
			}

			while (match(*p, SEXP_DELIM)) {
				p++;
				i++;
			}

			w = p;
		} else {
			p++;
			i++;
		}
	}

	*end = str;
	return 1;
}

static int append_sexp(struct Sexp **dst, struct Sexp *src) {
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

static int parse_sexp(char *str, struct Sexp **tree, size_t n) {
	/* args:
	 *	char *str		= string to be parsed
	 *	struct Sexp **tree	= tree to store the parsed Sexp
	 *	size_t n		= length of string to be parsed (excluding last '\0')
	 * */
	char *end = NULL;
	//char *t = NULL;
	int err = -1;
	struct Sexp *c;
	char *start = NULL;
	char *next = NULL;
	size_t i = 0;

#if DEBUGPARSE
	printf("parse_sexp: parsing length %lu\n", n);
	dump_string(str, n);
#endif

	/* while loop, parse str until end */
	//err = sexp_end(str, &end, n);
	//if (err) {
	//	fprintf(stderr, "parse_sexp: can't parse the sexp %s\n", str);
	//	return 1;
	//}

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
		err = tok(str, &start, &end, &next, n - i);
		if (err) {
#if DEBUGPARSE
			fprintf(stderr, "parse_sexp: can't find token %s\n", str);
#endif
			return 0;
		}
#if DEBUGPARSE
		printf("tok:\n");
		dump_string(start, end - start + 1);
		printf("start\t");
		dump_string(start, 1);
		printf("end\t");
		dump_string(end, 1);
		printf("next\t");
		dump_string(next, 1);
#endif

		if (check_if_sexp(start)) {
			/* parse the sexp one level down */

			size_t sexp_len = 0;
			err = sexp_end(start, &end, n - i);
			if (err) {
#if DEBUGPARSE
				fprintf(stderr, "parse_sexp: can't find end\n");
#endif
				return 1;
			}
			sexp_len = end - start - 1;
#if DEBUGPARSE
			printf("sexp_len = %lu\n", sexp_len);
			//sleep(1);
			printf("-------------------------START-------------------\n");
			printf("parse_sexp(%s, &inside, %lu)\n", start + 1, sexp_len);
#endif
			struct Sexp *inside;
			inside = malloc(sizeof(struct Sexp));
			*inside = s_null;

			err = parse_sexp(start + 1, &inside, sexp_len);
#if DEBUGPARSE
			printf("-------------------------END---------------------\n");
#endif
			if (err) {
#if DEBUGPARSE
				fprintf(stderr, "cant do the thing inside\n");
#endif
				return 1;
			}

			c = malloc(sizeof(struct Sexp));
			c->type = OBJ_PAIR;
			c->pair = inside;
			c->next = NULL;

			append_sexp(tree, c);

			i += (end - start) + 1;
			str = end + 1;
		} else {
			/* append the current atom to the sexp */

			c = malloc(sizeof(struct Sexp));

			c->type = OBJ_ATOM;
			char *buf = NULL;
			buf = malloc((end - start + 2) * sizeof(char));
			strlcpy(buf, start, end - start + 2);

#if DEBUGPARSE
			printf("parse_sexp: buf\n");
			dump_string(buf, strlen(buf));
#endif

			c->atom = buf;
			c->next = NULL;
#if DEBUGPARSE
			printf("%p appending c\n", (void *) c);
			printf("%p input pre parse\n", (void *) *tree);
#endif
			append_sexp(tree, c);
#if DEBUGPARSE
			printf("%p input post parse\n", (void *) *tree);
#endif
			i += (next - str);
			str = next;
		}
#if DEBUGPARSE
		printf("chars parsed %lu\n", i);
		printf("tok again\n");
		printf("str\n");
		dump_string(str, strlen(str));
		printf("start\t");
		dump_string(start, 1);
		printf("end\t");
		dump_string(end, 1);
		printf("next\t");
		dump_string(next, 1);
#endif
		start = NULL;
		end = NULL;
		next = NULL;

	} while (*str != '\0' && i < n);

#if DEBUGPARSE
	printf("done parse\n");
#endif

	return 0;
}

static int print_sexp(struct Sexp *s) {
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

static int eval(struct Sexp *s) {
#if DEBUGEVAL
	printf("in eval\n");
#endif
	print_sexp(s);
#if DEBUGEVAL
	printf("exit eval\n");
#endif
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
		printf("freeing %p\n", pp);
#endif
		switch (pp->type) {
			case OBJ_ATOM:
				free(pp->atom);
				break;
			case OBJ_PAIR:
				free_sexp(pp->pair);
				break;
			case OBJ_NULL:
				break;
		}
		free(pp);
	} while (p);

	return;
}

static int test_parse_sexp(char *str) {
	int status = -1;
	struct Sexp *input = NULL;


	if (!(input = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "can't initialize input tree\n");
		exit(1);
	}

	*input = s_null;

	printf("parse_sexp(\"%s\", &input, %lu)\n", str, strlen(str));
	printf("str:\n");
	dump_string(str, strlen(str) + 1);
	status = parse_sexp(str, &input, strlen(str));
	printf("input:\n");
	eval(input);

	free_sexp(input);
	return status;
}

void test() {
	int i = -1;
	char **j = NULL;
	char *tests[] = {
		"",
		" ",
		"	",
		" 	",

		//"a",
		//" a",
		//"a ",
		//" a ",

		//"abc",
		//" abc",
		//"abc ",
		//" abc ",

		//"a b",
		//" a b",
		//"a b ",
		//" a b ",

		//"ab cd",
		//" ab cd",
		//"ab cd ",
		//" ab cd ",

		//"(a)",
		//" (a)",
		//"(a) ",
		//" (a) ",
		//"( a)",
		//" ( a)",
		//"( a) ",
		//" ( a) ",

		//"(a )",
		//" (a )",
		//"(a ) ",
		//" (a ) ",
		//"( a )",
		//" ( a )",
		//"( a ) ",
		//" ( a ) ",

		//"(abc)",
		//" (abc)",
		//"(abc) ",
		//" (abc) ",
		//"( abc)",
		//" ( abc)",
		//"( abc) ",
		//" ( abc) ",

		//"(abc )",
		//" (abc )",
		//"(abc ) ",
		//" (abc ) ",
		//"( abc )",
		//" ( abc )",
		//"( abc ) ",
		//" ( abc ) ",

		//"(abc efg)",
		//" (abc efg)",
		//"(abc efg) ",
		//" (abc efg) ",
		//"( abc efg)",
		//" ( abc efg)",
		//"( abc efg) ",
		//" ( abc efg) ",

		//"(abc efg )",
		//" (abc efg )",
		//"(abc efg ) ",
		//" (abc efg ) ",
		//"( abc efg )",
		//" ( abc efg )",
		//"( abc efg ) ",
		//" ( abc efg ) ",

		//"(a)b)",
		//" (a)b)",
		//"(a)b) ",
		//" (a)b) ",
		//"( a)b)",
		//" ( a)b)",
		//"( a)b) ",
		//" ( a)b) ",

		//"(a)b )",
		//" (a)b )",
		//"(a)b ) ",
		//" (a)b ) ",
		//"( a)b )",
		//" ( a)b )",
		//"( a)b ) ",
		//" ( a)b ) ",

		//"((a) b)",
		//" ((a) b)",
		//"((a) b) ",
		//" ((a) b) ",
		//"( (a) b)",
		//" ( (a) b)",
		//"( (a) b) ",
		//" ( (a) b) ",

		//"((a) b )",
		//" ((a) b )",
		//"((a) b ) ",
		//" ((a) b ) ",
		//"( (a) b )",
		//" ( (a) b )",
		//"( (a) b ) ",
		//" ( (a) b ) ",

		NULL};

	for (j = tests; (*j) != NULL ; j++) {
		i = test_parse_sexp(*j);
		if (i) {
			printf("failed\n\n");
		} else {
			printf("success\n\n");
		}
	}
	return;
}

int main() {
	test();
	return 0;
}
