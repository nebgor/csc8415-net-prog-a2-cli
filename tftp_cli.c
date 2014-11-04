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

int cmd_tftp(tftp_t tftp, int opcode, char* filename);
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

    char cmdstr[50];
    char *token, *filename;
    DIR *dir;
    struct dirent *ent;
    time_t t, current_time, mod_time;
    long long_time_no_see;


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

    	// User input cli
    	printf("\nType h for help on commands.\n");
    	printf("> ");
    	scanf("%s", cmdstr);

		token = strtok(cmdstr, " ");
	    if (strcmp(token, "h") == 0) {
	    	printf("a verbose help message.\n");
	    	continue;

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


        	continue;
	    } else if (strcmp(token, "rupdate") == 0) {
	    	filename = strtok(NULL, " ");
	    	if (filename == NULL) {
	    		printf("error. missing filename argument.\n");
	    		continue;
	    	}
	    	printf("filename:%s\n", filename);
	    	printf("ok checking filename %s ...\n", filename);
	    	//compare timestamps via TFUP RTS...
		    // cmd_tfup(&tftp, opcode, filename)

	    	// update file.
	        opcode = TFTP_OPCODE_RRQ;
		    opcode = TFTP_OPCODE_WRQ;

		    // cmd_tftp(&tftp, opcode, filename)
		    continue;
	    } else if (strcmp(cmdstr, "rts") == 0) {
		    opcode = TFUP_OPCODE_RTS;
		    continue;
	    } else if (strcmp(token, "q") == 0) {
	    	printf("ok, quiting...\n");
	    	tftp_close(&tftp);
	    	printf("connection closed. bye.\n");
	    	exit(0);
	    } else {
	    	printf("unknown input, try again. h for help. q to quit.\n");
	    	continue;
	    }
    }
}

int cmd_tftp(tftp_t tftp, int opcode, char* filename) {

    	// upon rupdate 'f' do the necessary RRQ or WRQ (based on timestamp comparison)

	    if (opcode == TFTP_OPCODE_RRQ || opcode == TFTP_OPCODE_WRQ) {
			int rc = 0;
			int flags;
			int state;

		        // // create a UDP socket //
		        // udpnet_open_cli(&tftp);
		    // tftp.file = argv[4];
		    // if (argc == 6) {
		    //     tftp.target = argv[5];
		    // } else {
		    //     tftp.target = tftp.file; //smart people, simple life!
		    // }

			flags = (opcode == TFTP_OPCODE_RRQ) ? file_write(tftp.target, &tftp.fd) : file_read(tftp.file, &tftp.fd);
			state = (opcode == TFTP_OPCODE_RRQ) ? TFTP_STATE_RRQ_SENT : TFTP_STATE_WRQ_SENT;

			if (tftp.fd == -1) {
			    fprintf(stderr, "%s: failed to open '%s': %s\n",tftpstr, tftp.file, strerror(errno));
			    // tftp_close(&tftp);
			    return -1;
			}

			if (tftp_enc_packet(&tftp, opcode, 0, NULL, 0) == -1) {
			    fprintf(stderr, "%s: encoding error\n", tftpstr);
			    // tftp_close(&tftp);
			    return -1;
			}
			tftp.state = state;
			tftp.blkno = (opcode == TFTP_OPCODE_RRQ) ? 1 : 0; //sent,expecting this blkno back

			rc = tftp_mainloop(&tftp); //main part of FSM
			// tftp_close(&tftp);
			return rc;
	    }
}

long filestats(char* filename, int echostats) {
	struct stat st;
	int fd = open(filename, O_RDONLY);
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
