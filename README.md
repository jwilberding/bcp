# Broadcast Copy

Any easy way to copy files between terminals or computers on a local network.

Note: bcp currently assumes a trusted network, use with caution

# Compiling

make
or
gcc -D_FILE_OFFSET_BITS=64 -o bcp bcp.c

# Installation

```
$ make install
```

 One line install:
```
$ bash -c "mkdir /tmp/bcp && cd /tmp/bcp && curl -L# https://github.com/jgallen23/bcp/archive/master.tar.gz | tar zx --strip 1 && make install"
```

# Usage

To make file availabe for copying:

	$ ./bcp filename

To receive the file:

	$ ./bcp


# Example

Sender

	diginux@heisenberg:~/code/bcp/test$ ./bcp awesome.jpg
	Listening for request..
	Sending file to: 192.168.2.12:10789
	Sent 107545
	File sent.

Recipient

	Jordans-MacBook-Pro:bcp diginux$ ./bcp
	Requesting file..
	Incoming connection from: 192.168.2.12
	Receive: 107545
	File received: awesome.jpg

# Useful bash commands

bcpdir: to send directories

	# zip directory to /tmp and bcp it
	bcpdir() {

		curr_time=`date +%s`
		file=/tmp/files_$curr_time.zip

		if [[ -d $1 ]]; then

			# dir	
			cd $1
			zip -r -9 $2 $file .

		elif [[ -f $1 ]]; then
	
			# single file
			file_dir=`dirname $1`
			file_name=`basename $1`
			cd $file_dir
			zip -r -9 $2 $file $file_name

		else
			echo "$1 is not valid!"
			exit 1	
		fi

		bcp $file
		rm $file
		cd -
	}

bcppass: to send files/directories protected with password

	# zip file/directory with password to /tmp and bcp it
	bcppass() {
		bcpdir $1 -e
	}

# Alternatives:

* A great (more robust) program that uses polling: http://www.fefe.de/ncp/

* Another great approach, requires knowing name of sent file: https://www.udpcast.linux.lu/cmdlinedoc.html

* Quick file distribution challenge: http://www.advogato.org/article/555.html

