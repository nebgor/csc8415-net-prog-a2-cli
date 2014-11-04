/* Filename: tftp_cli.c
 * This is UDP client program for TFTP application.
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fsm.c"
//#include "defs.h"

void udpnet_open_cli(tftp_t *);

int main(int argc, char *argv[]) {
    int opcode = 0;

    tftpstr="tftp-cli";

    tftp_t tftp = {
	.mode = TFTP_MODE_OCTET,
	.state = TFTP_STATE_OPENED, //this state shouldn't last long in a client (unless its waiting for user cli input!!)
    };

    if (argc < 5) {
        fprintf(stderr, "Usage: %s <ip address> <port> get|put <filename> [target]\n", argv[0]);
        exit(1);
    }
    tftp.host = argv[1];
    tftp.port = argv[2];
    char *cmd=argv[3];

    printf("%d\n", TFUP_OPCODE_RTS);
    printf("%s\n", cmd);

    if (strcmp(cmd, "get") == 0) {
        opcode = TFTP_OPCODE_RRQ;
    } else if (strcmp(cmd, "put") == 0) {
	    opcode = TFTP_OPCODE_WRQ;
    } else if (strcmp(cmd, "rts") == 0) {
	    opcode = TFUP_OPCODE_RTS;
    } else {
    	printf("dunno.bye.\n");
    }
    printf("ok.\n");

    tftp.file = argv[4];
    if (argc == 6) {
        tftp.target = argv[5];
    } else {
        tftp.target = tftp.file; //smart people, simple life!
    }

    if (opcode == TFTP_OPCODE_RRQ || opcode == TFTP_OPCODE_WRQ ||  opcode == TFUP_OPCODE_RTS ||  opcode == TFUP_OPCODE_RLS) {
		int rc = 0;
		int flags;
		int state;

	        // create a UDP socket //
	        udpnet_open_cli(&tftp);

		flags = (opcode == TFTP_OPCODE_RRQ) ? file_write(tftp.target, &tftp.fd) : file_read(tftp.file, &tftp.fd);
		state = (opcode == TFTP_OPCODE_RRQ) ? TFTP_STATE_RRQ_SENT : TFTP_STATE_WRQ_SENT;

		if (tftp.fd == -1) {
		    fprintf(stderr, "%s: failed to open '%s': %s\n",tftpstr, tftp.file, strerror(errno));
		    tftp_close(&tftp);
		    return -1;
		}

		if (tftp_enc_packet(&tftp, opcode, 0, NULL, 0) == -1) {
		    fprintf(stderr, "%s: encoding error\n", tftpstr);
		    tftp_close(&tftp);
		    return -1;
		}
		tftp.state = state;
		tftp.blkno = (opcode == TFTP_OPCODE_RRQ) ? 1 : 0; //sent,expecting this blkno back

		rc = tftp_mainloop(&tftp); //main part of FSM
		tftp_close(&tftp);
		return rc;
    }
}
