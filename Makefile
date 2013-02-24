PREFIX ?= /usr/local

all: bcp

bcp: bcp.c
	gcc -D_FILE_OFFSET_BITS=64 -o bcp bcp.c

install: bcp
	cp -f bcp $(PREFIX)/bin/bcp

uninstall:
	rm -f $(PREFIX)/bin/bcp

clean:
	rm -rf bcp.o bcp

.PHONY: install uninstall
