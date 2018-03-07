#ifdef MTRACE
#define _XOPEN_SOURCE
#include <mcheck.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <bsd/string.h>
#include <stdlib.h>
#include <signal.h>

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

struct Env {
	struct Env *next;
	struct Sexp *s_object;
	char *name;
};

struct Closure {
	struct Sexp *arg;
	struct Sexp *body;
	struct Env *env;
};

static const struct Sexp s_null = {
	.type = OBJ_NULL,
	.next = NULL
};

#if DEBUG
static void segfault_handler(int error);
static int dump_string(char *str, size_t n);
#endif
static int tok(char *str, char **start, char **end, char **next, size_t n);
static int check_if_sexp(char *str);
static int sexp_end(char *str, char **end, size_t n);
static int print_sexp(struct Sexp *s);
static void free_sexp(struct Sexp *s);
static int append_sexp(struct Sexp **dst, struct Sexp *src);
static size_t len_sexp(struct Sexp *s);
static int parse_sexp(char *str, struct Sexp **tree, size_t n);
static int sexp_cp(struct Sexp *dst, struct Sexp *src);
static int sexp_sub(struct Sexp **orig, struct Sexp *new);
static int new_env_binding(struct Env **env, char *name, struct Sexp s_object);
static int env_cp(struct Env **dst, struct Env src);
static int env_rebind(struct Env **env, char *name, struct Sexp new_object);
static int env_unbind(struct Env **env, char *name);
static void free_env(struct Env *env);
static int print_env(struct Env *env);
static int lookup_env(struct Env *env, char *name, struct Sexp **s);
static int new_closure(struct Closure **cl, struct Sexp *s, struct Env env);
static int print_closure(struct Closure *cl);
static void free_closure(struct Closure *cl);
static int s_define(struct Sexp *s, struct Env **env);
static int s_beta_red(struct Sexp *s, struct Env *env, struct Sexp **res);
static int sexp_replace_all(struct Sexp **orig, char *name, struct Sexp *new);
static int sexp_env_replace(struct Sexp **orig, struct Env *env);
static int s_lambda(struct Sexp *s, struct Env *env, struct Sexp **res);
static int eval(struct Sexp *s, struct Env **env, struct Sexp **res);
static int parse_eval(char *str, struct Env **env);
static int read_line(FILE *stream, char *buf, size_t bufsiz);
static int repl();

#if DEBUG
static void segfault_handler(int error) {
	fprintf(stderr, "SEGFAULT, bad program!\n");
	exit(EXIT_FAILURE);
}

static int dump_string(char *str, size_t n) {
	size_t i = 0;
	char *p = str;

	for (p = str; i < n; p++, i++) {
		printf("%p\t%#x\t%c\n", (void *) p, *p, *p);
	}

	return i;
}
#endif

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

static int print_sexp(struct Sexp *s) {
	struct Sexp *p = s;
#if DEBUG
	printf("(\n");
#endif
	while (p != NULL) {
		switch (p->type) {
#if DEBUG
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
				printf("\"%s\" ", p->atom);
				break;
			case OBJ_PAIR:
				printf("(");
				print_sexp(p->pair);
				printf(") ");
				break;
			default:
				printf("unknown sexp type\n");
				return 1;
#endif
		}

		p = p->next;
		//printf("sleep print\n");
		//sleep(1);
	}
#if DEBUG
	printf(")\n");
#endif

	return 0;
}

static void free_sexp(struct Sexp *s) {
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

static size_t len_sexp(struct Sexp *s) {
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

static int sexp_cp(struct Sexp *dst, struct Sexp *src) {
	//dst should be a pointer to a memory allocated for struct Sexp
	struct Sexp *s = src;
	struct Sexp *d = dst;
	struct Sexp *dd = NULL;

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
				if (!(d->pair = malloc(sizeof(struct Sexp)))) {
					fprintf(stderr, "sexp_cp: not enough memory\n");
					return 1;
				}
				if (sexp_cp(d->pair, s->pair)) {
					fprintf(stderr, "sexp_cp: failed to copy the sexp below\n");
					return 1;
				}
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

static int sexp_sub(struct Sexp **orig, struct Sexp *new) {
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

static int new_env_binding(struct Env **env, char *name, struct Sexp s_object) {
	struct Env *e = NULL;
	struct Sexp *s = NULL;
	char *str = NULL;

	if (!(e = malloc(sizeof(struct Env)))) {
		fprintf(stderr, "new_env_binding: can't allocate memory for new environment\n");
		return 1;
	}

	if (!(s = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "new_env_binding: can't allocate memory for new environment\n");
		return 1;
	}

	if(sexp_cp(s, &s_object)) {
		fprintf(stderr, "new_env_binding: failed to copy env\n");
		return 1;
	}

	if (!(str = malloc(sizeof(char) * (strlen(name)+1)))) {
		fprintf(stderr, "new_env_binding: can't allocate memory for new environment\n");
		return 1;
	}

	if (strlcpy(str, name, (strlen(name)+1)) >= (strlen(name)+1)) {
		fprintf(stderr, "new_env_binding: strlcpy failed\n");
		return 1;
	}

	e->name = str;
	e->s_object = s;
	e->next = *env;

	*env = e;

	return 0;
}

static int env_cp(struct Env **dst, struct Env src) {
	//*dst should be a pointer to NULL
	struct Env *s = &src;
	struct Env *d = NULL;
	struct Env *dd = *dst;
	struct Sexp *p = NULL;
	char *str = NULL;

	while (s) {
		if (!(d = malloc(sizeof(struct Env)))) {
			fprintf(stderr, "env_cp: can't allocate memory for new environment\n");
			return 1;
		}

		if (!(p = malloc(sizeof(struct Sexp)))) {
			fprintf(stderr, "env_cp: can't allocate memory for new environment\n");
			return 1;
		}

		if(sexp_cp(p, s->s_object)) {
			fprintf(stderr, "env_cp: failed to copy env\n");
			return 1;
		}

		if (!(str = malloc(sizeof(char) * (strlen(s->name)+1)))) {
			fprintf(stderr, "env_cp: can't allocate memory for new environment\n");
			return 1;
		}

		if (strlcpy(str, s->name, (strlen(s->name)+1)) >= (strlen(s->name)+1)) {
			fprintf(stderr, "env_cp: strlcpy failed\n");
			return 1;
		}

		d->name = str;
		d->s_object = p;
		if (dd) {
			dd->next = d;
		} else {
			*dst = d;
		}
		dd = d;

		s = s->next;
	}
	dd->next = NULL;

	return 0;
}

static int env_rebind(struct Env **env, char *name, struct Sexp new_object) {
	struct Env *e = *env;
	struct Sexp *s = NULL;

	while (strcmp(e->name, name)) {
		e = e->next;
		if (!e) {
			fprintf(stderr, "env_rebind: \"%s\" not found\n", name);
			return 1;
		}
	}

#if DEBUG_ENV_REBIND
	printf("env_rebind: rebinding \"%s\"\n", e->name);
#endif

	s = e->s_object;
	free_sexp(s);

	if (!(s = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "env_rebind: not enough memory to rebind\n");
	}

	if (sexp_cp(s, &new_object)) {
		fprintf(stderr, "env_rebind: copying failed\n");
	}

	e->s_object = s;
	return 0;
}

static int env_unbind(struct Env **env, char *name) {
	struct Env *prev = NULL;
	struct Env *e = *env;

	while (strcmp(e->name, name)) {
		prev = e;
		e = e->next;
		if (!e) {
#if DEBUG_ENV_UNBIND
			fprintf(stderr, "env_unbind: \"%s\" not found\n", name);
#endif
			return 1;
		}
	}

#if DEBUG_ENV_UNBIND
	printf("env_unbind: unbinding \"%s\"\n", e->name);
	printf("before\n");
	print_env(*env);
#endif

	if (!prev) {
		//e is start
		*env = e->next;
		e->next = NULL;
		free_env(e);
	} else {
		//e is middle
		prev->next = e->next;
		e->next = NULL;
		free_env(e);
	}

#if DEBUG_ENV_UNBIND
	printf("after\n");
	print_env(*env);
#endif

	return 0;
}

static void free_env(struct Env *env) {
	struct Env *p = env;
	struct Env *pp = env;

	do {
		pp = p;
		p = p->next;

		free(pp->name);
		free_sexp(pp->s_object);

		free(pp);
	} while (p);

	return;
}

static int print_env(struct Env *env) {
	while (env) {
#if DEBUG
		printf("env->name\n");
		dump_string(env->name, strlen(env->name)+1);
#else
		printf("env->name\t%s\n", env->name);
#endif
		printf("env->s_object\n");
		print_sexp(env->s_object);
#ifndef DEBUG
		printf("\n");
#endif

		env = env->next;
	}
	return 0;
}

static int lookup_env(struct Env *env, char *name, struct Sexp **s) {
	//return 1 when name is found in env, set s to point at the sexp
	//return 0 otherwise
	while (env) {
		if (!(strcmp(env->name, name))) {
			*s = env->s_object;
			return 1;
		}
		env = env->next;
	}
	*s = NULL;

	return 0;
}

static int new_closure(struct Closure **cl, struct Sexp *s, struct Env env) {
	struct Sexp *p = NULL;
	struct Closure *c = NULL;
	struct Env *e = NULL;

	if (s->type != OBJ_PAIR) {
		fprintf(stderr, "new_closure: not a lambda expression\n");
		return 1;
	}

	if (strncmp((s->pair)->atom, "lambda", strlen("lambda")+1)
			|| len_sexp(s->pair) < 3) {
		fprintf(stderr, "new_closure: not a valid lambda expression\n");
		return 1;
	}

	if (!(c = malloc(sizeof(struct Closure)))) {
		fprintf(stderr, "new_closure: not enough memory for a closure\n");
		return 1;
	}

	if (!(p = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "new_closure: not enough memory for a sexp\n");
		return 1;
	}

	if (sexp_cp(p, ((s->pair)->next)->pair)) {
		fprintf(stderr, "new_closure: failed to copy args\n");
		return 1;
	}

	c->arg = p;

	if (!(p = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "new_closure: not enough memory for a sexp\n");
		return 1;
	}

	if (sexp_cp(p, ((s->pair)->next)->next)) {
		fprintf(stderr, "new_closure: failed to copy args\n");
		return 1;
	}

	c->body = p;

	if (env_cp(&e, env)) {
		fprintf(stderr, "new_closure: can't copy environment");
		return 1;
	}

	c->env = e;

	*cl = c;

	return 0;
}

static int print_closure(struct Closure *cl) {
	printf("cl->arg\n");
	if (print_sexp(cl->arg)) {
		return 1;
	}

	printf("cl->body\n");
	if (print_sexp(cl->body)) {
		return 1;
	}

	printf("cl->env\n");
	if (print_env(cl->env)) {
		return 1;
	}
	return 0;
}

static void free_closure(struct Closure *cl) {
	free_sexp(cl->arg);
	free_sexp(cl->body);
	free_env(cl->env);
	free(cl);
}

static int s_define(struct Sexp *s, struct Env **env) {
	struct Sexp *p = NULL;

	if (len_sexp(s) != 3) {
		fprintf(stderr, "s_define: define takes exactly two args\n");
		return 1;
	}

	s = s->next;
	if (s->type != OBJ_ATOM) {
		fprintf(stderr, "s_define: name must be an atom\n");
		return 1;
	}

	if (lookup_env(*env, s->atom, &p)) {
		if (env_rebind(env, s->atom, *(s->next))) {
			fprintf(stderr, "s_define: rebind failed\n");
			return 1;
		}
	} else {
		if (new_env_binding(env, s->atom, *(s->next))) {
			fprintf(stderr, "s_define: new binding failed\n");
			return 1;
		}
	}
#if DEBUG_DEFINE
	print_env(*env);
#endif
	return 0;
}

static int s_beta_red(struct Sexp *s, struct Env *env, struct Sexp **res) {
	// s = ((lambda (x) body) args)
	struct Sexp *p = NULL;
	struct Sexp *arg = NULL;

	if (len_sexp(s->pair) < 3) {
		fprintf(stderr, "s_beta_red: lambda expression must be of length at least 3\n");
		return 1;
	}

	//do beta reduction
	switch (((s->pair)->next)->type) {
		case OBJ_NULL:
			//(lambda () body)
			break;
		case OBJ_ATOM:
			//(lambda x body)
			if (!(p = malloc(sizeof(struct Sexp)))) {
				fprintf(stderr, "s_beta_red: not enough memory\n");
				return 1;
			}

			p->type = OBJ_PAIR;
			p->pair = ((s->pair)->next)->next;
			p->next = NULL;
			if (new_env_binding(&env,
						(((s->pair)->next)->atom),
						*p)) {

				fprintf(stderr, "s_beta_red: failed to bind arguments to formal params\n");
				return 1;
			}
			free_sexp(p);
			break;
		case OBJ_PAIR:
			//(lambda (x ...) body)
			p = ((s->pair)->next)->pair;
			arg = s->next;
			while (p || arg) {
				if (p->type == OBJ_NULL
						&& arg->type == OBJ_NULL) {
					break;
				} else if (p->type == OBJ_NULL
						|| arg->type == OBJ_NULL) {
					fprintf(stderr, "s_beta_red: wrong number of arguments supplied\n");
					return 1;
				}

				if (new_env_binding(&env,
							p->atom,
							*arg)) {
					fprintf(stderr, "s_beta_red: failed to bind arguments to formal params\n");
					return 1;
				}

				p = p->next;
				arg = arg->next;
			}
			break;
		default:
			fprintf(stderr, "s_beta_red: unrecognized lambda expression\n");
			return 1;
	}

	//eval body
	p = (((s->pair)->next)->next);
	while (p) {
		if (p->type == OBJ_NULL) {
			break;
		}

		if (eval(p, &env, res)) {
			fprintf(stderr, "s_beta_red: eval failed\n");
			return 1;
		}
		p = p->next;
	}

	return 0;
}

static int sexp_replace_all(struct Sexp **orig, char *name, struct Sexp *new) {
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

static int sexp_env_replace(struct Sexp **orig, struct Env *env) {
	struct Sexp *p = *orig;
	struct Sexp *result = NULL;
	struct Sexp *prev = NULL;

	while (p) {
		switch (p->type) {
			case OBJ_NULL:
				return 0;
			case OBJ_ATOM:
				if (lookup_env(env, p->atom, &result)) {
					if (sexp_sub(&p, result)) {
						fprintf(stderr, "sexp_env_replace: failed to replace\n");
						return 1;
					}

					if (prev) {
						prev->next = p;
					} else {
						*orig = p;
					}
				}
				break;
			case OBJ_PAIR:
				if (sexp_env_replace(&(p->pair), env)) {
					fprintf(stderr, "sexp_env_replace: failed to recurse\n");
					return 1;
				}
				break;
			default:
				fprintf(stderr, "sexp_env_replace: unknown sexp type\n");
				return 1;
				break;
		}

		prev = p;
		p = p->next;
	}
	return 0;
}

static int s_lambda(struct Sexp *s, struct Env *env, struct Sexp **res) {
	//s = (lambda (args) body)
	//s_lambda replaces all the variables in body (excluding formal params)
	//		with what they're bound to in env
	struct Sexp *p = s;
	struct Sexp *tmp = NULL;

	if (len_sexp(s) < 3) {
		fprintf(stderr, "s_lambda: lambda expression must be of length at least 3\n");
		return 1;
	}

	//remove args binding in env
	switch ((s->next)->type) {
		case OBJ_NULL:
			//(lambda () body)
			break;
		case OBJ_ATOM:
			//(lambda x body)
			if (env_unbind(&env, (s->next)->atom)) {
				fprintf(stderr, "s_lambda: can't remove args binding\n");
				return 1;
			}

			break;
		case OBJ_PAIR:
			//(lambda (x ... y) body)
			p = (s->next)->pair;
			while (p) {
				if (p->type == OBJ_NULL) {
					break;
				}
#if DEBUG_S_LAMBDA
				printf("unbinding %s\n", p->atom);
#endif
				env_unbind(&env, p->atom);
				p = p->next;
			}
			break;
		default:
			fprintf(stderr, "s_lambda: malformed args\n");
			return 1;
			break;
	}

	if (!(tmp = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "s_lambda: not enough memory to store result\n");
		return 1;
	}

	if (sexp_cp(tmp, s)) {
		fprintf(stderr, "s_lambda: not enough memory to store result\n");
	}

	if (sexp_env_replace(&((tmp->next)->next), env)) {
		fprintf(stderr, "s_lambda: failed to dereference\n");
		return 1;
	}

	if (!((*res) = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "s_lambda: not enough memory to store result\n");
		return 1;
	}

	(*res)->type = OBJ_PAIR;
	(*res)->pair = tmp;
	(*res)->next = NULL;

	return 0;
}

static int eval(struct Sexp *s, struct Env **env, struct Sexp **res) {
	//only eval the first element of s.
	//on return, if *res != NULL, then you should free it.
	struct Sexp *p = s;
	struct Sexp *pp = NULL;
	struct Sexp *result = NULL;
#if DEBUGEVAL
	printf("in eval\n");
	print_sexp(s);
#endif

	switch (s->type) {
		case OBJ_NULL:
			printf("eval: nil\n");
			break;
		case OBJ_ATOM:
			//do lookup
#if DEBUGEVAL
			printf("eval: atom\t%s\n", s->atom);
#endif

			if (!(strncmp(s->atom, ",p", strlen(",p")+1))) {
				printf("printing environment\n");
				print_env(*env);
				break;
			}

			if (lookup_env(*env, s->atom, &result)) {
#if DEBUGEVAL
				print_sexp(result);
#endif
				if (!(*res = malloc(sizeof(struct Sexp)))) {
					fprintf(stderr, "eval: failed to alloc after lookup\n");
					return 1;
				}

				if (sexp_cp(*res, result)) {
					fprintf(stderr, "eval: failed to copy result after lookup\n");
					return 1;
				}
#ifndef DEBUG
				printf("\n");
#endif
				return 0;
			} else {
				fprintf(stderr, "eval: \"%s\" not found\n", p->atom);
				return 1;
			}

			break;
		case OBJ_PAIR:
			//eval
#if DEBUGEVAL
			printf("eval: pair\n");
#endif
			p = s->pair;

			switch (p->type) {
				case OBJ_NULL:
					printf("eval: calling a non-function!\n");
					return 1;
					break;
				case OBJ_ATOM:
					if (!(strcmp(p->atom, "define"))) {
						*res = NULL;
						return s_define(p, env);
						break;
					} else if (!(strcmp(p->atom, "lambda"))) {
						return s_lambda(p, *env, res);
						break;
					} else if (!(strcmp(p->atom, "undef"))) {
						return env_unbind(env, (p->next)->atom);
						break;
					}
					//fall through;
				case OBJ_PAIR:
					if ((p->pair)->type == OBJ_ATOM && !(strcmp((p->pair)->atom, "lambda"))) {
						//lambda expression
						//do beta reduction
#if DEBUGEVAL
						printf("eval: lambda expression\n");
#endif
						return s_beta_red(p, *env, res);
						break;
					}
					//fall through;
				default:
					//function application (maybe)
					//lookup function and replace it by the definition
#if DEBUGEVAL
					printf("eval: function call\n");
#endif

					while (p) {
						if (p->type == OBJ_NULL) {
							break;
						}

						if (eval(p, env, &result)) {
							printf("eval: failed to eval\n");
							print_sexp(p);
							return 1;
						} else {
							//sub in
							if (sexp_sub(&p, result)) {
								fprintf(stderr, "eval: substitution failed\n");
								return 1;
							}
							free_sexp(result);

							if (pp) {
								pp->next = p;
							} else {
								s->pair = p;
							}
							pp = p;
						}

						p = p->next;
					}

#if DEBUGEVAL
					printf("after sub:\n");
					print_sexp(s);
#endif

					return eval(s, env, res);
					break;
			}
			break;
		default:
			fprintf(stderr, "eval: failed, unknown sexp type\n");
			return 1;
			break;
	}

	//unreachable
#if DEBUGEVAL
	printf("exit eval\n");
#endif
	return 0;
}

static int parse_eval(char *str, struct Env **env) {
	struct Sexp *input = NULL;
#if EVAL
	struct Sexp *p = NULL;
	struct Sexp *res = NULL;
#endif

	if (!(input = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "can't initialize input tree\n");
		exit(1);
	}

	*input = s_null;

	parse_sexp(str, &input, strlen(str));
#if DEBUGPARSE
	print_sexp(input);
#endif
#if EVAL
	p = input;
	while (p) {
		if (p->type == OBJ_NULL) {
			break;
		}

		if (eval(p, env, &res)) {
			fprintf(stderr, "parse_eval: eval failed\n");
		} else {
			if (res) {
				print_sexp(res);
#ifndef DEBUG
				printf("\n");
#endif
				free_sexp(res);
			} else {
				printf("parse_eval: unspecified\n");
			}
		}

		p = p->next;
	}
#endif
	free_sexp(input);

	return 0;
}

static int read_line(FILE *stream, char *buf, size_t bufsiz) {
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

static size_t append_string(char **dest, const char *src) {
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

static int repl() {
	char buf[BUFFER_SIZE];
	char *str = NULL;
#if PARSE || DEBUGPARSE
	char *p = NULL;
	int i = 0;
#endif
	int *level = NULL;
	struct Env *global = NULL;

	if (!(level = calloc(1, sizeof(int)))) {
		fprintf(stderr, "can't initialize level\n");
	}

	if (!(str = malloc(sizeof(char)))) {
		fprintf(stderr, "can't initialize str\n");
		exit(1);
	}

	*level = 0;
	*str = '\0';

	if (new_env_binding(&global, "nil", s_null)) {
		fprintf(stderr, "failed to define global environment\n");
		exit(1);
	}

	while (read_line(stdin, buf, BUFFER_SIZE) == 0) {
#if DEBUGREAD
		fprintf(stdout, "saved %s, read %s\n", str, buf);
		printf("{%lu} in buf, {%lu} in str\n", strlen(buf), strlen(str));
#endif
		if (*buf == ';') {
			continue;
		}

		append_string(&str, buf);
#if DEBUGREAD
		printf("read {%lu} %s\n", strlen(str), str);
#endif
#if PARSE
		i = check_if_sexp(str);
		if (i) {
			i = sexp_end(str, &p, strlen(str));
		}
#endif

#if PARSE
		if (!i) {
#else
		if (0) {
#endif
			parse_eval(str, &global);

			if (!(str = realloc(str, sizeof(char)))) {
				fprintf(stderr, "can't initialize str\n");
				exit(1);
			}

			*str = '\0';
		}
	}

	free_env(global);
	free(str);
	free(level);
	return 0;
}

int main() {
	//printf("main %p\n", (void *) main);
#ifdef MTRACE
	putenv("MALLOC_TRACE=mtrace.log");
	mtrace();
#endif
	signal(SIGSEGV, segfault_handler);
	repl();
#ifdef MTRACE
	muntrace();
#endif
	return 0;
}
