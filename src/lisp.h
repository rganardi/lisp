#define BUFFER_SIZE	256
#define LEN(x)	(sizeof(x)/sizeof(*(x)))

#define TRACE_FLAG	1 << 0
#define ENV_FLAG	1 << 1

struct Env {
	struct Env *next;
	struct Sexp *s_object;
	char *name;
};

#if DEBUG
void segfault_handler(int);
int dump_string(char *, size_t);
#endif
int new_env_binding(struct Env **, char *, struct Sexp);
int env_cp(struct Env **, struct Env);
int env_rebind(struct Env **, char *, struct Sexp);
int env_unbind(struct Env **, char *);
void free_env(struct Env *);
int print_env(struct Env *);
int lookup_env(struct Env *, char *, struct Sexp **);
int sexp_env_replace(struct Sexp **, struct Env *);
int s_beta_red(struct Sexp *, struct Env *, struct Sexp **);
int s_define(struct Sexp *, struct Env **, struct Sexp **);
int s_lambda(struct Sexp *, struct Env **, struct Sexp **);
int s_quote(struct Sexp *, struct Env **, struct Sexp **);
int s_undef(struct Sexp *, struct Env **, struct Sexp **);
int eval(struct Sexp *, struct Env **, struct Sexp **);
int parse_eval(char *, struct Env **);
int repl();
int main(int, char *);
