/* Filename: tftp_ser.c
 * This is UDP server for the TFTP application */

#include <stdlib.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>

#include "fsm.c"

void udpnet_open_ser(tftp_t *tftp);

extern struct sockaddr_in udpServer, udpClient; // relation: udpServer for udpnet_open_ser()

void server_listen(tftp_t *tftp) {
    int rcvcount, pid;

    //build/modify a tftp_t here and pass/fork into tftp_mainloop()
    tftp_t tftp_child;
    tftp_child.mode = TFTP_MODE_OCTET;
    tftp_child.state = TFTP_STATE_OPENED;
            
    fprintf(stdout, "socket1> I am the main server with pid:%d, listening..\n", getpid());
    tftp->addrlen = sizeof((struct sockaddr *) tftp->addr);
    //@todo : perform sd select to keep checking with check_sig_handler() - thisis just server for now so ending simply is OK.

//    buflen = recvfrom(tftp->sd, buf, sizeof(buf), 0, (struct sockaddr *) &tftp->addr, &tftp->addrlen);

    rcvcount = recvfrom(tftp->sd,tftp->msg,sizeof(tftp->msg),0,(struct sockaddr *) tftp->addr, &tftp->addrlen);
    //(gdb) p (struct sockaddr_in) tftp->addr
    if (rcvcount == -1) {
        fprintf(stderr, "%s: recvfrom: %s\n", tftpstr, strerror(errno));
        tftp_close(tftp);
        exit(1);
    }
    fprintf(stdout, "socket1> msg received, pid:%d, forking child to process further..\n", getpid());
    /* for easier debugging of child - uncomment these comments!*/
    pid=fork();
    if(pid==0) { //in child
     /**/
//        tftp_dec_port_change( &tftp_child, tftp);
        struct sockaddr_in *sin;
        char port[6];
        sin = (struct sockaddr_in *) tftp->addr;
        int remote_port = ntohs(sin->sin_port);
        snprintf(port, sizeof(port), "%d", remote_port);
        //point tftp to request's origin socket and addr.
        tftp_child.port = (char*) &port;
        int remote_host = inet_ntoa(sin->sin_addr);
        tftp_child.host = (char *)remote_host;
        
        fprintf(stdout, "socket2> I am the child. with pid:%d, acting on request from %s:%s ..\n",
                getpid(), tftp_child.host, tftp_child.port);//, inet_ntoa(udpServer.sin_addr), ntohs(udpServer.sin_port));
        fprintf(stdout, "socket2> Server has recvd data of len:%d\n", tftp->msglen);
        fprintf(stdout, "socket2> Server recvd msg:%s\n", tftp->msg);
        tftp_child.msglen = sizeof(tftp->msg);
        memcpy(tftp_child.msg, tftp->msg, sizeof(tftp->msg));

        fprintf(stdout, "socket2> Child has copy of recvd data of len:%d\n", tftp_child.msglen);
        //, inet_ntoa(udpServer.sin_addr) , ntohs(udpServer.sin_port)
        udpnet_open_cli(&tftp_child); // convert to char* for func.
        // enter main protocol loop. break 46
        tftp_mainloop(&tftp_child);
        //contains an FSM based loop which exits upon reaching state : TFTP_STATE_CLOSED
        fprintf(stdout, "child process (me) has ended comms with %s:%s, 3 cheers! \\o\\ \\o/ /o/ \n",
                tftp_child.host, tftp_child.port);
        exit(0); // children (always perfect) always make a perfect exit heh.
         /* for easier debugging of child - uncomment these comments!*/
    } else {
        fprintf(stdout, "The spawned child process (not me) has begun, lets see! I'll be listening..\n");
    }
         /**/
}

int main(int argc, char *argv[])
{
    tftp_t tftp;
    tftp.mode = TFTP_MODE_OCTET;
    tftp.port = (char *)TFTP_SVR_PORT;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    tftp.port = argv[1];

    setup_sig_handler();
    /* create a UDP socket */
    udpnet_open_ser(&tftp); //argv[1] is listening port (server initial communications).

    /* Server's 1st infinite loop */
    for(;;) {
        /* FSM  1-listen
         *  - contains sub FSM: 2-forked child protocol: 2.1-send/recv data, 2.2-await  ack or timeout/ send ack , 2.3-die|error.*/
        server_listen(&tftp);
    }

    fprintf(stdout, "ohno,somehow.. we're outside of the fsm loop!!!!");

    return 0;
}