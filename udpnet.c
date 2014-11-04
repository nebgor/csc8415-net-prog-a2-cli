#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fsm.c"

//todo: put these into non-global scope.. below .. fornow just static
/* define for the client */
static struct sockaddr_in xferClient, xferServer;

/* define for the server */
struct sockaddr_in udpServer, udpClient;

/* open a UDP socket for the client */
void udpnet_open_cli(tftp_t *tftp) // int *sockd, char *server, char *port)
{
    int  returnStatus;

    /* create a UDP socket */
    tftp->sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (tftp->sd == -1) {
       fprintf(stderr, "Could not access a socket!\n");
       exit(1);
    } else {
       printf("\tSocket created(cli).\n");
    }
 
    /* client address */
    /* use INADDR_ANY to use all local addresses */
    xferClient.sin_family = AF_INET;
    xferClient.sin_addr.s_addr = INADDR_ANY;
    xferClient.sin_port = 0;


    returnStatus = bind(tftp->sd, (struct sockaddr *)&xferClient, sizeof(xferClient));
//    returnStatus = bind(tftp->sd, (struct sockaddr *) &tftp->addr , tftp->addrlen); //don't mix up

    if (returnStatus == 0 ) {
        fprintf(stdout, "\tBind completed.\n");
    } else {
        fprintf(stderr, "Could not bind to address!\n");
        close(tftp->sd);
        tftp->state = TFTP_STATE_CLOSED;
        exit(1);
    }
    /* set up the server information */

    xferServer.sin_family = AF_INET;
    xferServer.sin_addr.s_addr = inet_addr(tftp->host); //not set on server!!
    xferServer.sin_port = htons(atoi(tftp->port));

    // make sure we adjust tftp->host and tftp->port upon any change.
    tftp->addr = (struct sockaddr *) &xferServer;
    tftp->addrlen = sizeof(*tftp->addr);
}



/* open a UDP socket for the server */
void udpnet_open_ser(tftp_t *tftp)
{
   int  returnStatus;

   /* create a UDP socket */
   tftp->sd = socket(AF_INET, SOCK_DGRAM, 0);
   if (tftp->sd == -1) {
       fprintf(stderr, "Could not create socket!\n");
       exit(1);
   } else {
       printf("Socket created (serv port %d).\n", atoi(tftp->port));
   }


   /* set up the server address and port */
   udpServer.sin_family = AF_INET;
   udpServer.sin_addr.s_addr = INADDR_ANY;
   udpServer.sin_port = htons(atoi(tftp->port));
   tftp->addr = (struct sockaddr *) &udpServer;
   tftp->addrlen = sizeof(*tftp->addr);
   /* bind to a socket, use INADDR_ANY for all local addresses */
   returnStatus = bind(tftp->sd, (struct sockaddr *) tftp->addr , tftp->addrlen);
// returnStatus = bind(*sockd, (struct sockaddr *)&udpServer, sizeof(udpServer));


   if (returnStatus == -1) {
      fprintf(stderr, "Could not bind to socket on ! Err: %s\n", strerror(errno));
      tftp->state = TFTP_STATE_CLOSED;
      exit(1);
   }
}

/* open a UDP socket for the server */
//void udpnet_open_ser(*sockd, server, port)
//{
//   int  returnStatus, addrlen;
//
//   /* create a UDP socket */
//   sockd = socket(AF_INET, SOCK_DGRAM, 0);
//   if (sockd == -1) {
//       fprintf(stderr, "Could not create socket!\n");
//       exit(1);
//   } else {
//       printf("Socket created (serv port %d).\n", atoi(port));
//   }
//
//
//   /* set up the server address and port */
//   udpServer.sin_family = AF_INET;
//   udpServer.sin_addr.s_addr = INADDR_ANY;
//   udpServer.sin_port = htons(atoi(port));
////   tftp->addr = (struct sockaddr *) &udpServer;
//   addrlen = sizeof(udpServer);
//   /* bind to a socket, use INADDR_ANY for all local addresses */
//   returnStatus = bind(tsockd, (struct sockaddr *) udpServer,addrlen);
//
//   if (returnStatus == -1) {
//      fprintf(stderr, "Could not bind to socket!\n");
//      tftp->state = TFTP_STATE_CLOSED;
//      exit(1);
//   }
//}
