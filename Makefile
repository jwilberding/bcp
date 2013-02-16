all: bcp

bcp: bcp.c
	gcc -D_FILE_OFFSET_BITS=64 -o bcp bcp.c
clean:
	rm -rf bcp.o bcp
