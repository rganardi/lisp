PROGNAME=lisp

SRCS=$(wildcard src/*.c)
OBJS=${SRCS:.c=.o}
CFLAGS=-g\
       -std=c11\
       -pedantic\
       -Wall\
       -DPARSE\
       -DEVAL\
       -DMTRACE\
       -DDEBUG\
       -DDEBUGEVAL\
       #-DDEBUGPARSE\
       #-DDEBUGREAD\
       #-DDEBUGSEXP_END\

LDFLAGS=-lbsd

.PHONY: tags clean

PROGNAME: ${OBJS} tags
	cc ${CFLAGS} -o ${PROGNAME} ${OBJS} ${LDFLAGS}

tags: ${SRCS}
	ctags -R src/

clean:
	rm ${PROGNAME} ${OBJS}
