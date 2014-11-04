/*
 * Structure used to keep track of a TFTP protocol session.
 */


#include "defs.h"
// #include "packet.h" // no lets use our own 'struct' and follow the betwork byte order better.

static char* tftpstr="tftpd";
static char filename[128]; //so memory exists in differing scopes.
static char *tftpstates[]={ "TFTP_STATE_OPENED",
                    "TFTP_STATE_CLOSED",
                    "TFTP_STATE_RRQ_SENT",
                    "TFTP_STATE_WRQ_SENT",
                    "TFTP_STATE_DATA_SENT",
                    "TFTP_STATE_LAST_DATA_SENT",
                    "TFTP_STATE_ACK_SENT",
                    "TFTP_STATE_LAST_ACK_SENT",
                    "TFTP_STATE_RRQ_SEND_ACK",
                    "TFTP_STATE_WRQ_SEND_ACK"
    }; //start @ i===-1 needs a +1

static char *tftpopcodes[]={ "TFTP_OPCODE_RRQ",
                    "TFTP_OPCODE_WRQ",
                    "TFTP_OPCODE_DATA",
                    "TFTP_OPCODE_ACK",
                    "TFTP_OPCODE_ERROR"
    }; //start @ i===1 needs a -1

//static sigjmp_buf jmpbuf;
static void showbits(unsigned int x)
{
    int i; 
    for(i=(sizeof(int)*8)-1; i>=0; i--)
        (x&(1<<i))?putchar('1'):putchar('0');

    printf("\n");
}

static void errmsg(char *msg) {
    fprintf(stderr, "%s: %s\n", tftpstr, msg);
}

static void tftp_close(tftp_t *tftp) { //close all pipes
    (void) close(tftp->sd);
    (void) close(tftp->fd);
}

static int sigint=0;
static void sigint_handler(int sig)
{
    write(0, "Ahhh! SIGINT!\n", 14);
    //all process close their own tftp descriptors.
    sigint=1;
}

static void setup_sig_handler() {
    void sigint_handler(int sig); /* portotype defintiion */
    struct sigaction sa;

    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0; //not SA_RESTART, just close and die.
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
}

static void check_sig_handler(tftp_t *tftp) {
    if (sigint == 1) {
        tftp_close(tftp);
        exit(0);
    }
}

// not using interupts, using select() after sendto() instead.

//static void sig_alrm() {
//    siglongjmp(jmpbuf, 1);
//}

//utility functions: uses shifting on datatypes within protocol packet... 
// ? define int as length of protocol packet byte fields on machine..?

//static void tftp_dec_port_change( tftp_t *updatetftp, tftp_t *reftftp) {
//        struct sockaddr_in *sin;
//        char port[6];
//        sin = (struct sockaddr_in *) reftftp->addr;
//        int remote_port = ntohs(sin->sin_port);
//        snprintf(port, sizeof(port), "%d", remote_port);
//        //point tftp to request's origin socket and addr.
//        updatetftp->port = (char*) &port;
//        int remote_host = (int) inet_ntoa(sin->sin_addr);
//        updatetftp->host = (char *)remote_host;
//}


//General tftp packet creation function
static int tftp_enc_packet(tftp_t *tftp, const int opcode, int blkno, unsigned char *data, int datalen) {
    unsigned char *p = tftp->msg; //our raw buffer
    int len;

    *p = (opcode & 0xff); //lower order byte is going in FIRST for the 2014 assignment 2 given model server..
    p++;   
    *p = ((opcode >> 8) & 0xff); //big-endian (network byte order also)
    p++;


    switch (opcode) {

        case TFTP_OPCODE_WRQ:
            //send tftp->target
            tftp->file = tftp->target;
        case TFTP_OPCODE_RRQ:
            //send tftp->file 
    	len = strlen(tftp->file) + 1 + strlen(tftp->mode) + 1; //len without opcode.
    	if (4 + len > TFTP_MAX_MSGSIZE) {
    	    errmsg("encoding error: filename too long");
    	    return -1;
    	}

    	len = strlen(tftp->file);
    	memcpy(p, tftp->file, len + 1);
    	p += len + 1;

    	len = strlen(tftp->mode);
    	memcpy(p, tftp->mode, len + 1);
    	p += len + 1;
    	break;

        case TFTP_OPCODE_DATA:
    	*p = ((blkno >> 8) & 0xff); p++;
    	*p = (blkno & 0xff); p++;

    	if ((4 + datalen) > TFTP_MAX_MSGSIZE) {
    	    errmsg("encoding error: data too big");
    	    return -1;
    	}
    	memcpy(p, data, datalen);
    	p += datalen;
    	break;

        case TFTP_OPCODE_ACK:
    	*p = ((blkno >> 8) & 0xff); p++;
    	*p = (blkno & 0xff); p++;
    	break;

        case TFTP_OPCODE_ERROR:
        default:
    	/* blkno contains an error code and data is a NUL-terminated
    	   string with an error message */
    	*p = ((blkno >> 8) & 0xff); p++;
    	*p = (blkno & 0xff); p++;

    	len = strlen((char *) data);
    	if ((4 + len + 1) > TFTP_MAX_MSGSIZE) {
    	    errmsg("encoding error: error message too big");
    	    return -1;
    	}
    	memcpy(p, data, len + 1);
    	p += len + 1;
    	break;
    }

    tftp->msglen = (p - tftp->msg);
    return tftp->msglen;
}

static int tftp_dec_opcode(unsigned char *buf, int buflen, int *opcode) {
    if (buflen < 2) {
	return 0xffff;
    }

    *opcode = (buf[0] << 8) + buf[1];
    fprintf(stdout,"decoded opcode %d\n",  *opcode);
    return 0;
}

static int tftp_dec_filename_mode(unsigned char *buf, tftp_t* tftp) {
//    (gdb) p (char*)&buf[2]
//    $10 = 0xbeffefae "files.o"
//    (gdb) p (char*)&buf[2+7+1]
//    $11 = 0xbeffefb6 "octet"
//
    tftp->file = filename;
    strcpy(tftp->file,(char *)&buf[2]);

//    memcpy(tftp->file, (char *)&buf[2], sizeof((char *)&buf[2]));
    fprintf(stdout,"decoded filename %s\n",  tftp->file);

    char* mode = (char *)&buf[2+sizeof(tftp->file)+4]; //opcode,str,0,mode,o

//    int i;
//    for(i = 0; mode[i]!='\0'; i++){ //checks for null term
//        mode[i] = tolower(mode[i]);
//    }
//    fprintf(stdout,"decoded mode %s\n",  mode);
//
//    if ( strcmp(mode, TFTP_MODE_OCTET) != 0 ) {
////        return -1; //no mode found or unsupported..
//    }
    return 0;
}

static int tftp_dec_blkno(unsigned char *buf, int buflen, int *blkno) {
    if (buflen < 4) {
	return 0xffff;
    }

    *blkno = (buf[2] << 8) + buf[3];
    fprintf(stdout,"decoded blkno %d\n",  *blkno);
    return 0;
}

static int tftp_dec_data(unsigned char *buf, int buflen, unsigned char **data, int *datalen) {
    if (buflen < 5) {
	*data = NULL;
	*datalen = 0;
	return 0xffff;
    }

    *data = buf+4;
    *datalen = buflen - 4;
    fprintf(stdout,"decoded datalen %d\n",  *datalen);
    return 0;
}

static int tftp_dec_error(unsigned char *buf, int buflen, int *errcode, char **msg) {
    int i;

    if (buflen < 5) {
	*msg = NULL;
	return 0xffff;
    }

    /* sanity check: the error message must be nul-terminated inside
       of the buffer buf, otherwise the packet is invalid */

    for (i = 4; i < buflen; i++) {
	if (buf[i] == 0) break;
    }
    if (i == buflen) {
	errmsg("error message is not a nul-terminated string");
	return -1;
    }

    *errcode = (buf[2] << 8) + buf[3];
    *msg = (char *) buf + 4;
    return 0;
}


//after defining all of that .. now:

/*
 * TFTP protocol FSM, assuming the initial message 
 * (read or write request) has already been encoded into the send buffer.
 */
//  run 127.0.0.1 2222 get files.o
// break 80 @ cli tftp_mainloop

static int tftp_mainloop(tftp_t *tftp)
{
    unsigned char buf[TFTP_MAX_MSGSIZE];
    //packet buf;
    int buflen;
    int opcode, blkno, errcode;
    char *servererrmsg;
    fd_set fdset;
    int retries, returnstatus = 1;
    struct timeval timeout, now;

    retries = TFTP_DEF_RETRIES;
    timerclear(&tftp->timer);
    while (tftp->state != TFTP_STATE_CLOSED) {
        fprintf(stdout, "%s (pid:%d): current tftp protocol state=%s\n", tftpstr, getpid(), tftpstates[(tftp->state)+1]);


        check_sig_handler(tftp);

	if (gettimeofday(&now, NULL) == -1) {
	    fprintf(stderr, "%s: gettimeofday: %s\n",
		    tftpstr, strerror(errno));
	    tftp_close(tftp);
	    return -1;
	}

        //immediately send prepared packet in for non opened states
        if (tftp->state != TFTP_STATE_OPENED) { //the server is quite passive!
            if (! timerisset(&tftp->timer) || timercmp(&now, &tftp->timer, > )) {

                // (gdb) p (struct sockaddr_in ) *tftp->addr
                tftp->addrlen = sizeof(*tftp->addr);
                // eg: first msg = "\000\001files.o\000octet"
                if (-1 == sendto(tftp->sd, tftp->msg, tftp->msglen, 0,(const struct sockaddr *) tftp->addr, tftp->addrlen)) {
                    fprintf(stderr, "%s: sendto: %s\n", tftpstr, strerror(errno));
                    tftp_close(tftp);
                    return -1;
                } else {
                    struct sockaddr_in sin;
                    int addrlen = sizeof(sin);
                    if(getsockname(tftp->sd, (struct sockaddr *)&sin, &addrlen) == 0 &&
                       sin.sin_family == AF_INET &&
                       addrlen == sizeof(sin)) {
                        int local_port = ntohs(sin.sin_port);
                        fprintf(stdout, "msg sent from %s:%d\n", //, to %s:%s",
                                inet_ntoa(sin.sin_addr),
                                local_port
                                );
                    }
                }
            }

            if (tftp->state == TFTP_STATE_LAST_ACK_SENT) {
                tftp->state = TFTP_STATE_CLOSED;
                break;
            }

	    // using fd_set for the select() further below.
            FD_ZERO(&fdset);
            FD_SET(tftp->sd, &fdset);

            if (! timerisset(&tftp->timer)) {
                tftp->backoff.tv_sec = TFTP_DEF_TIMEOUT_SEC;
                tftp->backoff.tv_usec = TFTP_DEF_TIMEOUT_USEC;
                timeout = tftp->backoff;
                timeradd(&now, &tftp->backoff, &tftp->timer);
            } else if (timercmp(&now, &tftp->timer, > )) {
                /* We just retransmitted. Double the interval. */
                timeradd(&tftp->backoff, &tftp->backoff, &tftp->backoff);
                timeout = tftp->backoff;
                timeradd(&now, &tftp->backoff, &tftp->timer);
            } else {
                /* We did not wait long enough yet. Calculate the
                   remaining time to block. */
                timersub(&tftp->timer, &now, &timeout);
            }

            //the wait control.
            if (select((tftp->sd)+1, &fdset, NULL, NULL, &timeout) == -1) {
                fprintf(stderr, "%s: select: %s\n",
                        tftpstr, strerror(errno));
                tftp_close(tftp);
                return -1;
            }

            if (! FD_ISSET(tftp->sd, &fdset)) {
                retries--;
                if (! retries) {
                    fprintf(stderr,
                            "%s: timeout, aborting data transfer\n",
                            tftpstr);
                    tftp_close(tftp);
                    return -1;
                }
                continue;
            }
            tftp->addrlen = sizeof((struct sockaddr *) tftp->addr);
            buflen = recvfrom(tftp->sd, buf, sizeof(buf), 0, (struct sockaddr *) tftp->addr, &tftp->addrlen);
            if (buflen == -1) {
                fprintf(stderr, "%s: recvfrom: %s\n", tftpstr, strerror(errno));
                tftp_close(tftp);
                return -1;
            }
//            fprintf(stdout, "%s: recvfrom: %s\n", tftpstr, tftp->addr);
            if (tftp_dec_opcode(buf, buflen, &opcode) != 0) {
                errmsg("failed to parse opcode in message");
                continue;
            }
        } else {
                fprintf(stdout, "%s (pid:%d): current tftp protocol msg=%s\n", tftpstr, getpid(), (tftp->msg));
            if (tftp->msglen == -1) {
                fprintf(stderr, "%s: recvfrom (via server primary port): %s\n", tftpstr, strerror(errno));
                tftp_close(tftp);
                return -1;
            }
            // eg: msg = "\000\001files.o\000octet"
            memcpy(buf,tftp->msg, tftp->msglen);
            if (tftp_dec_opcode((unsigned char *)buf, sizeof(buf), &opcode) != 0) {
                errmsg("failed to parse opcode in message (via server primary port)");
                //abort!
                exit(1);
            }
        }

        if (tftp->state == TFTP_STATE_OPENED) { //only server comes here with state===opened.
            if(opcode == TFTP_OPCODE_RRQ) { //we need to send a data block blkno 1 as ack.
                    tftp->state = TFTP_STATE_RRQ_SEND_ACK;
            } else if(opcode == TFTP_OPCODE_WRQ){ //@todo: we need to send a blkno 0 as ack
                    tftp->state = TFTP_STATE_WRQ_SEND_ACK;
            } else {
                    //ubnknown/unhandled state transition - @TODO
                    errmsg("unknown opcode in message (server)");
                    exit(1);
            }

        }

        fprintf(stdout, "%s (pid:%d): current on tftp protocol state=%s\n", tftpstr, getpid(), tftpstates[(tftp->state)+1]);
        fprintf(stdout, "%s (pid:%d): got opcode=%s\n", tftpstr, getpid(), tftpopcodes[opcode-1]);

        switch (tftp->state) { //control our opcode based on state.
            case TFTP_STATE_RRQ_SENT: //a client will write and ACK
            case TFTP_STATE_WRQ_SEND_ACK: //a server will write and ACK
            case TFTP_STATE_ACK_SENT: //con't to keep sending ACK
                opcode = TFTP_OPCODE_ACK; 
                fprintf(stdout, "%s (pid:%d): performing opcode=%s\n", tftpstr, getpid(), tftpopcodes[opcode-1]);
                break; //expect to send ACK
            case TFTP_STATE_WRQ_SENT: //a client will read and send DATA
            case TFTP_STATE_RRQ_SEND_ACK: ////a server will read and send DATA
            case TFTP_STATE_DATA_SENT: //con't to keep sending DATA
            case TFTP_STATE_LAST_DATA_SENT:
                opcode = TFTP_OPCODE_DATA;
                fprintf(stdout, "%s (pid:%d): performing opcode=%s\n", tftpstr, getpid(), tftpopcodes[opcode-1]);
                break; //expect to send ACK
            default:
                opcode = TFTP_OPCODE_ERROR; break; //unknown state.. very bad.
        }
        switch (opcode) {
            case TFTP_OPCODE_ACK: //the client or server will do this
                errmsg("starting ACK out\n");
                if (tftp->state != TFTP_STATE_WRQ_SEND_ACK ) { //i'm a client.
                    if (tftp_dec_blkno(buf, buflen, &blkno) != 0) {
                        errmsg("failed to decode block number in data packet");
                        continue;
                    }
                    //switch to new communicate with the server's spawned process's remote port
                    if(blkno<=1) { //hmm..security issue?-> update this port everytime after recvfrom() in this function?
                        //(gdb) p ntohs(((struct sockaddr_in *) &tftp.addr).sin_port)
                        struct sockaddr_in *sin;
                        char port[6];
                        sin = (struct sockaddr_in *) tftp->addr;
                        int remote_port = ntohs(sin->sin_port);
                        snprintf(port, sizeof(port), "%d", remote_port);
                        //point tftp to server's new origin socket (not new addr tho).
                        tftp->port = (char*) &port;
                    }
                } else { //doing ACK (not RRQ/ normal ACK) ; ie: WRQ ack...
                    tftp->blkno=0;
                    blkno=0;
                }
                if (blkno == tftp->blkno) {
                    unsigned char *data;
                    int len, datalen;

                    if (tftp->blkno>0) {
                        if (tftp_dec_data(buf, buflen, &data, &datalen) != 0) {
                            errmsg("failed to decode data/datalen");
                        } else {
                            fprintf(stderr, "%s: got data to write (size:%d)\n", tftpstr, datalen);
                        }
                        len = write(tftp->fd, data, datalen);
                        if (len == -1) {
                            fprintf(stderr, "%s: write: %s\n",
                                    tftpstr, strerror(errno));
                            tftp_close(tftp);
                            return -1;
                        }
                    } else {
                        if (tftp_dec_filename_mode(buf, tftp) != 0) {
                            errmsg("failed to decode filename in RRQ packet or unsupported tftp mode");
                            tftp_close(tftp);
                            continue;
                        }
                        tftp->target=tftp->file;
                        file_write(tftp->file, &tftp->fd);
                        fprintf(stderr, "%s: write : %s\n",tftpstr, strerror(errno));
                    }

                    if (tftp_enc_packet(tftp, TFTP_OPCODE_ACK, tftp->blkno, NULL, 0) == -1) {
                        fprintf(stderr, "%s: encoding error\n", tftpstr);
                        return -1;
                    } else {
                        errmsg("i'm writing the file. ack packet prepped: opcode_ack");
                    }
                    tftp->blkno++;
                    timerclear(&tftp->timer);
                    retries = TFTP_DEF_RETRIES;
                    if (blkno > 0) {
                        tftp->state = (datalen == TFTP_BLOCKSIZE ) ?
                            TFTP_STATE_ACK_SENT : TFTP_STATE_LAST_ACK_SENT;
                    } else {
                        tftp->state = TFTP_STATE_ACK_SENT;
                    }

                }

                break;
            case TFTP_OPCODE_DATA:
                errmsg("starting DATA out\n");
                if (tftp->state != TFTP_STATE_RRQ_SEND_ACK ) {
                    if (tftp_dec_blkno(buf, buflen, &blkno) != 0) {
                        errmsg("failed to decode block numer in ack packet");
                        continue;
                    }
                    if (blkno != tftp->blkno) {
                        errmsg("ignoring unexpected block numer in ack packet"); //RRQ
                        continue;
                    }
                } else {
                    tftp->blkno=0;//first block of an RRQ. we've read opcode so now..
                    if (tftp_dec_filename_mode(buf, tftp) != 0) {
                        errmsg("failed to decode filename in RRQ packet or mode unsupported by server.");
                        tftp_close(tftp);
                        exit(0);
                    }
                    fprintf(stdout,"got filename %s\n",  tftp->file);
                    tftp->target = tftp->file;
                    file_read(tftp->file, &tftp->fd);
                }

                if (tftp->state == TFTP_STATE_LAST_DATA_SENT) {
                    //got ack, good to end.
                    tftp->state = TFTP_STATE_CLOSED;
                } else {
                    ssize_t len;
                    len = read(tftp->fd, buf, TFTP_BLOCKSIZE);
                    if (len == -1) {
                        fprintf(stderr, "%s: read: %s\n", tftpstr, strerror(errno));
                        tftp_close(tftp);
                        return -1;
                    }
                    if (tftp_enc_packet(tftp, TFTP_OPCODE_DATA, ++tftp->blkno, buf, len) == -1) {
                        fprintf(stderr, "%s: encoding error\n", tftpstr);
                        return -1;
                    } else {
                        errmsg("i'm reading the file. data packet prepped: opcode_data");
                    }
                    timerclear(&tftp->timer);
                    retries = TFTP_DEF_RETRIES;
                    tftp->state = (len == TFTP_BLOCKSIZE) ? TFTP_STATE_DATA_SENT : TFTP_STATE_LAST_DATA_SENT;
                }
                break;
            case TFTP_OPCODE_ERROR:
                if (tftp_dec_error(buf, buflen, &errcode, &servererrmsg) != 0) {
                    errmsg("failed to decode error message");
                    continue;
                }
                fprintf(stderr, "%s: tftp error %d: %s\n",
                        tftpstr, errcode, servererrmsg);
                returnstatus = -1;
                tftp->state = TFTP_STATE_CLOSED;
                break;
            default:
                fprintf(stdout, "unexpected message ignored - got opcode %s\n",  tftpopcodes[opcode-1]);
                continue;
        }
        fprintf(stdout, "%s (pid:%d): current block number:%d (prepared), current tftp protocol state=%s\n",
                tftpstr, getpid(), tftp->blkno, tftpstates[(tftp->state)+1]);
    }

    return returnstatus;
}
