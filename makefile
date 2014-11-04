# 
SRC = tftp_cli.c tftp_ser.c files.c udpnet.c fsm.c
OBJ =(*.o.c)

CC = gcc -g -O0

all: tftp_cli tftp_ser

tftp_ser: tftp_ser.o files.o udpnet.o fsm.o
	$(CC) -o tftp_ser tftp_ser.o files.o udpnet.o fsm.o

tftp_cli: tftp_cli.o  files.o udpnet.o fsm.o
	$(CC) -o tftp_cli tftp_cli.o files.o udpnet.o fsm.o

#
tftp_ser.o: tftp_ser.c fsm.o defs.h
	$(CC) -c tftp_ser.c

tftp_cli.o: tftp_cli.c fsm.o defs.h
	$(CC) -c tftp_cli.c

#
files.o: files.c
	$(CC) -c files.c

udpnet.o: udpnet.c fsm.o
	$(CC) -c udpnet.c

fsm.o: fsm.c
	$(CC) -c fsm.c

clean:
	rm -f *.o *~ tftp_ser tftp_cli 

zip: clean
	pth=`pwd` ; \
	dname=$${pth##*/} ; \
	cd .. ; rm -f $${dname}.zip ; zip -r $${dname}.zip $${dname}
