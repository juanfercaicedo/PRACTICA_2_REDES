#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>

#include "rlib.h"

static packet_t *last_packet;     
static size_t last_packet_size;   
static int waiting_for_ack;       
static uint32_t next_seqno;       
static uint32_t expected_seqno;   
static long timeout_val;          




void connection_initialization(int window_size, long timeout_in_ns) {
    last_packet = (packet_t *) malloc(sizeof(packet_t));
    memset(last_packet, 0, sizeof(packet_t));
    
    waiting_for_ack = 0;
    next_seqno = 1;      
    expected_seqno = 1;
    timeout_val = timeout_in_ns; 
}

void receive_callback(packet_t *pkt, size_t pkt_size) {
    if (!VALIDATE_CHECKSUM(pkt)) {
        return; 
    }

    if (pkt->ackno > 0) {
        if (pkt->ackno >= next_seqno) {
            SET_TIMER(0, -1);           
            waiting_for_ack = 0;
            RESUME_TRANSMISSION();    
        }
    } 
    else {
        if (pkt->seqno == expected_seqno) {
            ACCEPT_DATA(pkt->data, pkt->len); 
            expected_seqno++;
        }
        
        packet_t ack_pkt;
        memset(&ack_pkt, 0, sizeof(packet_t));
        ack_pkt.ackno = expected_seqno;
        ack_pkt.seqno = 0;
        ack_pkt.len = 0;
        
        rel_sendpkt(ack_pkt.ackno, &ack_pkt, ACK_PACKET_SIZE);
    }
}

void send_callback() {
    if (waiting_for_ack) {
        PAUSE_TRANSMISSION();
        return;
    }

    int bytes_read = READ_DATA_FROM_APP_LAYER(last_packet->data, MAX_PAYLOAD);

    if (bytes_read > 0) {
        last_packet->seqno = next_seqno;
        last_packet->ackno = 0;
        last_packet->len = bytes_read;
        last_packet_size = bytes_read + DATA_PACKET_HEADER;

        rel_sendpkt(last_packet->seqno, last_packet, last_packet_size);
        SET_TIMER(0, timeout_val); 
        
        next_seqno++;
        waiting_for_ack = 1; 
        PAUSE_TRANSMISSION();
    }
}


void timer_callback(int timer_number) {
    if (timer_number == 0 && waiting_for_ack) {
        rel_sendpkt(last_packet->seqno, last_packet, last_packet_size);
        SET_TIMER(0, timeout_val);
    }
}