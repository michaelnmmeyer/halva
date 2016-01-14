PREFIX = /usr/local

CFLAGS = -std=c11 -Wall -Werror -g
CFLAGS += -O2 -s -DNDEBUG -march=native -mtune=native -fomit-frame-pointer
CFLAGS += -flto -fdata-sections -ffunction-sections -Wl,--gc-sections

#--------------------------------------
# Abstract targets
#--------------------------------------

all: halva example

clean:
	rm -f halva example lua/halva.so

check: lua/halva.so
	cd test && valgrind --leak-check=full --error-exitcode=1 lua test.lua

install: halva
	install -spm 0755 $< $(PREFIX)/bin/halva

uninstall:
	rm -f $(PREFIX)/bin/halva

.PHONY: all clean check install uninstall


#--------------------------------------
# Concrete targets
#--------------------------------------

cmd/halva.ih: cmd/halva.txt
	cmd/mkcstring.py < $< > $@

halva: $(wildcard cmd/*) halva.h halva.c
	$(CC) $(CFLAGS) cmd/halva.c cmd/cmd.c halva.c -o $@

example: example.c halva.h halva.c
	$(CC) $(CFLAGS) $< halva.c -o $@

lua/halva.so: halva.h halva.c lua/halva.c
	$(MAKE) -C lua
