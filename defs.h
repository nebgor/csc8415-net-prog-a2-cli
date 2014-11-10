#include <stdlib.h>
#include <fcntl.h>

#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#define TFTP_SVR_PORT 69
//#define TFTP_SVR_STATE_LISTEN 0
//#define TFTP_SVR_STATE_HANDLE 0

//Most of below is as numerically defined in RFC 1350
#define TFTP_OPCODE_RRQ		1
#define TFTP_OPCODE_WRQ		2
#define TFTP_OPCODE_DATA	3
#define TFTP_OPCODE_ACK		4
#define TFTP_OPCODE_ERROR	5

//Add on TFUP protocol design. implemented as an extended tftp design here.

// request timestamp
#define TFUP_OPCODE_RTS   6
// request timestamp response
#define TFUP_OPCODE_TIME   7
// 'rlist' - requests a list of file in the remote. client may implement an 'llist' locally.
#define TFUP_OPCODE_RLS   8

#define TFTP_DEF_RETRIES	10
#define TFTP_DEF_TIMEOUT_SEC	0
// start at around double the round the earth rtt 133ms=250ms
#define TFTP_DEF_TIMEOUT_USEC	250000
#define TFTP_BLOCKSIZE		510
//
#define TFTP_MAX_MSGSIZE	(2 + TFTP_BLOCKSIZE)

#define TFTP_MODE_OCTET		"octet"
//Use mainly octet but rest are for extending
#define TFTP_MODE_NETASCII	"netascii"
#define TFTP_MODE_MAIL		"mail"

#define TFTP_ERR_NOT_DEFINED	0
#define TFTP_ERR_NOT_FOUND	1
#define TFTP_ERR_ACCESS_DENIED	2
#define TFTP_ERR_DISK_FULL	3
#define TFTP_ERR_UNKNOWN_TID	4
#define TFTP_ERR_ILLEGAL_OP	5
#define TFTP_ERR_FILE_EXISTS	6
#define TFTP_ERR_NO_SUCH_USER	7

//see lib for a helpful array of this too... state debugging.
#define TFTP_STATE_OPENED	  -1
#define TFTP_STATE_CLOSED	  0
#define TFTP_STATE_RRQ_SENT	  1
#define TFTP_STATE_WRQ_SENT	  2
#define TFTP_STATE_DATA_SENT	  3
#define TFTP_STATE_LAST_DATA_SENT 4
#define TFTP_STATE_ACK_SENT	  5
#define TFTP_STATE_LAST_ACK_SENT  6
//two more for server to merge into unified implementation of FSM
#define TFTP_STATE_RRQ_SEND_ACK	  7
#define TFTP_STATE_WRQ_SEND_ACK	  8

#define TFUP_STATE_RTS_SENT 9
#define TFUP_STATE_RLS_SENT 10

//the most useful struct
typedef struct tftp {
    char *host; //literal address
    char *port;
    char *mode;
    char *file;
    char *target;	// target filename
    int  sd;		//socket descriptor
    int  fd;		//file descriptor
    int		  state;	// state of the TFTP state machine
    int      blkno;	// current block number
    struct timeval backoff;
    struct timeval timer;
    struct sockaddr *addr;	// address of the server
    int addrlen;
    unsigned char msg[TFTP_MAX_MSGSIZE]; //tftp msg send buffer
    int        msglen;		//tftp msg send buffer len
    time_t    timestamp;
    int opcode;
} tftp_t;
