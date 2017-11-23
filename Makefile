PROGNAME=lisp

SRC=$(wildcard src/*.c)
OBJ=${SRC:.c=.o}
CFLAGS=-Wall\
       -lbsd\
       -DPARSE\
       -DEVAL

PROGNAME: ${OBJ}
	cc ${CFLAGS} -o ${PROGNAME} ${OBJ}

.c.o:

clean:
	rm ${PROGNAME} ${OBJ}
