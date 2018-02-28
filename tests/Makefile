SRC=$(wildcard *.c)
MAINSRC=$(wildcard ../src/*.c)
OBJ=${SRC:.c=.o}

.PHONY: default clean

default: test

test: ${OBJ}
	gcc ${LDFLAGS} -o $@ $^

clean:
	rm -f ${OBJ} test

%.o: %.c
	gcc ${CFLAGS} -c -o $@ $<
