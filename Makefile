export CFLAGS =-pipe -Wall -std=c99 -pedantic -D _GNU_SOURCE -D _XOPEN_SOURCE_EXTENDED
export LDFLAGS =
PREFIX = /usr

.PHONY: default debug release src tests strip clean install uninstall

default: debug

debug: CFLAGS += -g -Werror
debug: src tests

release: CFLAGS += -O2 -march=native -mtune=native
release: src strip

src:
	$(MAKE) -C $@
tests:
	$(MAKE) -C $@
strip:
	strip src/sheriff

clean:
	$(MAKE) -C src clean
	$(MAKE) -C tests clean

install: src
	@echo installing executable file to ${PREFIX}/bin
	@mkdir -p ${PREFIX}/bin
	@cp -f src/sheriff ${PREFIX}/bin
	@chmod 755 ${PREFIX}/bin/sheriff

uninstall:
	@echo removing executable file from ${PREFIX}/bin
	@rm -f ${PREFIX}/bin/sheriff
