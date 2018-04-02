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
       -Wno-unused-function\
       #-DDEBUGEVAL\
       #-DDEBUG_DEFINE\
       #-DDEBUG_S_BETA_RED\
       #-DDEBUG_S_LAMBDA\
       #-DDEBUG_SEXP_CP\
       #-DDEBUG_PARSE_EVAL\
       #-DDEBUG_PRINT_SEXP\
       #-rdynamic\
       #-DDEBUG_ENV_UNBIND\
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
