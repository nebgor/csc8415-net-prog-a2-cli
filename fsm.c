/*
 * Structure used to keep track of a TFTP protocol session.
 */


#include "defs.h"
// #include "packet.h" // no lets use our own 'struct' and follow the betwork byte order better.

#define FSM_DEBUG   0
 #define FSM_DEBUG_EXTRA   0

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
                    "TFTP_STATE_WRQ_SEND_ACK",
                    "TFUP_STATE_RTS_SENT"
    }; //start @ i===-1 needs a +1

static char *tftpopcodes[]={ "TFTP_OPCODE_RRQ",
                    "TFTP_OPCODE_WRQ",
                    "TFTP_OPCODE_DATA",
                    "TFTP_OPCODE_ACK",
                    "TFTP_OPCODE_ERROR",
                    "TFUP_OPCODE_RTS",
                    "TFUP_OPCODE_TIME",
                    "TFUP_OPCODE_RLS"
    }; //start @ i===1 needs a -1

//static sigjmp_buf jmpbuf;
static void showbits(unsigned int x)
{
    int i; 
    for(i=(sizeof(int)*8)-1; i>=0; i--)
        (x&(1<<i))?putchar('1'):putchar('0');

    printf("\n");
}


static void jacobson_rto(struct timeval *mrtt, struct timeval *srtt, struct timeval *drtt, struct timeval *vrtt, struct timeval *rto) {
// struct timeval timeout, transmissiontime, mrtt, srtt, drtt; //rto=timeout
/***
jacobson's algo for Round Trip Timeout (RTO):
    simplistic variance.
    delta = measuredRTT - smoothedRTT;
    smoothedRTT = smoothedRTT + factor * delta;

    varianceRTT = variance RTT + h * (abs(delta) - varianceRTT );
    RTO = 1*smoothedRTT + 4 * varianceRTT;

*******/
    int i=0;

    rto->tv_sec = 0;
    rto->tv_usec = 0;

    drtt->tv_sec = 0;
    drtt->tv_usec = 0;

    if(FSM_DEBUG ) printf("mrtt: %d:%d\t", mrtt->tv_sec, mrtt->tv_usec);
    if(FSM_DEBUG) printf("srtt: %d:%d\t", srtt->tv_sec, srtt->tv_usec);
    timersub(mrtt, srtt, drtt);
    if(FSM_DEBUG) printf("drtt: %d:%d\t", drtt->tv_sec, drtt->tv_usec);
    drtt->tv_sec *= 0.125; //delta factor by phi=1/8
    drtt->tv_usec *= 0.125;//delta factor by phi=1/8
    timeradd(srtt, drtt, srtt);
    if(FSM_DEBUG) printf("srtt: %d:%d\n", srtt->tv_sec, srtt->tv_usec);



    drtt->tv_sec = abs(drtt->tv_sec);
    drtt->tv_usec = abs(drtt->tv_usec);
    if(FSM_DEBUG) printf("drtt: %d:%d\t", drtt->tv_sec, drtt->tv_usec);
    if(FSM_DEBUG) printf("vrtt: %d:%d\t", vrtt->tv_sec, vrtt->tv_usec);
    timersub(drtt, vrtt, drtt);
    if(FSM_DEBUG) printf("'drtt': %d:%d\t", drtt->tv_sec, drtt->tv_usec);
    drtt->tv_sec *= 0.25; //h of 1/4
    drtt->tv_usec *=0.25; //h of 1/4
    if(FSM_DEBUG) printf("'drtt': %d:%d\t", drtt->tv_sec, drtt->tv_usec);
    timeradd(drtt, vrtt, vrtt);
    if(FSM_DEBUG) printf("vrtt: %d:%d\n", vrtt->tv_sec, vrtt->tv_usec);

 

    //multiply by 4. 2 doublings.
    for(i=0; i<2; i++) timeradd(vrtt, rto, rto);
    if(FSM_DEBUG) printf("vrtt: %d:%d\t", vrtt->tv_sec, vrtt->tv_usec);

    if(FSM_DEBUG) printf("srtt: %d:%d\n", srtt->tv_sec, srtt->tv_usec);


    timeradd(rto, srtt, rto);//srtt factor of 1

    if(FSM_DEBUG) printf("rto: %d:%d\n", rto->tv_sec, rto->tv_usec);

}

static void errmsg(char *msg) {
    if(FSM_DEBUG_EXTRA) fprintf(stderr, "%s: %s\n", tftpstr, msg);
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

// General TFUP functions


// request for timestamp [just opcode and filename]
static int tfup_enc_rts_packet(tftp_t *tftp, const int opcode) {
   if(FSM_DEBUG) printf("encoding RTS, opcode:%d\n", opcode);

    unsigned char *p = tftp->msg; //our raw buffer
    int len;
    // showbits(*p);
    // *p = opcode; //basically replicating this below in bitwise operationss.
    *p = (opcode & 0xff); //lower order byte is going in FIRST for the 2014 assignment 2 given model server..
        // showbits(*p);
    p++;  
    *p = ((opcode >> 8) & 0xff); // so NOT big-endian (network byte order also) but its what the model server expects. 'standards'
        // showbits(*p);
    p++;
    // showbits(tftp->msg);
    //send tftp->file
    len = strlen(tftp->file)+1; //copy the null.
    printf("len(with null):%d\n", len);

    if (len+2 > TFTP_MAX_MSGSIZE) {
        errmsg("encoding error: filename too long");
        return -1;
    }

    memcpy(p, tftp->file, len);
    // p += len + 1;

    tftp->msglen=len+2;
    printf("msglen:%d\n", tftp->msglen);
    tftp->opcode=opcode; //for detecting if tfup operation
    return tftp->msglen;
}

// request for timestamp [just opcode and filename]
static int tfup_enc_rls_packet(tftp_t *tftp, const int opcode) {
   if(FSM_DEBUG) printf("encoding RLS, opcode:%d\n", opcode);

    unsigned char *p = tftp->msg; //our raw buffer
    int len;
    // showbits(*p);
    // *p = opcode; //basically replicating this below in bitwise operationss.
    *p = (opcode & 0xff); //lower order byte is going in FIRST for the 2014 assignment 2 given model server..
     if(FSM_DEBUG) showbits(*p);
    p++;  
    *p = ((opcode >> 8) & 0xff); // so NOT big-endian (network byte order also) but its what the model server expects. 'standards'
        // showbits(*p);
    p++;
    // showbits(tftp->msg);
    //send tftp->file
    len = 2 ; //len

    tftp->msglen=len;
    tftp->opcode=opcode; //for detecting if tfup operation
    return tftp->msglen;
}

static int tfup_dec_opcode(unsigned char *buf, int buflen, int *opcode) {
    if (buflen < 2) {
    return 0xffff;
    }

    *opcode = (buf[1] << 8) + buf[0];
    if(FSM_DEBUG) printf("decoded tfup opcode %d\n",  *opcode);
    return 0;
}

static int tfup_dec_timestamp(unsigned char *buf, int buflen, tftp_t *tftp) {
    long ts;
    // bcopy(buf[2],ts,4);
    memcpy(&ts, buf+2, buflen-2);

    tftp->timestamp = ntohl(ts);

    // tftp->timestamp = ntohl(tftp->timestamp);
    // *timestamp = (buf[0] << 8) + buf[0];
    if(FSM_DEBUG) printf("decoded tfup timestamp %ld\n",  tftp->timestamp);
    return 0;
}

//General tftp packet creation function
static int tftp_enc_packet(tftp_t *tftp, const int opcode, int blkno, unsigned char *data, int datalen) {
    unsigned char *p = tftp->msg; //our raw buffer
    int len;

    *p = (opcode & 0xff); //lower order byte is going in FIRST for the 2014 assignment 2 given model server..
    p++;   
    *p = ((opcode >> 8) & 0xff); // so NOT big-endian (network byte order also) but its what the model server expects. 'standards'
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
    	// *p = ((blkno >> 8) & 0xff); p++;
    	// *p = (blkno & 0xff); p++;

    	if ((2 + datalen) > TFTP_MAX_MSGSIZE) {
    	    errmsg("encoding error: data too big");
    	    return -1;
    	} else {
           if(FSM_DEBUG) printf("loaded %d bytes onto data packet.\n", datalen);
        }
    	memcpy(p, data, datalen);
    	p += datalen;
    	break;

        case TFTP_OPCODE_ACK:
        // no blkno in tfup. .. we really want a good ack or timeout instead!
    	// *p = ((blkno >> 8) & 0xff); p++;
    	// *p = (blkno & 0xff); p++;
    	break;

        case TFTP_OPCODE_ERROR:
        default:
    	/* blkno contains an error code and data is a NUL-terminated
    	   string with an error message */
        //no blkno in tfup
    	// *p = ((blkno >> 8) & 0xff); p++;
    	// *p = (blkno & 0xff); p++;

    	len = strlen((char *) data);
    	if ((2 + len + 1) > TFTP_MAX_MSGSIZE) {
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

    // so NOT big-endian (network byte order also) but its what the model server expects. 'standards'
    *opcode = (buf[1] << 8) + buf[0]; //opcode is not like in standard tftp protocol now. opcode it little endian.
    if(FSM_DEBUG) fprintf(stdout,"decoded tftp opcode %d\n",  *opcode);
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
    if(FSM_DEBUG) fprintf(stdout,"decoded filename %s\n",  tftp->file);

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


static int tfup_dec_rls_data(unsigned char *buf, int buflen, unsigned char **data, int *datalen) {
    if (buflen < 3) {
        *data = NULL;
        *datalen = 0;
        return 0;
    }

    *data = buf+2; // no blkno this time in tfup...
    *datalen = buflen-2; //no blkno, just opcode to offset.
    *(data+buflen-2) = '\0';
    if(FSM_DEBUG) fprintf(stdout,"decoded datalen %d\n",  *datalen);
    return 0;
}

static int tftp_dec_data(unsigned char *buf, int buflen, unsigned char **data, int *datalen) {
    if (buflen < 3) {
    	*data = NULL;
    	*datalen = 0;
    	return 0;
    }

    *data = buf+2; // no blkno this time in tfup...
    *datalen = buflen - 2; //no blkno, just opcode to offset.
    if(FSM_DEBUG) fprintf(stdout,"decoded datalen %d\n",  *datalen);
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
    struct timeval rto={0}, timeout={0}, now={0}, transmissiontime={0}, originaltrans={0}, mrtt={0}, srtt={0}, drtt={0}, vrtt={0}; //rto=timeout

    retries = TFTP_DEF_RETRIES;
    timerclear(&tftp->timer);
    timerclear(&tftp->backoff);
    while (tftp->state != TFTP_STATE_CLOSED) {
        if(FSM_DEBUG) fprintf(stdout, "%s (pid:%d): current tftp protocol state=%s\n", tftpstr, getpid(), tftpstates[(tftp->state)+1]);

        check_sig_handler(tftp);

    	if (gettimeofday(&transmissiontime, NULL) == -1) {
    	    fprintf(stderr, "%s: gettimeofday: %s\n",
    		    tftpstr, strerror(errno));
    	    // tftp_close(tftp);
    	    return -1;
    	}

        //immediately send prepared packet in for non opened states
        if (tftp->state != TFTP_STATE_OPENED) { //the server is quite passive!
            if (timercmp(&transmissiontime, &tftp->timer, > )) { // allow retransmission ie: when timerisset(&tftp->timer)

                // (gdb) p (struct sockaddr_in ) *tftp->addr
                tftp->addrlen = sizeof(*tftp->addr);
                // eg: first msg = "\000\001files.o\000octet"

                if (-1 == sendto(tftp->sd, tftp->msg, tftp->msglen, 0,(const struct sockaddr *) tftp->addr, tftp->addrlen)) {
                    fprintf(stderr, "%s: sendto: %s\n", tftpstr, strerror(errno));
                    // tftp_close(tftp);
                    return -1;
                } else {
                    struct sockaddr_in sin;
                    int addrlen = sizeof(sin);
                    if(getsockname(tftp->sd, (struct sockaddr *)&sin, &addrlen) == 0 &&
                       sin.sin_family == AF_INET &&
                       addrlen == sizeof(sin)) {
                        int local_port = ntohs(sin.sin_port);
                        // fprintf(stdout, "msg sent from %s:%d\n", //, to %s:%s",
                        //         inet_ntoa(sin.sin_addr),
                        //         local_port
                        //         );
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

            if ( !timerisset(&tftp->timer) || tftp->backoff.tv_usec == 0) { //first timer setup when there isn't or no backoff(had a success)

                if ( tftp->backoff.tv_usec == 0) { //do this for every new round of packet attempts...
                    tftp->backoff.tv_sec = TFTP_DEF_TIMEOUT_SEC;
                    tftp->backoff.tv_usec = TFTP_DEF_TIMEOUT_USEC;
                } else {
                    if(FSM_DEBUG) printf("initial timer is setup.\n");
                    rto.tv_sec=TFTP_DEF_TIMEOUT_SEC;
                    rto.tv_usec=TFTP_DEF_TIMEOUT_USEC;
                }
                timerclear(&timeout);
                timeradd(&rto, &timeout, &timeout);

                if(FSM_DEBUG) printf("using timeout duration: %d:%d\n", timeout.tv_sec, timeout.tv_usec);
                timeradd(&transmissiontime, &timeout, &tftp->timer);

                //save first packet transmission TRY time.
                originaltrans.tv_sec = transmissiontime.tv_sec;
                originaltrans.tv_usec = transmissiontime.tv_usec;


            } else if (timercmp(&transmissiontime, &tftp->timer, > )) {
                /* We just retransmitted. Double the backoff interval. */
                timeradd(&tftp->backoff, &tftp->backoff, &tftp->backoff); //dbl backoff

                timeradd(&rto, &tftp->backoff, &timeout); //add backoff and RTO.

               if(FSM_DEBUG) printf("(re)transmitting, timeradded with double backoff : %d:%d\n", timeout.tv_sec, timeout.tv_usec);

            } else {
                /*select got back with 'I/O ready' but no data (not important data?)...
                 transmissiontime (now) is not great than timer.. so not retransmitted yet.
                Calculate the remaining time to block. to continue waiting for the remaining time.*/

               if(FSM_DEBUG) printf("timeout from select() (TIMEOUT) (LINUX:amount of time not slept - indicative only) : %d:%d\n", timeout.tv_sec, timeout.tv_usec);
               if(FSM_DEBUG) printf("timeout from select() (TIMEOUT) backoff in use : %d:%d\n", tftp->backoff.tv_sec, tftp->backoff.tv_usec);
               if(FSM_DEBUG) printf("timeout from select() (TIMEOUT) RTO in use : %d:%d\n", rto.tv_sec, rto.tv_usec);
                timersub(&tftp->timer, &transmissiontime, &timeout); //timeout is/maybe adjusted by select when it returns upon success.
               if(FSM_DEBUG) printf("timeout left since last transmission. will waiting again for: %d:%d\n", timeout.tv_sec, timeout.tv_usec);
            }



            //the wait control.
            if(FSM_DEBUG||1) printf("timeout: %d:%d\t", timeout.tv_sec, timeout.tv_usec);
            if (select((tftp->sd)+1, &fdset, NULL, NULL, &timeout) == -1) {
                fprintf(stderr, "%s: select: %s\n",
                        tftpstr, strerror(errno));
                // tftp_close(tftp);
                return -1;
            }


            // did we timeout or did we receive data from the FD?
            if (! FD_ISSET(tftp->sd, &fdset)) {
                // punishment for timing out..
                retries--;
                if (! retries) {
                    fprintf(stderr,
                            "%s: timeout, aborting data transfer\n",
                            tftpstr);
                    // tftp_close(tftp);
                    return -1;
                }
                // try again!! (retransmit)
                continue;
            } else {
                //measure rtt from (karn's algo) first transmission of the unique packet.

               if(FSM_DEBUG) printf("timeout from after select  (successful) (LINUX:amount of time not slept - indicative only) : %d:%d\n", timeout.tv_sec, timeout.tv_usec);
               if(FSM_DEBUG) printf("timeout from after select  (successful) backoff used : %d:%d\n", tftp->backoff.tv_sec, tftp->backoff.tv_usec);
               if(FSM_DEBUG) printf("timeout from after select  (successful) RTO used : %d:%d\n", rto.tv_sec, rto.tv_usec);
               if(FSM_DEBUG) printf("time of original transmission : %d:%d\n", originaltrans.tv_sec, originaltrans.tv_usec);
               if(FSM_DEBUG) printf("time of recent transmission     : %d:%d\n", transmissiontime.tv_sec, transmissiontime.tv_usec);
               gettimeofday(&now, NULL);
               timersub(&now, &originaltrans, &mrtt);
               if(FSM_DEBUG||1) printf("time since original transmission (mrtt) : %d:%d\n", mrtt.tv_sec, mrtt.tv_usec);

                //success, reset backoff
                tftp->backoff.tv_sec = 0;
                tftp->backoff.tv_usec = 0;
                
                // Jacobson's internet saving algorithm
                jacobson_rto(&mrtt, &srtt, &drtt, &vrtt, &rto);

               if(FSM_DEBUG) printf("RTO: %d:%d\n", rto.tv_sec, rto.tv_usec);

            }




            tftp->addrlen = sizeof((struct sockaddr *) tftp->addr);
            buflen = recvfrom(tftp->sd, buf, sizeof(buf), 0, (struct sockaddr *) tftp->addr, &tftp->addrlen);
            if (buflen == -1) {
                fprintf(stderr, "%s: recvfrom: %s\n", tftpstr, strerror(errno));
                // tftp_close(tftp);
                return -1;
            }
            if(FSM_DEBUG) fprintf(stdout, "%s: recvfrom: %s\n", tftpstr, tftp->addr);
            if(tftp->opcode > 5) {
                if(tftp->opcode < TFUP_OPCODE_RLS) { //RTS-response
                    if (tfup_dec_opcode(buf, buflen, &opcode) != 0) {
                        errmsg("failed to parse tfup opcode in message");
                        continue;
                    }
                } else {
                    //nothing.
                }
            } else {
                if (tftp_dec_opcode(buf, buflen, &opcode) != 0) {
                    errmsg("failed to parse tftp opcode in message");
                    continue;
                }
            }
        } else {
                // fprintf(stdout, "%s (pid:%d): current tftp protocol msg=%s\n", tftpstr, getpid(), (tftp->msg));
            if (tftp->msglen == -1) {
                // fprintf(stderr, "%s: recvfrom (via server primary port): %s\n", tftpstr, strerror(errno));
                // tftp_close(tftp);
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
            if(opcode == TFTP_OPCODE_RRQ) { //we need to send a data block blkno 1 as ack. (not in tfup)
                    tftp->state = TFTP_STATE_RRQ_SEND_ACK;
            } else if(opcode == TFTP_OPCODE_WRQ){ //@todo: we need to send a blkno 0 as ack (not in tfup)
                    tftp->state = TFTP_STATE_WRQ_SEND_ACK;
            } else {
                    //ubnknown/unhandled state transition - @TODO
                    errmsg("unknown opcode in message (server)");
                    exit(1);
            }

        }

        // fprintf(stdout, "%s (pid:%d): current on tftp protocol state=%s\n", tftpstr, getpid(), tftpstates[(tftp->state)+1]);
        // fprintf(stdout, "%s (pid:%d): got opcode=%s\n", tftpstr, getpid(), tftpopcodes[opcode-1]);

        switch (tftp->state) { //control our opcode based on state.
            case TFUP_STATE_RTS_SENT:
                opcode = TFUP_OPCODE_TIME;
                if(FSM_DEBUG) fprintf(stdout, "%s (pid:%d): ending opcode=%s\n", tftpstr, getpid(), tftpopcodes[opcode-1]);
                break;
            case TFUP_STATE_RLS_SENT:
                opcode = TFUP_OPCODE_RLS;
                if(FSM_DEBUG) fprintf(stdout, "%s (pid:%d): ending opcode=%s\n", tftpstr, getpid(), tftpopcodes[opcode-1]);
                break;
            case TFTP_STATE_RRQ_SENT: //a client will write and ACK
            case TFTP_STATE_WRQ_SEND_ACK: //a server will write and ACK
            case TFTP_STATE_ACK_SENT: //con't to keep sending ACK
                opcode = TFTP_OPCODE_ACK;
                tftp->opcode=opcode;
                if(FSM_DEBUG) fprintf(stdout, "%s (pid:%d): performing opcode=%s\n", tftpstr, getpid(), tftpopcodes[opcode-1]);
                break; //expect to send ACK
            case TFTP_STATE_WRQ_SENT: //a client will read and send DATA
            case TFTP_STATE_RRQ_SEND_ACK: ////a server will read and send DATA
            case TFTP_STATE_DATA_SENT: //con't to keep sending DATA
            case TFTP_STATE_LAST_DATA_SENT:
                opcode = TFTP_OPCODE_DATA;
                if(FSM_DEBUG) fprintf(stdout, "%s (pid:%d): performing opcode=%s\n", tftpstr, getpid(), tftpopcodes[opcode-1]);
                break; //expect to send ACK
            default:
                opcode = TFTP_OPCODE_ERROR; break; //unknown state.. very bad.
        }
        switch (opcode) {
            case TFUP_OPCODE_TIME:
                return tfup_dec_timestamp(buf, buflen, tftp);
                break;
            case TFUP_OPCODE_RLS:
                printf("reading rlist file\n");
                unsigned char *dat;
                int datal;
                
                if (tfup_dec_rls_data(buf, buflen, &dat, &datal) != 0) {
                        errmsg("failed to decode data/datalen");
                        return -1;
                }
                fwrite(dat, sizeof(char), buflen-2, stdout);
                // printf("\n\n%s\n\n", dat);
                return 0;
                break;
            case TFTP_OPCODE_ACK: //the client or server will do this
                errmsg("starting ACK out\n");
                if (tftp->state != TFTP_STATE_WRQ_SEND_ACK ) { //i'm a client.
                    //no blk number in tfup server this time.....
                    // if (tftp_dec_blkno(buf, buflen, &blkno) != 0) {
                    //     errmsg("failed to decode block number in data packet");
                    //     continue;
                    // }
                    blkno= (blkno<1)?blkno=1:blkno+1; //not used at all in assignment 2 (tfup) thsis just for local logic. payload has no blkno so assume theres always a good blkno.
                    //switch to new communicate with the server's spawned process's remote port
                    if(blkno<=1) { //hmm..security issue?-> update this port everytime after recvfrom() in this function?
                        //(gdb) p ntohs(((struct sockaddr_in *) &tftp.addr).sin_port)
                        struct sockaddr_in *sin;
                        char port[6];
                        sin = (struct sockaddr_in *) tftp->addr;
                        int remote_port = ntohs(sin->sin_port);
                        snprintf(port, sizeof(port), "%d", remote_port);
                        // printf("point tftp to server's new origin socket.\n");
                        tftp->port = (char*) &port;
                    }
                } else { //doing ACK (not RRQ/ normal ACK) ; ie: WRQ ack...
                    tftp->blkno=0;
                    blkno=0;
                }
                // if (blkno == tftp->blkno) { // no more blkno, must use timer to reply or ignore. so ignore the ones after 1st reply ...
                    unsigned char *data;
                    int len, datalen;

                    if (tftp->blkno>0) {
                        if (tftp_dec_data(buf, buflen, &data, &datalen) != 0) {
                            errmsg("failed to decode data/datalen");
                        } else {
                            if(FSM_DEBUG) fprintf(stderr, "%s: got data to write (size:%d)\n", tftpstr, datalen);
                        }
                        len = write(tftp->fd, data, datalen);
                        if (len == -1) {
                            fprintf(stderr, "%s: write: %s\n",
                                    tftpstr, strerror(errno));
                            // tftp_close(tftp);
                            return -1;
                        }
                    } else {
                        if (tftp_dec_filename_mode(buf, tftp) != 0) {
                            errmsg("failed to decode filename in RRQ packet or unsupported tftp mode");
                            // tftp_close(tftp);
                            continue;
                        }
                        tftp->target=tftp->file;
                        file_write(tftp->file, &tftp->fd);
                        if(FSM_DEBUG) fprintf(stderr, "%s: write : %s\n",tftpstr, strerror(errno));
                    }

                    if (tftp_enc_packet(tftp, TFTP_OPCODE_ACK, tftp->blkno, NULL, 0) == -1) { //can use blkno as tranmission count also.
                        fprintf(stderr, "%s: encoding error\n", tftpstr);
                        return -1;
                    } else {
                        // errmsg("i'm writing the file. ack packet prepped: opcode_ack");
                    }
                    tftp->blkno++;
                    timerclear(&tftp->timer); //preserve timer across operation
                    retries = TFTP_DEF_RETRIES;
                    if (blkno > 0) {
                        tftp->state = (datalen == TFTP_BLOCKSIZE ) ?
                            TFTP_STATE_ACK_SENT : TFTP_STATE_LAST_ACK_SENT;
                    } else {
                        tftp->state = TFTP_STATE_ACK_SENT;
                    }

                // }

                break;
            case TFTP_OPCODE_DATA:
                errmsg("starting DATA out\n");
                if (tftp->state != TFTP_STATE_RRQ_SEND_ACK ) {
                    //no blk number in tfup server this time.....
                    // if (tftp_dec_blkno(buf, buflen, &blkno) != 0) {
                    //     errmsg("failed to decode block number in ack packet");
                    //     continue;
                    // }
                    blkno= (blkno<1)?blkno=1:blkno+1;
                    // if (blkno != tftp->blkno) {
                    //     errmsg("ignoring unexpected block numer in ack packet"); //RRQ
                    //     continue;
                    // }
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
                        // tftp_close(tftp);
                        return -1;
                    }
                    if (tftp_enc_packet(tftp, TFTP_OPCODE_DATA, ++tftp->blkno, buf, len) == -1) {
                        fprintf(stderr, "%s: encoding error\n", tftpstr);
                        return -1;
                    } else {
                        // errmsg("i'm reading the file. data packet prepped: opcode_data");
                    }
                    timerclear(&tftp->timer); //preserve timer across operation
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
        if(FSM_DEBUG) fprintf(stdout, "%s (pid:%d): current block number:%d (prepared), current tftp protocol state=%s\n",
        tftpstr, getpid(), tftp->blkno, tftpstates[(tftp->state)+1]);
    }

    return returnstatus;
}
