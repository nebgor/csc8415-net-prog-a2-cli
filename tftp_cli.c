/* Filename: tftp_cli.c
 * This is UDP client program for TFTP application.
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
 #include <dirent.h>

#include "fsm.c"
//#include "defs.h"

void udpnet_open_cli(tftp_t *);

int cmd_tftp_rw(tftp_t tftp, int opcode, char* filename);
long cmd_tfup_rts(tftp_t tftp, int opcode, char* filename);
long filestats(char* filename, int echostats);

int main(int argc, char *argv[]) {
    int opcode = 0;
    tftp_t tftp = {
		.mode = TFTP_MODE_OCTET,
		.state = TFTP_STATE_OPENED, //this state shouldn't last long in a client (unless its waiting for user cli input!!)
    };

    tftpstr="tftp-cli";

    if (argc < 3) {
        //fprintf(stderr, "Usage: %s <ip address> <port> get|put <filename> [target]\n", argv[0]);
        fprintf(stderr, "Usage: %s <ip address> <port> \n", argv[0]);
        exit(1);
    }

    char cmdstr[50] = "This is - www.tutorialspoint.com - website";
    char *token;
    DIR *dir;
    struct dirent *ent;
    time_t t, current_time, mod_time;
    long long_time_no_see=0, remote_time_seen=0;
    char delim[] = " \n";

    // if (strcmp(cmd, "get") == 0) {
    //     opcode = TFTP_OPCODE_RRQ;
    // } else if (strcmp(cmd, "put") == 0) {
	   //  opcode = TFTP_OPCODE_WRQ;
    // } else if (strcmp(cmd, "rts") == 0) {
	   //  opcode = TFUP_OPCODE_RTS;
    // }
    
    tftp.host = argv[1];
    tftp.port = argv[2];

    // create a UDP socket //
    udpnet_open_cli(&tftp);

    while(1) {
    	// brand new connection.
    	tftp.state = TFTP_STATE_OPENED; //this state shouldn't last long in a client (unless its waiting for user cli input!!)
    	// User input cli - if the tokens are all null now.. (no more left)

    	printf("\nType h for help on commands.\n");
    	printf("> ");
    	fgets(cmdstr, sizeof(cmdstr), stdin);

    	//get the first token.
		token = strtok(cmdstr, delim);

		while (token != NULL) {
			// printf("token:%s", token);
		    if (strcmp(token, "h") == 0) {
		    	printf("a verbose help message.\n");

		    } else if (strcmp(token, "llist") == 0) {

				current_time = time(&t); 
				printf("current time since the EPOCH is %ld ticks\n", (long)t);
	        	printf("listing local current directory files....\n\n");
	        	printf("[filename]\t\tm[Last file modification in ticks since EPOCH]\n");
				if ((dir = opendir (".")) != NULL) {
				  /* print all the files and directories within directory */
				  while ((ent = readdir (dir)) != NULL) {
				  	if (ent->d_type == DT_REG) {
				  		long_time_no_see = filestats(ent->d_name,0);
				  		mod_time = (time_t)long_time_no_see;
				  		printf ("%s \t\tm:%s", ent->d_name, ctime(&mod_time));
				  	}
				    	
				  }
				  closedir (dir);
				} else {
				  /* could not open directory */
				  perror ("Could not open directory.\n");
				}

		    } else if (strcmp(token, "rupdate") == 0) {
		    	opcode = TFUP_OPCODE_RTS;
		    } else if (strcmp(token, "rts") == 0) {
			    opcode = TFUP_OPCODE_RTS;
			} else if (opcode > 0) {

		    	if (token == NULL) { //this is really defensive.. won't come here... thanks strtok()!
		    		printf("error. missing filename argument.\n");
		    		printf("resetting all operations from [opcode=%d] to none [filename=%s]. please try again. h for help. q to quit.\n", opcode, token);
		    		opcode=0;
		    		continue;
	    		}

		    	printf("ok checking filename %s ...\n", token);
		    	//compare timestamps via TFUP RTS...
		    	long_time_no_see = filestats(ent->d_name,0);


	    		printf("ok sending");
	    		mod_time = (time_t)long_time_no_see;
		  		printf ("%s \t\tm:%s", ent->d_name, ctime(&mod_time));
	    		remote_time_seen = cmd_tfup_rts(tftp, opcode, ent->d_name);


		    	if (0) {

		    	}
		    	// update file.
		        opcode = TFTP_OPCODE_RRQ;
			    opcode = TFTP_OPCODE_WRQ;

			    // cmd_tftp(&tftp, opcode, token)	    		

		    	printf("file updated. localts:%ld remotets:%ld\n", long_time_no_see, remote_time_seen);


		    	opcode = 0;

		    } else if (strcmp(token, "q") == 0) {
		    	printf("ok, quiting...\n");
		    	tftp_close(&tftp);
		    	printf("connection closed. bye.\n");
		    	exit(0);
		    } else {
	    		printf("unknown input %s. [opcode=%d], try again. h for help. q to quit.\n", token, opcode);
		    }

		    token = strtok(NULL, delim);
		}

    }
}

long cmd_tfup_rts(tftp_t tftp, int opcode, char* filename) {

	int rc = 0;
	int flags;
	int state;

    tftp.file = filename;

	state = TFUP_STATE_RTS_SENT;
	printf("prepping tfup packet for ts checking\n");
	if (tfup_enc_rts_packet(&tftp, opcode) == -1) {
	    fprintf(stderr, "%s: encoding error\n", tftpstr);
	    // tftp_close(&tftp);
	    return -1;
	}
	tftp.state = state;
	printf("rts packet ready.\n");

	            // case TFUP_STATE_RTS_SENT: //TFUP state for RTS
             //    opcode = TFUP_OPCODE_TIME; 
             //    fprintf(stdout, "%s (pid:%d): performing receipt of opcode=%s\n", tftpstr, getpid(), tftpopcodes[opcode-1]);
             //    tftp->timestamp = (buf[2] << 8) + buf[3];
             //    fprintf(stdout,"decoded timestamp %s\n",  tftp->timestamp);
             //    tftp->state = TFTP_STATE_CLOSED;
             //    return 1;
             //    break; //don't expect to send ACK

	rc = tftp_mainloop(&tftp); //main part of FSM
	// tftp_close(&tftp);
	return tftp.timestamp;
}

int cmd_tftp_rw(tftp_t tftp, int opcode, char* filename) {

	int rc = 0;
	int flags;
	int state;

    tftp.file = filename;
    // if (argc == 6) {
    //     tftp.target = argv[5];
    // } else {
    tftp.target = tftp.file; //smart people, simple life!
    // }

	flags = (opcode == TFTP_OPCODE_RRQ) ? file_write(tftp.target, &tftp.fd) : file_read(tftp.file, &tftp.fd);
	state = (opcode == TFTP_OPCODE_RRQ) ? TFTP_STATE_RRQ_SENT : TFTP_STATE_WRQ_SENT;

	if (tftp.fd == -1) {
	    fprintf(stderr, "%s: failed to open '%s': %s\n",tftpstr, tftp.file, strerror(errno));
	    return -1;
	}

	if (tftp_enc_packet(&tftp, opcode, 0, NULL, 0) == -1) {
	    fprintf(stderr, "%s: encoding error\n", tftpstr);
	    return -1;
	}
	tftp.state = state;
	tftp.blkno = (opcode == TFTP_OPCODE_RRQ) ? 1 : 0; //sent,expecting this blkno back

	rc = tftp_mainloop(&tftp); //main part of FSM
	// tftp_close(&tftp);
	return rc;

}

long filestats(char* filename, int echostats) {
	struct stat st;
	printf("filestats\n");
	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
	    fprintf(stderr, "%s: failed to stat '%s': %s\n",tftpstr, filename, strerror(errno));
	    return 0;
	}

	 /* Get info. about the file */
	fstat(fd, &st);
	// *length = st.st_size;
	if(echostats) {
		printf ("%s c:%s a:%s m:%s ", filename, ctime(&st.st_ctime), ctime(&st.st_atime), ctime(&st.st_mtime));
	}
	// printf("File last modification time: %ld ticks\n", mod_time);
	close(fd);
	/* last modification time */
	return (long)st.st_mtime;
}
