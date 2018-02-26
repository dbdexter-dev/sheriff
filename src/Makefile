SRC=$(wildcard *.c)
OBJ=${SRC:.c=.o}

LDFLAGS += -lncursesw -pthread

.PHONY: default clean strip

default: sheriff
clean:
	rm -f ${OBJ} sheriff

strip: sheriff
	strip $^

sheriff: ${OBJ}
	gcc ${LDFLAGS} -o $@ $^

sheriff.o: sheriff.c config.h
	gcc ${CFLAGS} -c -o $@ $<

%.o: %.c
	gcc ${CFLAGS} -c -o $@ $<


