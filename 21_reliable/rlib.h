#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>

/*
	This code is adapted for Universidad del Atlantico - "Redes de Ordenadores"
from the code used for the following labs:
		- Standford "CS144" - Reliable transport
		- Universidad de Cantabria "Introduccion a las Redes de Computadores" -
	Design an analysis of a flow control protocol based on stop and wait and
	sliding window.
*/

/*------------------------------------------------------------------------------
|					YOU SHOULD UNDERSTAND FROM HERE ONWARDS!				   |
------------------------------------------------------------------------------*/

/* #-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-
	Simple flow control protocol:

	There are two kinds of packets, Data packets and Ack-only packets. You can
tell the type of a packet by length. Ack packets are 8 bytes, while Data
packets vary from 12 to 512 bytes.

	Every Data packet contains a 32-bit sequence number and a variable length
payload that can vary from 0 up to MAXIMUM_PAYLOAD bytes.

	Both Data and Ack packets contain the following fields:
		- cksum: 16-bit checksum. This checksum is automatically calculated
	for each packet transmitted. However, when the packet is received,
	you have to validate if the frame is corrupt with the VALIDATE_CHECKSUM
	function.
		- len: 16-bit total length of the packet.  This will be 8 for Ack
	packets, and 10 + payload-size for data packets (since 10
	bytes are used for the header of data packets).
		- ackno: 32-bit acknowledgement index. The meaning of this index is up to
	the programmer. Essentially, there are two different options:
			- Confirms the reception of the frame with index "ackno".
			- Comfirms the reception of the frames previous to "ackno".
		and that the system is waiting for the frame "ackno".
	Note that data packets do not need to use this value.

	The following fields only exist in a  packet:
		- seqno: Each packet transmitted in a stream of data must be numbered
	with a sequence number, "seqno". The first packet in a stream has
	seqno 1. Note that in TCP, sequence numbers indicate bytes, whereas by
	contrast this protocol just numbers packets.
		- data:  Contains (len - 12) bytes of payload data for the
	application.
*/

// Ack-only packet type comprises 3 fields (8 bytes in total).
struct ack_packet
{
	uint16_t cksum;
	uint16_t len;
	uint32_t ackno;
};

// Defines the reserved space for payload in data packets
#define MAX_PAYLOAD 500

// Data packets' header comprises 4 fields (12 bytes).
struct packet
{
	uint16_t cksum;
	uint16_t len;
	uint32_t ackno;
	uint32_t seqno; // Only valid if len > 8
	char data[MAX_PAYLOAD];
};
typedef struct packet packet_t;

// This macro allows to diferentiate between ack-only and data packets.
#define IS_ACK_PACKET(packet) ((packet->len) == ACK_PACKET_SIZE)

/*
	Important notes about the framework:

	The generated program, once completed, models a link with flow-control
between to endpoints (same or different PCs). Real messages are sent between
these endpoints over the network. These messages are sent over UDP, IP (and
Ethernet); however, this is non visible to the programmer.

	There are two modes of operation. The default mode employs the console for
input and output of data: when you write something on an endpoint, this string
is sent to the other endpoint and printed on the console. Alternatively, the
runtime can model synthetic traffic that generates fixed size packets as fast
as possible. You have to use the flag -s to active this mode.

	You can pass certain variables to the program, which are passed to the
"connection_initialization" function:
		- window:  Tells you the size of the sliding window (which will
	be 1 for stop-and-wait). You can ignore the case of window > 1 (sliding
	window).
		- timeout: Tells you what your retransmission timer should be, in
	nanoseconds. If after these many nanoseconds a packet you sent has still not
	been acknowledged, you should retransmit the packet. You can use the
	function SET_TIMER to activate a timer. The default timer is 10 ms
	(10000000 ns).

	The framework can model frame errors; you can specify the frame corruption
probability using the -e flag. By default, there is no error corruption
(although the real-world network that transmits the packets might loss some
traffic). The frames have a checksum field in the header, which is
automatically calculated and set by the library. However, you have to manually
verify it using an API function described later. When a corruption occurs and a
data packet does not receive the corresponding ack, it has to be retransmitted.
You should have activated a timer for this. There are up to 16 different timers
(0 to 15) which can be active concurrently. When a timer expires, the
timer_callback function is called. The timeout of the timers is defined in
nanoseconds; you have to compile with -lrt for this to work (this is already
included in the Makefile).

	After transmission starts, the application displays statistics every 10
seconds. Note that these stats can consider either the "sent bytes" (including
data and headers, and possibly from a duplicated frame), or "application bytes"
(only the accepted data fields, without any duplication). Note that the
transmission speed (in the sender side) and the average speed in the application
level (in the receiver side) can differ significantly, especially when there are
errors.

	To aid debugging, you can use the -d flag, specifying a verbosity level.
There are 3 verbosity levels (1 to 3). When debugging is active, some messages
are printed regarding the transmission and reception of packets, accepted data
in the application layer, and errors. Each of these messages is displayed in a
different colour. You can also print messages from your functions to help
debugging (use "printf" for that).
*/

/*
	Your task is to implement the following four functions:
connection_initialization, recv_callback, send_callback, timer_callback as well
to describe the connection state data.  All the changes you need to make should
be done in file reliable.c.

	Important: You do not have to change anything in rlib.h or rlib.c. Indeed,
you do not need to understand the major part of the code in those files.
*/

/*
	This function is called only once at the beginning of the execution. It
should initialize all the necessary variables for the protocol. Parameters:
		- int window_size: the size of the window, as declared by the -w flag.
	This window size can be ignored in stop & wait protocol.
		- long timeout_in_ns: the timeout value to be used in the protocol.
*/
void connection_initialization(int window_size, long timeout_in_ns);

/*
	This function is called when a packet arrives. The packet can be accessed
with the pkt pointer. The length (number of bytes received) is also passed.
Note that you should validate the checksum of the packet using the
VALIDATE_CHECKSUM API call; if the checksum fails, no field can be trusted.
*/
void receive_callback(packet_t *pkt, size_t len);

/*
	This function is called when the application has data to be sent. Note that
if you call PAUSE_TRANSMISSION this function is never called, until you resume
the transmission.
*/
void send_callback();

// This function is called when the timer timer_number expires
void timer_callback(int timer_number);

/*
	Additionally, there are several API functions that allow you to interact
with the lower layer (physical: send and receive data) and the upper layer
(application: stop/resume transmission, accept data to be sent, pass data that
has been correctly received from the network. The name of these functions
employs capital letters. You can see the prototypes and description of them
following:
*/

/*
	Call this function to accept the data from the packets you have received.
The function returns number of bytes written accepted on success, or -1 if
there has been an error. Parameters:
		- *buf is a pointer to the location of the array of data.
		- len is the length of the data (not the whole packet), in bytes
	You shall use this function when the library calls receive_callback function
to accept the data you have received in the frame.
*/
int ACCEPT_DATA(const void *buf, size_t len);

/*
	Call this function to read a block of data from the higher level of the
protocol stack (the console or the synthetic traffic generator). This function
returns the number of bytes received, 0 if there is no data currently available,
and -1 on EOF or error. Parameters:
		- *buf is a pointer to an area of memory where the data will be copied.
		- len is the maximum amount of data that can be accepted, in bytes.
	You shall use this function to get the input data that you must send in your
packets when the library calls send_callback because this implies that the
application has data to be sent.
*/
int READ_DATA_FROM_APP_LAYER(void *buf, size_t len);

/*
	Call this function to send a complete packet to the other side. You have to
provide all the fields in the packet header, and a pointer to the data stream
to be sent. Note that length parameter refers to the complete packet length in
bytes, including the header fields plus the data field (payload). Parameters:
		- length: the length of the whole packet (header + data) in bytes.
		- ackno: the value to be sent in the ackno field of the data packet.
		- seqno: the sequence index to use.
		- *data: a pointer to the array of data.
	You shall use this function to transmit a packet with the information
retrieved from the application when the library calls send_callback.
*/
int SEND_DATA_PACKET(uint16_t length, uint32_t ackno, uint32_t seqno, void *data);

/*
	This function validates the checksum to determine if the packet has been
corrupted in transit. This function returns 0 if the packet is corrupted and
1 if not. Parameters:
		- *pkt is a pointer to the packet to be validated
	You shall use this function when the library calls receive_callback for a
data packet in order to detect if you need to send and ack or not for this seqno.
*/
int VALIDATE_CHECKSUM(const packet_t *pkt);

/*
	Call this function to send an ack-only packet to the other side. The packet
type and length are implicit, and the sequence number and data fields are
omitted according to the packet type specification. Parameters:
		- ackno: The value to be sent in the ACK field of the data packet.
	You shall use this function to confirm the reception of a data packet if it
is not corrupted.
*/
int SEND_ACK_PACKET(uint32_t ackno);

/*
	This function activates a timer that will expire in timer_delay_ns
nanoseconds. There are 16 different timers available, using numbers from 0 to 15.
You can set multiple timers concurrently, each of them with its own deadline.
	You shall use this function when a packet has been sent to determine if that
packet should be re-transmitted or not based on the reception of an ack for it.
*/
long SET_TIMER(int timer_number, long timer_delay_ns);

/*
	This function clears the timer with index timer_number. It returns -1 if the
timer wasn't set; otherwise, it returns the remaining time in ns.
	You shall use this function when an expected ackno is received.
 */
long CLEAR_TIMER(int timer_number);

/*
	Call these functions to temporarily pause or continue the transmission of
data, respectively. When PAUSE_TRANSMISSION is called, the upper layer will not
generate any more data to be sent (so send_callback is never called), until
RESUME_TRANSMISSION is used.
	You shall pause the transmission when the flow control does not allow to
send more frames (for example, when the window closes), and resume when the
expected ACKs are received.
*/
void PAUSE_TRANSMISSION();
void RESUME_TRANSMISSION();

/*------------------------------------------------------------------------------
|							YOU CAN STOP READING NOW!!						   |
|	You do not need to understand from here onwards to do your assignment.	   |
------------------------------------------------------------------------------*/

#define TIMER_COUNT 16
#define ACK_PACKET_SIZE 8
#define DATA_PACKET_HEADER 12

struct config_common
{
	int window;	  /* # of unacknowledged packets in flight */
	long timeout; /* Retransmission timeout in nanoseconds*/
	float error_probability;
};

/*
from http://stackoverflow.com/questions/3219393/
http://en.wikipedia.org/wiki/ANSI_escape_code#Colors
*/
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_BLACK "\x1b[0m"
#define ANSI_COLOR_RESET ANSI_COLOR_BLACK

#define DEBUG_TIMER(priority, MESSAGE, ...)                                                     \
	{                                                                                           \
		if (opt_debug >= priority)                                                              \
		{                                                                                       \
			printf(ANSI_COLOR_BLUE "    TIMER: " MESSAGE ANSI_COLOR_RESET "\n", ##__VA_ARGS__); \
			fflush(stdout);                                                                     \
		}                                                                                       \
	}

#define DEBUG_RECEPTION(priority, MESSAGE, ...)                                                      \
	{                                                                                                \
		if (opt_debug >= priority)                                                                   \
		{                                                                                            \
			printf(ANSI_COLOR_GREEN "    RECEPTION: " MESSAGE ANSI_COLOR_RESET "\n", ##__VA_ARGS__); \
			fflush(stdout);                                                                          \
		}                                                                                            \
	}

#define DEBUG_SEND(priority, MESSAGE, ...)                                                        \
	{                                                                                             \
		if (opt_debug >= priority)                                                                \
		{                                                                                         \
			printf(ANSI_COLOR_MAGENTA "    SEND: " MESSAGE ANSI_COLOR_RESET "\n", ##__VA_ARGS__); \
			fflush(stdout);                                                                       \
		}                                                                                         \
	}

#define DEBUG_ERRORS(priority, MESSAGE, ...)                                                    \
	{                                                                                           \
		if (opt_debug >= priority)                                                              \
		{                                                                                       \
			printf(ANSI_COLOR_RED "    ERRORS: " MESSAGE ANSI_COLOR_RESET "\n", ##__VA_ARGS__); \
			fflush(stdout);                                                                     \
		}                                                                                       \
	}

#define DEBUG_MSG(priority, COLOR, MESSAGE) \
	{                                       \
		if (opt_debug >= priority)          \
		{                                   \
			printf("\(COLOR) (MESSAGE)");   \
			fflush(stdout);                 \
		}                                   \
	}

extern char *progname; /* Set to name of program by main */
extern int opt_debug;  /* When != 0, print packets */

void *xmalloc(size_t);
uint16_t cksum(const void *_data, int len); /* compute TCP-like checksum */

/* Returns 1 when two addresses equal, 0 otherwise */
int addreq(const struct sockaddr_storage *a, const struct sockaddr_storage *b);

/* Actual size of the real socket address structure stashed in a
 sockaddr_storage. */
size_t addrsize(const struct sockaddr_storage *ss);

/* Useful for debugging. */
void print_pkt(const packet_t *buf, const char *op, int n);

/* Call this function to send a UDP packet to the other side. */
int SEND_PACKET(const packet_t *pkt, size_t len);

/* Below are some utility functions you don't need for this lab */

/* Fill in a sockaddr_storage with a socket address.  If local is
 * non-zero, the socket will be used for binding.  If dgram is
 * non-zero, use datagram sockets (e.g., UDP).  If unixdom is
 * non-zero, use unix-domain sockets.  name is either "port" or
 * "host:port". */
int get_address(struct sockaddr_storage *ss, int local, int dgram, int unixdom, char *name);

/* Put socket in non-blocking mode */
int make_async(int s);

/* Bind to a particular socket (and listen if not dgram). */
int listen_on(int dgram, struct sockaddr_storage *ss);

/* Convenient way to get a socket connected to a destination */
int connect_to(int dgram, const struct sockaddr_storage *ss);

#include <time.h>
#include <sys/time.h>

#ifndef CLOCK_REALTIME
#define NEED_CLOCK_GETTIME 1
#define CLOCK_REALTIME 0
#undef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif /* !HAVE_CLOCK_GETTIME */

#if NEED_CLOCK_GETTIME
int clock_gettime(int, struct timespec *);
#endif /* NEED_CLOCK_GETTIME */
