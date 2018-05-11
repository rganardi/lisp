#define SEXP_BEGIN	"("
#define SEXP_END	")"
#define SEXP_DELIM	" \t"

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

int match(char c, const char *bytes);
int tok(char *str, char **start, char **end, char **next, size_t n);
int read_line(FILE *stream, char *buf, size_t bufsiz);
size_t append_string(char **dest, const char *src);
int check_if_sexp(char *str);
int sexp_end(char *str, char **end, size_t n);
int parse_sexp(char *str, struct Sexp **tree, size_t n);
int print_sexp(struct Sexp *s);
void free_sexp(struct Sexp *s);
int append_sexp(struct Sexp **dst, struct Sexp *src);
size_t len_sexp(struct Sexp *s);
int sexp_cp(struct Sexp *dst, struct Sexp *src);
int sexp_sub(struct Sexp **orig, struct Sexp *new);
int sexp_replace_all(struct Sexp **orig, char *name, struct Sexp *new);

static const struct Sexp s_null = {
	.type = OBJ_NULL,
	.next = NULL
};
