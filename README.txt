This folder contains 2 programs that implements the TFTP protocol using two FSMs defined in fsm.c. They are coded together in the function tftp_mainloop().
There are also 2 folders to test from within.

the tftp client is "tftp_cli". run it to see usage help.

The tftp server is "tftp_ser". It is a multi process server.

The programs implement octet communication between them.
Values for retry, timer timeout can be modified in defs.h (and recompiled)

A make file exists that compiles and builds files.c, fsm.c and udpnet.c altogether during compilation where needed. 

common building commands are:

server:
	pi@raspberrypi1 ~/csc8415-net-prog/a1source/dataserver $ cd ..;make clean; make; cd -; ../tftp_ser 2222

server (debugging):
	pi@raspberrypi1 ~/csc8415-net-prog/a1source/dataserver $ cd ..;make clean; make; cd -; gdb --args ../tftp_ser 2222

ctrl-C gracefully kills server. (closes descriptors.)



client commands (tests):
	pi@raspberrypi1 ~/csc8415-net-prog/a1source/dataclifiles $ cd ..; make; cd -; ../tftp_cli 127.0.0.1 2222 put test3.txt

	pi@raspberrypi1 ~/csc8415-net-prog/a1source/dataclifiles $ cd ..; make; cd -; ../tftp_cli 127.0.0.1 2222 get test3.txt

	pi@raspberrypi1 ~/csc8415-net-prog/a1source/dataclifiles $ cd ..; make; cd -; ../tftp_cli 127.0.0.1 2222 put test3.txt test3put.txt

	pi@raspberrypi1 ~/csc8415-net-prog/a1source/dataclifiles $ cd ..; make; cd -; ../tftp_cli 127.0.0.1 2222 get test3.txt test3put.txt


note , to save saome time, you only need to run make once and by default all project files willl be compiled.

client (debugging): 
	pi@raspberrypi1 ~/csc8415-net-prog/a1source/dataclifiles $ cd ..; make; cd -; gdb --args ../tftp_cli 127.0.0.1 2222 put test3.txt

	pi@raspberrypi1 ~/csc8415-net-prog/a1source/dataclifiles $ cd ..; make; cd -; gdb --args  ../tftp_cli 127.0.0.1 2222 get test3.txt

	pi@raspberrypi1 ~/csc8415-net-prog/a1source/dataclifiles $ cd ..; make; cd -; gdb --args  ../tftp_cli 127.0.0.1 2222 put test3.txt test3target.txt

	pi@raspberrypi1 ~/csc8415-net-prog/a1source/dataclifiles $ cd ..; make; cd -; gdb --args  ../tftp_cli 127.0.0.1 2222 get test3.txt test3target.txt


notes:
	gdb common useful commands:

	break <numberline> or <functionname>
	list
	where
	run

run 'make clean' for a quick clean up. see 'makefile' for building details. 


	


