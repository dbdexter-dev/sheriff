export CFLAGS =-pipe -Wall
export LDFLAGS =-lncurses

.PHONY: default debug release src unittests clean

default: debug

debug: CFLAGS += -g -Werror
debug: src unittests

release: CFLAGS += -O2 -march=native -mtune=native
release: sheriff strip

src:
	$(MAKE) -C $@
unittests:
	$(MAKE) -C $@

clean:
	$(MAKE) -C src clean
	$(MAKE) -C unittests clean

