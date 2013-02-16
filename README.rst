Broadcast Copy
--------------

Any easy way to copy files between terminals or computers on a local network.


Compiling
---------

make
or
gcc -D_FILE_OFFSET_BITS=64 -o bcp bcp.c


Usage
-----

To make file availabe for copying: ./bcp filename

To receive the file: ./bcp


Example
-------

diginux@heisenberg:~/code/bcp/test$ ./bcp awesome.jpg
Listening for request..
Sending file to: 192.168.2.12:10789
File sent.

diginux@heisenberg:~/code/bcp$ ./bcp
Requesting file..
Incoming connection from: 192.168.2.12
File received: awesome.jpg

