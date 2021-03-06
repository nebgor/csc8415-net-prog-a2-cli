/* Filename: tftp_cli.c
 * This is UDP client program for TFTP application.
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <utime.h>

#include "fsm.c"
//#include "defs.h"

void udpnet_open_cli(tftp_t *);

int cmd_tftp_rw(tftp_t *tftp, int opcode, char* filename);
int cmd_tfup_rts(tftp_t *tftp, int opcode, char* filename);
long filestats(char* filename, int echostats);

int main(int argc, char *argv[]) {
    int opcode = 0;
    tftp_t tftp = {
		.mode = TFTP_MODE_OCTET,
		.state = TFTP_STATE_OPENED, //this state shouldn't last long in a client (unless its waiting for user cli input!!)
		.opcode = 0
    };

    tftpstr="tftp-cli";

    if (argc < 3) {
        //fprintf(stderr, "Usage: %s <ip address> <port> get|put <filename> [target]\n", argv[0]);
        fprintf(stderr, "Usage: %s <ip address> <port> \n", argv[0]);
        exit(1);
    }

    char cmdstr[TFTP_MAX_MSGSIZE], msgstr[TFTP_MAX_MSGSIZE]={0};
    char *token;
    DIR *dir;
    struct dirent *ent;
    time_t t, current_time, mod_time;
    long long_time_no_see=0, remote_time_seen=0;
    char* delim = " \n";

    // if (strcmp(cmd, "get") == 0) {
    //     opcode = TFTP_OPCODE_RRQ;
    // } else if (strcmp(cmd, "put") == 0) {
	   //  opcode = TFTP_OPCODE_WRQ;
    // } else if (strcmp(cmd, "rts") == 0) {
	   //  opcode = TFUP_OPCODE_RTS;
    // }


    while(1) {
    
	    tftp.host = argv[1];
	    tftp.port = argv[2];

	    // create a UDP socket //
	    udpnet_open_cli(&tftp);

    	// brand new connection.
    	tftp.state = TFTP_STATE_OPENED; //this state shouldn't last long in a client (unless its waiting for user cli input!!)

    	// User input cli - if the tokens are all null now.. (no more left)
    	
    	if(opcode>0) {
    		printf("\n An argument was missing. Please re-enter command in full.\n");
    	}
    	printf("\nType h for help on commands.\n");
    	printf("> ");
    	token = fgets(cmdstr, 50, stdin);
    	if ( token != NULL && token != "\n") {
    		printf("%s\n", token);
	    	//get the first token.
			token = strtok(token, delim);

			while (token != NULL) {
				// printf("token:%s\n", token);
			    if (strcmp(token, "h") == 0) {
			    	printf("\nh\t\t- for this help.\n");
			    	printf("\nllist\t\t - lists file in local current directory.\n");
			    	printf("\nrlist\t\t - list file in remote server's current directory.\n");
			    	printf("\nrupdate <filename>\t\t - requests to check that the local file and remote file specified by argument in <filename> are in sync.\nIf they are not in sync, an operation will be performed so that they become in sync with the latest file.\nThis depends on the fact that both operating systems have time setup correctly. See ntp command for more help in managing a system's time.\n");
			    	printf("\nq\t\t - to close connection and quit.\n");

			    } else if (strcmp(token, "llist") == 0) {

					current_time = time(&t); 
					printf("current time since the EPOCH is %ld ticks on this system.\n", (long)t);
		        	printf("listing local current directory files....\n\n");
		        	printf("[filename]\t\t ts[timestamp]\t\tm[Last file modification in ticks since EPOCH]\n");
					if ((dir = opendir (".")) != NULL) {
					  /* print all the files and directories within directory */
					  while ((ent = readdir (dir)) != NULL) {
					  	if (ent->d_type == DT_REG) {
					  		long_time_no_see = filestats(ent->d_name,0);
					  		mod_time = (time_t)long_time_no_see;
					  		printf ("%s \t\tts:%ld\t\tm:%s", ent->d_name, long_time_no_see, ctime(&mod_time));
					  	}
					    	
					  }
					  closedir (dir);
					} else {
					  /* could not open directory */
					  perror ("Could not open directory.\n");
					}

			    } else if (strcmp(token, "rupdate") == 0) {
			    	opcode = TFUP_OPCODE_RTS;
			    } else if (strcmp(token, "rlist") == 0) {
				    opcode = TFUP_OPCODE_RLS;
				    cmd_tfup_rls(&tftp, opcode);

				    //display file contents and remove file.


				    opcode = 0;

				} else if (opcode > 0) {

			    	if (token == NULL) { //this is really defensive.. won't come here... thanks strtok()!
			    		printf("error. missing filename argument.\n");
			    		printf("resetting all operations from [opcode=%d] to none [filename=%s]. please try again. h for help. q to quit.\n", opcode, token);
			    		opcode=0;
		    		} else {
				    	printf("ok checking filename %s ...\n", token);
				    	//compare timestamps via TFUP RTS...
				    	long_time_no_see = filestats(token,0);
				    	if (long_time_no_see == 0) { //file not found
				    		// do a get if file exists on remote. if that fails just abort operation and say not found anywhere.
				    		printf("The file was not found in the local current directory. Attempting to check on server.\n");
				    	}


			    		// printf("ok sending RTS reques \n");
			    		mod_time = (time_t)long_time_no_see;
				  		printf ("%s \t\tm:%s", token, ctime(&mod_time));

				  		int toklen= strlen(token);
				  		strcpy(msgstr, token);

						msgstr[toklen] = '\0';

				  		// printf("filename:%s\n", msgstr);
			    		if(cmd_tfup_rts(&tftp, opcode, msgstr)) {
			    			printf("error with RTS request. sync aborted. \n");
			    		} else {

				    		remote_time_seen = tftp.timestamp;

				    		if(remote_time_seen == -1 && long_time_no_see == 0) {
						    	printf("\n ==== file does not exist ==== .\n localts:%ld remotets:%ld\n", long_time_no_see, remote_time_seen);
				    		} else {
						    	if (remote_time_seen > long_time_no_see) { // found newer in remote.
						    		printf("remote file is newer. Doing RRQ\n");
								    opcode = TFTP_OPCODE_RRQ; //get remote file.
								    tftp.opcode = opcode;
						    	} else if (remote_time_seen < long_time_no_see && long_time_no_see > 0) { //found newer locally. remote may not have or its older there.
						    		printf("local file is newer. Doing WRQ\n");
								    opcode = TFTP_OPCODE_WRQ; //send to remote the (local) file.
								    tftp.opcode = opcode;
						    	}

						    	if (opcode == TFTP_OPCODE_RRQ || opcode == TFTP_OPCODE_WRQ) {
								    cmd_tftp_rw(&tftp, opcode, msgstr);
								    
								    struct utimbuf localtimes;
									//set file times to be in sync.
								    if (opcode == TFTP_OPCODE_RRQ) {
									    localtimes.modtime = remote_time_seen;
    								    localtimes.actime = (int)NULL; //st.st_atime;
								    	utime(token,&localtimes);
									} else {
										//writing to remote. can't control remote timestamp but can get it.
									    tftp.host = argv[1];
									    tftp.port = argv[2];

									    // create a UDP socket //
									    udpnet_open_cli(&tftp);

								    	// brand new connection.
								    	tftp.state = TFTP_STATE_OPENED; //this state shouldn't last long in a client (unless its waiting for user cli input!!)

										opcode = TFUP_OPCODE_RTS;

	   									printf("updating local with remote timestamp for filename:%s\n", msgstr);
						    			cmd_tfup_rts(&tftp, opcode, msgstr);
				    					localtimes.modtime = tftp.timestamp;
    								   	localtimes.actime = (int)NULL; //st.st_atime;
								    	utime(token,&localtimes);


									}



							    	printf("\n==== file updated ==== \nlocalts:%ld remotets:%ld\n", long_time_no_see, remote_time_seen);
						    	} else {
						    		printf("\n==== file is up to date ==== \nlocalts:%ld remotets:%ld\n", long_time_no_see, remote_time_seen);
						    	}
				    		}



			    		}

				    	//no more operations.
				    	opcode = 0;

		    		}
		    	
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
}

int cmd_tfup_rts(tftp_t *tftp, int opcode, char* filename) {

	int rc = 0;
	int flags;
	int state;

    tftp->file = filename;

	state = TFUP_STATE_RTS_SENT;
	// printf("prepping tfup packet for ts checking\n");
	if (tfup_enc_rts_packet(tftp, opcode) == -1) {
	    printf("%s: encoding error\n", tftpstr);
	    // tftp_close(&tftp);
	    return -1;
	}
	tftp->state = state;
	// printf("rts packet ready.\n");
	rc = tftp_mainloop(tftp); //main part of FSM
	// tftp_close(&tftp);
	return rc;
}

int cmd_tfup_rls(tftp_t *tftp, int opcode) {

	int rc = 0;
	int flags;
	int state;

    // tftp->fd=1; //STDOUT - bad for when closing file descriptors!!! - so just printing.

	state = TFUP_STATE_RLS_SENT;
	// printf("prepping tfup packet for ts checking\n");
	if (tfup_enc_rls_packet(tftp, opcode) == -1) {
	    printf("%s: encoding error\n", tftpstr);
	    // tftp_close(&tftp);
	    return -1;
	}
	tftp->state = state;
	// printf("rts packet ready.\n");
	rc = tftp_mainloop(tftp); //main part of FSM
	// tftp_close(&tftp);
	return rc;
}

int cmd_tftp_rw(tftp_t *tftp, int opcode, char* filename) {

	int rc = 0;
	int flags;
	int state;

    tftp->file = filename;
    // if (argc == 6) {
    //     tftp.target = argv[5];
    // } else {
    tftp->target = tftp->file; //smart people, simple life!
    // }

	flags = (opcode == TFTP_OPCODE_RRQ) ? file_write(tftp->target, &(tftp->fd) ) : file_read(tftp->file, &(tftp->fd) );
	state = (opcode == TFTP_OPCODE_RRQ) ? TFTP_STATE_RRQ_SENT : TFTP_STATE_WRQ_SENT;

	if (tftp->fd == -1) {
	    fprintf(stderr, "%s: failed to open '%s': %s\n",tftpstr, tftp->file, strerror(errno));
	    return -1;
	}

	if (tftp_enc_packet(tftp, opcode, 0, NULL, 0) == -1) {
	    fprintf(stderr, "%s: encoding error\n", tftpstr);
	    return -1;
	}
	tftp->state = state;
	tftp->blkno = (opcode == TFTP_OPCODE_RRQ) ? 1 : 0; //sent,expecting this blkno back

	rc = tftp_mainloop(tftp); //main part of FSM

	// tftp_close(&tftp);
	return rc;

}

long filestats(char* filename, int echostats) {
	struct stat st;
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
	// printf("File last modification time: %ld ticks\n", ctime(&st.st_mtime));
	close(fd);
	/* last modification time */
	return (long)st.st_mtime;
}
