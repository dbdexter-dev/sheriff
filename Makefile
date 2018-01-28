export CFLAGS =-pipe -Wall
export LDFLAGS =-lncurses
PREFIX = /usr

.PHONY: default debug release src unittests clean install uninstall

default: debug

debug: CFLAGS += -g -Werror
debug: src unittests

release: CFLAGS += -O2 -march=native -mtune=native
release: src strip

src:
	$(MAKE) -C $@
unittests:
	$(MAKE) -C $@
strip:
	strip src/sheriff

clean:
	$(MAKE) -C src clean
	$(MAKE) -C unittests clean

install: src
	@echo installing executable file to ${PREFIX}/bin
	@mkdir -p ${PREFIX}/bin
	@cp -f src/sheriff ${PREFIX}/bin
	@chmod 755 ${PREFIX}/bin/sheriff

uninstall:
	@echo removing executable file from ${PREFIX}/bin
	@rm -f ${PREFIX}/bin/sheriff
