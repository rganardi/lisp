PROGNAME=lisp

SRCS=$(wildcard src/*.c)
OBJS=${SRCS:.c=.o}
CFLAGS=-g\
       -Wall\
       -lbsd\
       -DPARSE\
       -DEVAL\
       #-DDEBUGPARSE\
       #-DDEBUGSEXP_END\

PROGNAME: ${OBJS} tags
	cc ${CFLAGS} -o ${PROGNAME} ${OBJS}

tags: ${SRCS}
	ctags -R src/

clean:
	rm ${PROGNAME} ${OBJS}
