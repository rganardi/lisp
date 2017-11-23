#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <bsd/string.h>

#define STACK_MAX 256
#define INITIAL_GC_THRESHOLD 10
#define BUFFER_SIZE 512
#define MAX_TOKENS 128

void assert(int test, char msg[]) {
	if (test == 0) {
		fprintf(stderr, "%s\n", msg);
		abort();
	}
	return;
}


enum ObjectType {
	OBJ_PAIR,
	OBJ_ATOM
};

enum Mark {
	MARKED,
	UNMARKED
};

struct Sexp {
	enum ObjectType type;

	union {
		char *a;

		struct {
			struct Sexp *car;
			struct Sexp *cdr;
		};
	};
};

struct Object {
	struct Object* next;
	enum Mark marked;
	enum ObjectType type;

	union {
		/* OBJ_ATOM */
		char *a;

		/* OBJ_PAIR*/
		struct {
			struct Object *car;
			struct Object *cdr;
		};
	};
};

struct VM {
	int numObjects;
	int maxObjects;
	struct Object *firstObject;
	struct Object *stack[STACK_MAX];
	int stackSize;
};

struct Lambda {
	char **args;
	char *body;
};


void mark(struct Object *object) {
	if (object->marked == MARKED) return;

	object->marked = MARKED;

	if (object->type == OBJ_PAIR) {
		mark(object->car);
		mark(object->cdr);
	}
}

void markAll(struct VM *vm) {
	for (int i = 0; i < vm->stackSize; i++) {
		mark(vm->stack[i]);
	}
}

void sweep(struct VM *vm) {
	struct Object **object = &vm->firstObject;
	while (*object) {
		if ((*object)->marked != MARKED) {
			/* This isn't reachable, sweep! */
			struct Object *unreached = *object;

			*object = unreached->next;
			free(unreached);
			--vm->numObjects;
		} else {
			/* reachable */
			(*object)->marked = UNMARKED;
			object = &(*object)->next;
		}
	}
}

void gc(struct VM *vm) {
	printf("pre gc, %d/%d objects alive\n", vm->numObjects, vm->maxObjects);

	markAll(vm);
	sweep(vm);

	vm->maxObjects = (vm->numObjects * 2) + 1;
	printf("post gc, %d/%d objects alive\n", vm->numObjects, vm->maxObjects);
}

struct VM *newVM() {
	struct VM *vm = malloc(sizeof(struct VM));
	vm->stackSize = 0;
	vm->numObjects = 0;
	vm->maxObjects = INITIAL_GC_THRESHOLD;
	return vm;
}

void push(struct VM *vm, struct Object *value) {
	assert(vm->stackSize < STACK_MAX, "Stack overflow!");
	vm->stack[vm->stackSize++] = value;
}

struct Object *pop(struct VM *vm) {
	assert(vm->stackSize > 0, "Stack underflow!");
	return vm->stack[--vm->stackSize];
}

struct Object *newObject(struct VM *vm, enum ObjectType type) {
	if (vm->numObjects == vm->maxObjects) gc(vm);

	struct Object *object = malloc(sizeof(struct Object));
	object->type = type;
	object->marked = UNMARKED;

	object->next = vm->firstObject;
	vm->firstObject = object;

	vm->numObjects++;

	return object;
}

void pushInt(struct VM *vm, int intValue) {
	struct Object *object = newObject(vm, OBJ_ATOM);
	object->a = calloc(1, sizeof(int));
	sprintf(object->a, "%d", intValue);
	push(vm, object);
}

struct Object *pushPair(struct VM* vm) {
	struct Object *object = newObject(vm, OBJ_PAIR);
	object->cdr = pop(vm);
	object->car = pop(vm);

	push(vm, object);
	return object;
}

void freeVM(struct VM *vm) {
	vm->stackSize = 0;
	gc(vm);
	free(vm);
}

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
		buf[i - 1] = ' '; /* eliminates '\n' */
	}
	buf[i] = '\0';
	return 0;
}

/*
static int tokenize(char *str, char **tok) {
	int i = 0;

#if DEBUG
	char *sexp_begin = NULL;
	char *sexp_end = NULL;

	sexp_begin = strchr(str, '(');
	sexp_end = strrchr(str, ')');

	if (sexp_begin == NULL || sexp_end == NULL) {
		fprintf(stderr, "it's not a sexp\n");
		return 1;
	}

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
*/

int tok(char **str, char **tok) {
	char *p;

	while (*(*str) == ' ' || *(*str) == '\t') {
		(*str)++;
	}

	*tok = *str;
	p = *str;

	while (*p != ' ' && *p != '\t') {
		if (*(p++) == '\0') {
			*str = NULL;
			return 1;
		}
	}
	*p = '\0';
	*str = ++p;
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

void print_Sexp(struct Sexp *s) {
	switch (s->type) {
		case OBJ_ATOM:
			printf("{%s}", s->a);
			break;
		case OBJ_PAIR:
			print_Sexp(s->car);
			print_Sexp(s->cdr);
			break;
		default:
			printf("unknown sexp\n");
	}
	return;

	//if (s->type == OBJ_ATOM) {
	//	printf("%s", s->a);
	//} else if (s->type == OBJ_PAIR) {
	//	print_Sexp(s->car);
	//	print_Sexp(s->cdr);
	//}
	//return;
}

struct Sexp *newSexp(enum ObjectType t) {
	struct Sexp *s;
	s = calloc(1, sizeof(struct Sexp));

	s->type = t;

	return s;
}

void rmSexp(struct Sexp *s) {
	switch (s->type) {
		case OBJ_ATOM:
			free(s->a);
			break;
		case OBJ_PAIR:
			rmSexp(s->car);
			rmSexp(s->cdr);
	}

	return;
}

static int parse(char *str, struct Sexp *tree) {
	//char *tok[MAX_TOKENS];
	//int i = 0;
	char *s;
	char *t;

#if DEBUG
	printf("tokenizing %s\n", str);
#endif

	//while (i < MAX_TOKENS) {
	//	tok[i++] = NULL;
	//}

	//if (tokenize(str, tok)) {
	//	return 1;
	//}

	s = str;
	tok(&s, &t);

	if (!s) {
		/* OBJ_ATOM */
		/*
		tree->type = OBJ_ATOM;
		tree->a = t;
		*/
		tree = newSexp(OBJ_ATOM);
		tree->a = strdup(t);

		//fprintf(stderr, "nothing to parse\n");
	} else {
		/* OBJ_PAIR */
		//tree->type = OBJ_PAIR;

		tree = newSexp(OBJ_PAIR);
		struct Sexp *car = newSexp(OBJ_ATOM);
		car->a = strdup(t);
		struct Sexp *cdr;
		parse(s, cdr);
		//if (*s == '(') {
		//	parse(s, cdr);
		//} else {
		//	cdr = newSexp(OBJ_ATOM);
		//	cdr->a = s;
		//}

		tree->car = car;
		tree->cdr = cdr;

		print_Sexp(tree);
		/*
		struct Sexp *car = malloc(sizeof(struct Sexp));
		struct Sexp *cdr = malloc(sizeof(struct Sexp));

		car->type = OBJ_ATOM;

		car->a = t;

		if (*s == '(') {
			cdr->type = OBJ_PAIR;
			parse(s, cdr);
		} else {
			cdr->type = OBJ_ATOM;
			cdr->a = s;
		}

		tree->car = car;
		tree->cdr = cdr;

		free(car);
		free(cdr);
		*/

		/*
		struct Sexp car;
		struct Sexp cdr;

		car.type = OBJ_ATOM;
		car.a = t;

		if (*s == '(') {
			cdr.type = OBJ_PAIR;
			parse(s, &cdr);
		} else {
			cdr.type = OBJ_ATOM;
			cdr.a = s;
		}

		tree->car = &car;
		tree->cdr = &cdr;
		print_Sexp(tree);
		*/
	}

#if DEBUG
	printf("done tokenizing\n");
#endif



	//for (i = 0; i < MAX_TOKENS ; i++) {
	//	if (!(tok[i])) {
	//		break;
	//	}
#if DEBUG
	//	printf("token: %s\n", tok[i]);
#endif
	//}
	return 0;
}

static int eval(char *str) {
	struct Sexp *tree;

	//tree = malloc(sizeof(struct Sexp));

	if (parse(str, tree)) {
		return 1;
	}
	//puts((tree->car)->a);
	//puts((tree->cdr)->a);
	print_Sexp(tree);

	rmSexp(tree);


	return 0;
}

int repl() {
	char *buf;
	char *str;
	size_t i = 0;
	int err = 0;
	int *level;


	if (!(level = calloc(1, sizeof(int)))) {
		fprintf(stderr, "can't initialize level\n");
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
			eval(str);
			free(str);
			str = calloc(BUFFER_SIZE, sizeof(str));
		}
	}
	free(str);
	free(level);
	free(buf);
	return 0;
}
int main () {
	struct VM *vm;
	vm = newVM();
	repl();
	freeVM(vm);
	return 0;
}
