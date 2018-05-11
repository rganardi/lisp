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
void segfault_handler(int error);
int dump_string(char *str, size_t n);
#endif
int new_env_binding(struct Env **env, char *name, struct Sexp s_object);
int env_cp(struct Env **dst, struct Env src);
int env_rebind(struct Env **env, char *name, struct Sexp new_object);
int env_unbind(struct Env **env, char *name);
void free_env(struct Env *env);
int print_env(struct Env *env);
int lookup_env(struct Env *env, char *name, struct Sexp **s);
int sexp_replace_all(struct Sexp **orig, char *name, struct Sexp *new);
int sexp_env_replace(struct Sexp **orig, struct Env *env);
int s_beta_red(struct Sexp *s, struct Env *env, struct Sexp **res);
int s_define(struct Sexp *s, struct Env **env, struct Sexp **res);
int s_lambda(struct Sexp *s, struct Env **env, struct Sexp **res);
int s_quote(struct Sexp *s, struct Env **env, struct Sexp **res);
int s_undef(struct Sexp *s, struct Env **env, struct Sexp **res);
int eval(struct Sexp *s, struct Env **env, struct Sexp **res);
int parse_eval(char *str, struct Env **env);
int repl();
int main(int argc, char *argv[]);

char flag = 0;

static const struct {
	char *s;
	int (*handler)(struct Sexp *s, struct Env **env, struct Sexp **res);
} handler_map[] = {
	{"define", s_define},
	{"lambda", s_lambda},
	{"quote", s_quote},
	{"undef", s_undef},
};
