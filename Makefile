SRC=$(wildcard *.c)
OBJ=${SRC:.c=.o}

CFLAGS="-g -pipe"
LDFLAGS="-lncurses"

all: sheriff

sheriff: ${OBJ}
	gcc ${LDFLAGS} -o $@ $^

%.o: %.c
	gcc ${CFLAGS} -c -o $@ $<

clean:
	rm -v ${OBJ} sheriff
