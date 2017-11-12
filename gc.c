#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define STACK_MAX 256
#define INITIAL_GC_THRESHOLD 10


void assert(int test, char msg[]) {
	if (test == 0) {
		fprintf(stderr, "%s\n", msg);
		abort();
	}
	return;
}

enum ObjectType {
	OBJ_INT,
	OBJ_PAIR
};

enum Mark {
	MARKED,
	UNMARKED
};

struct Object {
	struct Object* next;
	enum Mark marked;
	enum ObjectType type;

	union {
		/* OBJ_INT */
		int value;

		/* OBJ_PAIR*/
		struct {
			struct Object *head;
			struct Object *tail;
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

void mark(struct Object *object) {
	if (object->marked == MARKED) return;

	object->marked = MARKED;

	if (object->type == OBJ_PAIR) {
		mark(object->head);
		mark(object->tail);
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
	struct Object *object = newObject(vm, OBJ_INT);
	object->value = intValue;
	push(vm, object);
}

struct Object *pushPair(struct VM* vm) {
	struct Object *object = newObject(vm, OBJ_PAIR);
	object->tail = pop(vm);
	object->head = pop(vm);

	push(vm, object);
	return object;
}

void freeVM(struct VM *vm) {
	vm->maxObjects = 0;
	gc(vm);
	free(vm);
}
/*
int main() {
	struct VM *vm;
	int count = 0;
	vm = newVM();
	while (1) {
		count++;
		printf("push %d\n", count);
		pushInt(vm, count);
		pushInt(vm, count);
		printf("push pair\n");
		pushPair(vm);
		printf("pop %d\n", pop(vm)->type);
		sleep(1);
	}
	free(vm);
	return 0;
}
*/
