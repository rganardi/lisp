PROGNAME:=lisp

SRCS:=$(wildcard src/*.c)
OBJS:=${SRCS:.c=.o}
all-tests:=$(addsuffix .test, $(basename $(wildcard tests/*.in)))
CFLAGS:=-g\
       -std=c11\
       -pedantic\
       -Wall\
       -DPARSE\
       -DEVAL\
       -DDEBUG\
       #-Wno-unused-function\
       #-DTRACE\
       #-DMTRACE\
       #-DDEBUG_S_BETA_RED\
       #-DDEBUGEVAL\
       #-DDEBUG_DEFINE\
       #-DDEBUG_S_LAMBDA\
       #-DDEBUG_SEXP_CP\
       #-DDEBUG_PARSE_EVAL\
       #-DDEBUG_PRINT_SEXP\
       #-rdynamic\
       #-DDEBUG_ENV_UNBIND\
       #-DDEBUGPARSE\
       #-DDEBUGREAD\
       #-DDEBUGSEXP_END\

LDFLAGS:=-lbsd

.PHONY: tags clean test all

PROGNAME: ${OBJS} tags
	cc ${CFLAGS} -o ${PROGNAME} ${OBJS} ${LDFLAGS}

tags: ${SRCS}
	ctags -R src/

clean:
	rm ${PROGNAME} ${OBJS}

check : $(all-tests)
	@echo "all test passed."

%.test : %.in %.out ${PROGNAME}
	@$(abspath $(PROGNAME)) <$< 2>&1 | diff -q $(word 2, $?) - >/dev/null || \
		(echo "Test $(<F) failed" && exit 1)
