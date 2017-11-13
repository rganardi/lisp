#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <bsd/string.h>
#include <stdlib.h>

#define BUFFER_SIZE 512
#define MAX_TOKENS 128

enum ObjectType {
	OBJ_PAIR,
	OBJ_INT
};

struct Pair {
	struct Object *car;
	struct Object *cdr;
};

struct Object {
	enum ObjectType type;

	union {
		/* OBJ_INT */
		int i;

		/* OBJ_PAIR */
		struct Pair p;
	};
};

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
		buf[i - 1] = '\0'; /* eliminates '\n' */
	}
	buf[i] = '\0';
	return 0;
}

static int tokenize(char *str, char **tok) {
	int i = 0;
	char *sexp_begin = NULL;
	char *sexp_end = NULL;

	sexp_begin = strchr(str, '(');
	sexp_end = strrchr(str, ')');

	if (sexp_begin == NULL || sexp_end == NULL) {
		fprintf(stderr, "it's not a sexp\n");
		return 1;
	}
#if DEBUG
	printf("begin %.2s ... %.2s end\n", sexp_begin++, --sexp_end);
#endif
	tok[i] = strtok(str, " \t");
	if (tok[i] == NULL) {
		fprintf(stderr, "can't parse input %s\n", str);
		return 1;
	}

	while (tok[i] != NULL && i < MAX_TOKENS - 1) {
		tok[++i] = strtok(NULL, " \t");
	}
	return 0;
}

static int walk(char *str, int *level) {
	char *c = NULL;

	for (c = str; *c != '\0'; c++) {
		if (*c == '(') {
			(*level)++;
		} else if (*c == ')') {
			(*level)--;
		}

		if (*level < 0) {
			fprintf(stderr, "unbalanced parens\n");
			*level = 0;
			while (*c != '\0') {
				*c = '\0';
				c++;
			}
			return 1;
		}
#if VDEBUG
		printf("%d", *level);
#endif
	}
	return 0;
}

int repl() {
	char *buf;
	char *str;
	char *tok[MAX_TOKENS];
	int i = 0;
	int err = 0;
	int *level;


	if (!(level = calloc(1, sizeof(int)))) {
		fprintf(stderr, "can't initialize level\n");
	}

	while (i < MAX_TOKENS) {
		tok[i++] = NULL;
	}
	if (!(buf = calloc(BUFFER_SIZE, sizeof(buf)))) {
		fprintf(stderr, "can't initialize buf\n");
		exit(1);
	}
	if (!(str = calloc(BUFFER_SIZE, sizeof(str)))) {
		fprintf(stderr, "can't initialize str\n");
		exit(1);
	}

	while (read_line(STDIN_FILENO, buf, BUFFER_SIZE) == 0) {
#if DEBUG
		fprintf(stdout, "saved %s, read %s\n", str, buf);
#endif

		err = walk(buf, level);
		if (err) {
			fprintf(stderr, "error walking\n");
		}

		if (!(realloc(str, strlen(str) + BUFFER_SIZE))) {
			fprintf(stderr, "can't resize str\n");
		}
		i = strlcat(str, buf, strlen(str) + BUFFER_SIZE);
#if VDEBUG
		printf("copied %d\n", i);
#endif
		if (i >= strlen(str) + BUFFER_SIZE) {
			fprintf(stderr, "can't append current line\n");
		}

		if (*level == 0 && i > 0) {
#if DEBUG
			printf("tokenizing %s\n", str);
#endif
			tokenize(str, tok);
			for (i = 0; i < MAX_TOKENS ; i++) {
				if (!(tok[i])) {
					break;
				}
#if DEBUG
				printf("token: %s\n", tok[i]);
#endif
			}
			tok[0] = NULL;
			free(str);
			str = calloc(BUFFER_SIZE, sizeof(str));
		}
	}
	free(str);
	free(level);
	free(buf);
	return 0;
}
