#ifdef MTRACE
#define _XOPEN_SOURCE
#include <mcheck.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <bsd/string.h>

char *argv0;
#include "arg.h"
#include "parser.h"
#include "lisp.h"

//#include "strlcat.c"

#if DEBUG
void segfault_handler(int error) {
	char buf[23] = "SEGFAULT, bad program\n";
	write(STDERR_FILENO, buf, 23);
	fflush(stdout);
	_exit(EXIT_FAILURE);
}

int dump_string(char *str, size_t n) {
	size_t i = 0;
	char *p = str;

	for (p = str; i < n; p++, i++) {
		printf("%p\t%#x\t%c\n", (void *) p, *p, *p);
	}

	return i;
}
#endif

int new_env_binding(struct Env **env, char *name, struct Sexp s_object) {
	struct Env *e = NULL;
	struct Sexp *s = NULL;
	char *str = NULL;

	if (!(e = malloc(sizeof(struct Env)))) {
		fprintf(stderr, "new_env_binding: can't allocate memory for new environment\n");
		return 1;
	}

	if (!(s_object.next = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "new_env_binding: can't allocate memory for new environment\n");
		return 1;
	}
	*(s_object.next) = s_null;

	if (!(s = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "new_env_binding: can't allocate memory for new environment\n");
		return 1;
	}

	if(sexp_cp(s, &s_object)) {
		fprintf(stderr, "new_env_binding: failed to copy env\n");
		return 1;
	}

	free_sexp(s_object.next);

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

int env_cp(struct Env **dst, struct Env src) {
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

int env_rebind(struct Env **env, char *name, struct Sexp new_object) {
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

int env_unbind(struct Env **env, char *name) {
	struct Env *pp = *env;
	struct Env **p = env;

#if DEBUG_ENV_UNBIND
	printf("env_unbind: unbinding \"%s\"\n", name);
	printf("before\n");
	print_env(*env);
#endif
	while (*p) {
		if (!(strcmp((*p)->name, name))) {
			pp = *p;
			*p = (*p)->next;

			pp->next = NULL;
			free_env(pp);
			continue;
		}
		p = &(*p)->next;
	}
#if DEBUG_ENV_UNBIND
	printf("after\n");
	print_env(*env);
#endif

	return 0;
}

void free_env(struct Env *env) {
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

int print_env(struct Env *env) {
	while (env) {
#if DEBUG_PRINT_ENV
		printf("env->name\n");
		dump_string(env->name, strlen(env->name)+1);
		printf("env->s_object\n");
		print_sexp(env->s_object);
		printf("\n");
#else
		printf("%s\t->\t", env->name);
		print_sexp(env->s_object);
		printf("\n");
#endif

		env = env->next;
	}
	return 0;
}

int lookup_env(struct Env *env, char *name, struct Sexp **s) {
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

int sexp_env_replace(struct Sexp **orig, struct Env *env) {
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
				//special case for lambda expression
				if ((p->pair)->type == OBJ_ATOM
						&& !(strcmp((p->pair)->atom, "lambda"))) {
					if (s_lambda(p->pair, &env, &result)) {
						fprintf(stderr, "sexp_env_replace: failed to handle lambda\n");
						return 1;
					}
					if (sexp_sub(&p, result)) {
						fprintf(stderr, "sexp_env_replace: failed to sub\n");
						return 1;
					}
					free_sexp(result);

					if (prev) {
						prev->next = p;
					} else {
						*orig = p;
					}
				} else if (sexp_env_replace(&(p->pair), env)) {
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

int s_beta_red(struct Sexp *s, struct Env *env, struct Sexp **res) {
	// s = ((lambda (x) body) args)
	struct Sexp *p = NULL;
	struct Sexp *arg = NULL;
	struct Env *e = NULL;
	struct Sexp *pp = NULL;
	struct Sexp *args = NULL;
	struct Env *tmp_e = NULL;

	if (len_sexp(s->pair) < 3) {
		fprintf(stderr, "s_beta_red: lambda expression must be of length at least 3\n");
		return 1;
	}

#if DEBUG_S_BETA_RED
	printf("s_beta_red: copy env\n");
#endif
	if (env_cp(&e, *env)) {
		fprintf(stderr, "s_beta_red: failed to copy env\n");
		return 1;
	}
#if DEBUG_S_BETA_RED
	printf("s_beta_red: copy env done, got this\n");
	print_env(e);
	printf("s_beta_red: bind args to formal params\n");
#endif

	//do beta reduction
	switch (((s->pair)->next)->type) {
		case OBJ_NULL:
			//(lambda () body)
			break;
		case OBJ_ATOM:
			//(lambda x body)
			if (!(args = malloc(sizeof(struct Sexp)))) {
				fprintf(stderr, "s_beta_red: failed to alloc for args\n");
				free_env(e);
				return 1;
			}

			if (sexp_cp(args, s->next)) {
				fprintf(stderr, "s_beta_red: failed to make copy of args\n");
				free_env(e);
				return 1;
			}
			arg = args;
			pp = NULL;

			while (arg) {
				if (arg->type == OBJ_NULL) {
					break;
				}

				if (eval(arg, &e, res)) {
					fprintf(stderr, "s_beta_red: eval failed\n");
					free_env(e);
					return 1;
				}

				if ((*res) && sexp_sub(&arg, *res)) {
					fprintf(stderr, "s_beta_red: sub in failed\n");
					free_env(e);
					return 1;
				}

				free_sexp(*res);
				*res = NULL;

				if (pp) {
					pp->next = arg;
				} else {
					args = arg;
				}
				pp = arg;
				arg = arg->next;
			}

			if (!(p = malloc(sizeof(struct Sexp)))) {
				fprintf(stderr, "s_beta_red: not enough memory\n");
				free_env(e);
				return 1;
			}

			p->type = OBJ_PAIR;
			p->pair = args;
			p->next = NULL;

			if (new_env_binding(&e,
						(((s->pair)->next)->atom),
						*p)) {

				fprintf(stderr, "s_beta_red: failed to bind arguments to formal params\n");
				free_env(e);
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
					free_env(e);
					return 1;
				}

				if (env_cp(&tmp_e, *env)) {
					fprintf(stderr, "s_beta_red: failed to copy env\n");
					return 1;
				}

				if (eval(arg, &tmp_e, res)) {
					fprintf(stderr, "s_beta_red: eval failed\n");
					free_env(e);
					return 1;
				}

				free_env(tmp_e);
				tmp_e = NULL;

				if ((*res) && new_env_binding(&e,
							p->atom,
							*(*res))) {
					fprintf(stderr, "s_beta_red: failed to bind arguments to formal params\n");
					free_env(e);
					return 1;
				}

				if (*res) {
					free_sexp(*res);
					*res = NULL;
				}

				p = p->next;
				arg = arg->next;
			}
			break;
		default:
			fprintf(stderr, "s_beta_red: unrecognized lambda expression\n");
			free_env(e);
			return 1;
	}
#if DEBUG_S_BETA_RED
	printf("s_beta_red: binding done\n");
	printf("s_beta_red: current env is\n");
	print_env(e);
	printf("s_beta_red: do eval\n");
#endif

	//eval body
	p = (((s->pair)->next)->next);
	while (p) {
		if (p->type == OBJ_NULL) {
			break;
		}
		if (*res) {
			free_sexp(*res);
		}
#if DEBUG_S_BETA_RED
		printf("s_beta_red: current env is\n");
		print_env(e);
#endif

		if (eval(p, &e, res)) {
			fprintf(stderr, "s_beta_red: eval failed\n");
			free_env(e);
			return 1;
		}
		p = p->next;
	}

	free_env(e);

	return 0;
}

int s_define(struct Sexp *s, struct Env **env, struct Sexp **res) {
	struct Sexp *p = NULL;
	struct Sexp *tmp = NULL;

	*res = NULL;

	if (len_sexp(s) != 3) {
		fprintf(stderr, "s_define: define takes exactly two args\n");
		return 1;
	}

	s = s->next;
	if (s->type != OBJ_ATOM) {
		fprintf(stderr, "s_define: name must be an atom\n");
		return 1;
	}

#if DEBUG_DEFINE
	printf("s_define: eval atom\n");
#endif
	if (eval(s->next, env, &tmp)) {
		fprintf(stderr, "s_define: failed to eval the sexp\n");
		return 1;
	}

	if (lookup_env(*env, s->atom, &p)) {
		if (env_rebind(env, s->atom, *tmp)) {
			fprintf(stderr, "s_define: rebind failed\n");
			free_sexp(tmp);
			return 1;
		}
	} else {
		if (new_env_binding(env, s->atom, *tmp)) {
			fprintf(stderr, "s_define: new binding failed\n");
			free_sexp(tmp);
			return 1;
		}
	}
#if DEBUG_DEFINE
	print_env(*env);
	printf("s_define: quitting\n");
#endif
	free_sexp(tmp);
	return 0;
}

int s_lambda(struct Sexp *s, struct Env **env, struct Sexp **res) {
	//s = (lambda (args) body)
	//s_lambda replaces all the variables in body (excluding formal params)
	//		with what they're bound to in env
	struct Sexp *p = s;
	struct Env *e = NULL;

	if (len_sexp(s) < 3) {
		fprintf(stderr, "s_lambda: lambda expression must be of length at least 3\n");
		return 1;
	}

	//remove args binding in env
#if DEBUG_S_LAMBDA
	printf("s_lambda: copying this env\n");
	print_env(*env);
	printf("s_lambda: copying\n");
#endif
	if (env_cp(&e, **env)) {
		fprintf(stderr, "s_lambda: failed to copy env\n");
		return 1;
	}
#if DEBUG_S_LAMBDA
	printf("s_lambda: done copy env\n");
#endif

	switch ((s->next)->type) {
		case OBJ_NULL:
			//(lambda () body)
			break;
		case OBJ_ATOM:
			//(lambda x body)
			env_unbind(&e, (s->next)->atom);
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
				env_unbind(&e, p->atom);
				p = p->next;
			}
			break;
		default:
			fprintf(stderr, "s_lambda: malformed args\n");
			free_env(e);
			return 1;
			break;
	}

	if (!((*res) = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "s_lambda: not enough memory to store result\n");
		return 1;
	}

	(*res)->type = OBJ_PAIR;

	if (!((*res)->pair = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "s_lambda: not enough memory to store result\n");
		free(*res);
		return 1;
	}

	if (sexp_cp((*res)->pair, s)) {
		fprintf(stderr, "s_lambda: not enough memory to store result\n");
		free_sexp(*res);
		free_env(e);
		return 1;
	}

	if (sexp_env_replace(&((((*res)->pair)->next)->next), e)) {
		fprintf(stderr, "s_lambda: failed to dereference\n");
		free_sexp(*res);
		free_env(e);
		return 1;
	}

	free_env(e);

	if (!((*res)->next = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "s_lambda: not enough memory to store result\n");
		free_sexp(*res);
		return 1;
	}
	*((*res)->next) = s_null;

#if DEBUG_S_LAMBDA
	printf("s_lambda: returning\n");
	print_sexp(*res);
	printf("s_lambda: quitting\n");
#endif
	return 0;
}

int s_quote(struct Sexp *s, struct Env **env, struct Sexp **res) {
	if (!(*res = malloc(sizeof(struct Sexp)))) {
		fprintf(stderr, "eval: not enough memory\n");
		return 1;
	}
#if DEBUG_S_QUOTE
	printf("eval: quote quitting\n");
#endif
	return sexp_cp(*res, s->next);
}

int s_undef(struct Sexp *s, struct Env **env, struct Sexp **res) {
	*res = NULL;
	return env_unbind(env, (s->next)->atom);
}

int eval(struct Sexp *s, struct Env **env, struct Sexp **res) {
	//only eval the first element of s.
	//on return, if *res != NULL, then you should free it.
	struct Sexp *p = s;
	struct Sexp *result = NULL;
	int i = 0;
#if DEBUGEVAL
	printf("in eval\n");
	print_sexp(s);
	printf("with env\n");
	print_env(*env);
#endif
	if (flag & TRACE_FLAG) {
		printf("trace: ");
		p = s->next;
		s->next = NULL;
		print_sexp(s);
		s->next = p;
		p = s;
		printf("\n");
	}

	if (flag & ENV_FLAG) {
		printf("with env\n");
		print_env(*env);
		printf("\n");
	}

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
				if (!(*res = malloc(sizeof(struct Sexp)))) {
					fprintf(stderr, "eval: failed to alloc after lookup\n");
					return 1;
				}

#if DEBUGEVAL
				printf("eval: got result\n");
				print_sexp(result);
#endif

				if (sexp_cp(*res, result)) {
					fprintf(stderr, "eval: failed to copy result after lookup\n");
					free(*res);
					return 1;
				}

#if DEBUGEVAL
				printf("eval: lookup done\n");
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
					for (i = 0; i < LEN(handler_map); i++) {
						if (!(strcmp(p->atom, handler_map[i].s))) {
							return handler_map[i].handler(p, env, res);
							break;
						}
					}
					//fall through;
				case OBJ_PAIR:
					if ((p->pair)->type == OBJ_ATOM && !(strcmp((p->pair)->atom, "lambda"))) {
						//lambda expression
						//do beta reduction
#if DEBUGEVAL
						printf("eval: beta reduction\n");
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

					if (p->type == OBJ_NULL) {
						break;
					}

					if (eval(p, env, &result)) {
						printf("eval: failed to eval\n");
						print_sexp(p);
						return 1;
					} else {
						//sub in
#if DEBUGEVAL
						printf("eval: subbing in result\n");
#endif
						if (sexp_sub(&p, result)) {
							fprintf(stderr, "eval: substitution failed\n");
							return 1;
						}
						free_sexp(result);
						result = NULL;
#if DEBUGEVAL
						printf("eval: subbing in done\n");
#endif
						s->pair = p;
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

int parse_eval(char *str, struct Env **env) {
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
#if DEBUG_PARSE_EVAL
		printf("parse_eval: new token\n");
#endif
		if (p->type == OBJ_NULL) {
			break;
		}

		if (eval(p, env, &res)) {
			fprintf(stderr, "parse_eval: eval failed\n");
		} else {
			if (res) {
#if DEBUG_PARSE_EVAL
				printf("parse_eval: result is\n");
#endif
				print_sexp(res);
#ifndef DEBUG_PRINT_SEXP
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

int repl() {
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

int main(int argc, char *argv[]) {
#ifdef MTRACE
	putenv("MALLOC_TRACE=mtrace.log");
	mtrace();
#endif
	signal(SIGSEGV, segfault_handler);

	ARGBEGIN {
		case 't':
			flag = flag ^ TRACE_FLAG;
			break;
		case 'e':
			flag = flag ^ ENV_FLAG;
			break;
	} ARGEND;

	repl();
#ifdef MTRACE
	muntrace();
#endif
	return 0;
}
