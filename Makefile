PROGNAME=lisp

SRC=$(wildcard src/*.c)
OBJ=${SRC:.c=.o}
CFLAGS=-Wall\
       -lbsd\
       -DPARSE\
       -DEVAL\
       #-DDEBUGFREE_SEXP

PROGNAME: ${OBJ} tags
	cc ${CFLAGS} -o ${PROGNAME} ${OBJ}

tags:
	ctags -R src/

clean:
	rm ${PROGNAME} ${OBJ}
