SRC=$(wildcard *.c)
OBJ=${SRC:.c=.o}

CFLAGS=-g -pipe
LDFLAGS=-lncurses

all: sheriff

sheriff: ${OBJ}
	gcc ${LDFLAGS} -o $@ $^

sheriff.o: sheriff.c config.h
	gcc ${CFLAGS} -c -o $@ $<

%.o: %.c
	gcc ${CFLAGS} -c -o $@ $<

clean:
	rm -fv ${OBJ} sheriff
