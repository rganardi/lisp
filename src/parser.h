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

int match(char, const char *);
int tok(char *, char **, char **, char **, size_t);
int read_line(FILE *, char *, size_t);
size_t append_string(char **, const char *);
int check_if_sexp(char *);
int sexp_end(char *, char **, size_t);
int parse_sexp(char *, struct Sexp **, size_t);
int print_sexp(struct Sexp *);
void free_sexp(struct Sexp *);
int append_sexp(struct Sexp **, struct Sexp *);
size_t len_sexp(struct Sexp *);
int sexp_cp(struct Sexp *, struct Sexp *);
int sexp_sub(struct Sexp **, struct Sexp *);

static const struct Sexp s_null = {
	.type = OBJ_NULL,
	.next = NULL
};
