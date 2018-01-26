SRC=$(wildcard *.c)
OBJ=${SRC:.c=.o}

CFLAGS = -pipe
LDFLAGS = -lncurses

all: debug

debug : CFLAGS += -g
debug: sheriff

release : CFLAGS += -O2 -march=native -mtune=native
release: sheriff strip

.PHONY: clean all debug release strip

strip: sheriff
	strip $^

sheriff: ${OBJ}
	gcc ${LDFLAGS} -o $@ $^

# Custom rule for sheriff.o so that config.h is
# taken into consideration
sheriff.o: sheriff.c config.h
	gcc ${CFLAGS} -c -o $@ $<

%.o: %.c
	gcc ${CFLAGS} -c -o $@ $<

clean:
	rm -fv ${OBJ} sheriff
