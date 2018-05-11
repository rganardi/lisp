#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bsd/string.h>

#include "parser.h"

int match(char c, const char *bytes) {
	const char *p = NULL;
	for (p = bytes; *p != '\0'; p++) {
		if (*p == c) {
			return 1;
		}
	}
	return 0;
}

int tok(char *str, char **start, char **end, char **next, size_t n) {
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

int read_line(FILE *stream, char *buf, size_t bufsiz) {
	// read at most bufsize chars from fd into buf (including '\0')
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

int sexp_end(char *str, char **end, size_t n) {
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

int parse_sexp(char *str, struct Sexp **tree, size_t n) {
	/* args:
	 *	char *str		= string to be parsed
	 *	struct Sexp **tree	= tree to store the parsed Sexp
	 *	size_t n		= length of string to be parsed (excluding last '\0')
	 * */
	char *end = NULL;
	int err = -1;
	struct Sexp *c;
	char *start = NULL;
	char *next = NULL;
	size_t i = 0;

#if DEBUGPARSE
	printf("parse_sexp: parsing length %lu\n", n);
	dump_string(str, n);
#endif

	while (match(*str, SEXP_DELIM)) {
		str++;	/* eat whitespace */
		i++;
	}


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

			i += (end - str) + 1;
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

int print_sexp(struct Sexp *s) {
	struct Sexp *p = s;
#if DEBUG_PRINT_SEXP
	printf("(\n");
#endif
	while (p != NULL) {
		switch (p->type) {
#if DEBUG_PRINT_SEXP
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
#else
			case OBJ_NULL:
				break;
			case OBJ_ATOM:
				printf("%s", p->atom);
				break;
			case OBJ_PAIR:
				printf("(");
				print_sexp(p->pair);
				printf(")");
				break;
			default:
				printf("unknown sexp type\n");
				return 1;
#endif
		}

		p = p->next;
		if (p && p->type != OBJ_NULL) printf(" ");
	}
#if DEBUG_PRINT_SEXP
	printf(")\n");
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

size_t len_sexp(struct Sexp *s) {
	size_t n = 0;
	while (s) {
		if (s->type == OBJ_NULL) {
			break;
		}
		n++;
		s = s->next;
	}
	return n;
}

int sexp_cp(struct Sexp *dst, struct Sexp *src) {
	//dst should be a pointer to a memory allocated for struct Sexp
	struct Sexp *s = src;
	struct Sexp *d = dst;
	struct Sexp *dd = NULL;

#if DEBUG_SEXP_CP
	printf("sexp_cp: copying\n");
	print_sexp(src);
	printf("\n");
#endif

	while (s) {
		d->type = s->type;
		switch (s->type) {
			case OBJ_NULL:
				d->type = OBJ_NULL;
				d->next = NULL;
				return 0;
				break;
			case OBJ_ATOM:
				if (!(d->atom = malloc(sizeof(char) * (strlen(s->atom)+1)))) {
					fprintf(stderr, "sexp_cp: not enough memory\n");
					return 1;
				}

				if (strlcpy(d->atom, s->atom, (strlen(s->atom)+1)) >= (strlen(s->atom)+1)) {
					fprintf(stderr, "sexp_cp: strlcpy failed\n");
					return 1;
				}
				break;
			case OBJ_PAIR:
				if (!(dd = malloc(sizeof(struct Sexp)))) {
					fprintf(stderr, "sexp_cp: not enough memory\n");
					return 1;
				}
				if (sexp_cp(dd, s->pair)) {
					fprintf(stderr, "sexp_cp: failed to copy the sexp below\n");
					return 1;
				}
				d->pair = dd;
				break;
			default:
				fprintf(stderr, "sexp_cp: unknown sexp type\n");
				return 1;
				break;
		}

		if (!(dd = malloc(sizeof(struct Sexp)))) {
			fprintf(stderr, "sexp_cp: not enough memory\n");
			return 1;
		}
		d->next = dd;
		d = dd;

		s = s->next;
	}
	return 0;
}

int sexp_sub(struct Sexp **orig, struct Sexp *new) {
	//subs in the Sexp pointed to at orig by new
	struct Sexp *s = NULL;

	if (len_sexp(new) != 1) {
		fprintf(stderr, "sexp_sub: len_sexp(new) is %lu != 1\n", len_sexp(new));
		return 1;
	}

	if (!(s = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "sexp_sub: not enough memory to sub\n");
		return 1;
	}

	if (sexp_cp(s, new)) {
		fprintf(stderr, "sexp_sub: can't copy new sexp\n");
		return 1;
	}

	free_sexp(s->next);

	s->next = (*orig)->next;

	(*orig)->next = NULL;
	free_sexp(*orig);

	*orig = s;

	return 0;
}

//FIXME: this is unused
int sexp_replace_all(struct Sexp **orig, char *name, struct Sexp *new) {
	//replaces all occurences of name with new
	struct Sexp *s = *orig;
	struct Sexp *prev = NULL;

	while (s) {
		switch (s->type) {
			case OBJ_NULL:
				return 0;
			case OBJ_ATOM:
				if (!(strcmp(s->atom, name))) {
					if (sexp_sub(&s, new)) {
						fprintf(stderr, "sexp_replace_all: failed to sub\n");
						return 1;
					}

					if (prev) {
						prev->next = s;
					} else {
						*orig = s;
					}
				}
				break;
			case OBJ_PAIR:
				if (sexp_replace_all(&(s->pair), name, new)) {
					fprintf(stderr, "sexp_replace_all: failed to recurse\n");
					return 1;
				}

				break;
			default:
				fprintf(stderr, "sexp_replace_all: unknown sexp type\n");
				return 1;
				break;
		}

		prev = s;
		s = s->next;
	}

	return 0;
}

