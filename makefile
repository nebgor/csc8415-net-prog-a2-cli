# 
SRC = tftp_cli.c tftp_ser.c files.c udpnet.c fsm.c
OBJ =(*.o.c)

CC = gcc -g -O0

all: fsynccli

fsynccli: fsynccli.o  files.o udpnet.o fsm.o
	$(CC) -o fsynccli fsynccli.o files.o udpnet.o fsm.o

fsynccli.o: fsynccli.c fsm.o defs.h
	$(CC) -c fsynccli.c

#
files.o: files.c
	$(CC) -c files.c

udpnet.o: udpnet.c fsm.o
	$(CC) -c udpnet.c

fsm.o: fsm.c
	$(CC) -c fsm.c

clean:
	rm -f *.o *~ fsynccli 

zip: clean
	pth=`pwd` ; \
	dname=$${pth##*/} ; \
	cd .. ; rm -f $${dname}.zip ; zip -r $${dname}.zip $${dname}
