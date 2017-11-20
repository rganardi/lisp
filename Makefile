PROGNAME=lisp

SRC=$(wildcard *.c)
OBJ=${SRC:.c=.o}
CFLAGS=-Wall\
       -lbsd\
       -DDEBUG\
       -DPARSE\
       #-DDEBUGCAT\

PROGNAME: ${OBJ}
	cc ${CFLAGS} -o ${PROGNAME} ${OBJ}

.c.o:

clean:
	rm ${PROGNAME} ${OBJ}
