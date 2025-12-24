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

static packet_t *last_sent_packet;  
static size_t last_packet_full_size; 
static int waiting_for_ack;         
static uint32_t next_seq_to_send;   
static uint32_t expected_seq_num;   
static long timeout_ns;             


void connection_initialization(int window_size, long timeout_in_ns)
{
    last_sent_packet = (packet_t *) malloc(sizeof(packet_t));
    memset(last_sent_packet, 0, sizeof(packet_t));

    waiting_for_ack = 0;      
    next_seq_to_send = 1;     
    expected_seq_num = 1;     
    timeout_ns = timeout_in_ns; 
}

void receive_callback(packet_t *pkt, size_t pkt_size)
{
    if (!VALIDATE_CHECKSUM(pkt, pkt_size)) {
        return; 
    }

    if (pkt->ackno > 0) {
        if (pkt->ackno > last_sent_packet->seqno) {
            CANCEL_TIMER(0);           
            waiting_for_ack = 0;       
            next_seq_to_send = pkt->ackno; 
            RESUME_TRANSMISSION();     
        }
    } 
    else {
        
        if (pkt->seqno == expected_seq_num) {
            ACCEPT_DATA(pkt->data, pkt_size - DATA_PACKET_HEADER); 
            expected_seq_num++; 
        }
        
        packet_t ack_pkt;
        memset(&ack_pkt, 0, sizeof(packet_t));
        ack_pkt.ackno = expected_seq_num;
        ack_pkt.seqno = 0; 
        SEND_PACKET_TO_NETWORK(&ack_pkt, ACK_PACKET_SIZE); [cite: 197]
    }
}

void send_callback()
{
    if (waiting_for_ack) {
        PAUSE_TRANSMISSION();
        return;
    }

    int bytes_read = READ_DATA_FROM_APP_LAYER(last_sent_packet->data, MAX_PAYLOAD_SIZE);

    if (bytes_read > 0) {
        last_sent_packet->seqno = next_seq_to_send;
        last_sent_packet->ackno = 0;
        last_sent_packet->len = bytes_read;
        
        last_packet_full_size = bytes_read + DATA_PACKET_HEADER;

        SEND_PACKET_TO_NETWORK(last_sent_packet, last_packet_full_size);
        SET_TIMER(0, timeout_ns); 
        
        waiting_for_ack = 1; 
        PAUSE_TRANSMISSION();
    }
}

void timer_callback(int timer_number)
{
    if (timer_number == 0 && waiting_for_ack) {
        SEND_PACKET_TO_NETWORK(last_sent_packet, last_packet_full_size); 
        SET_TIMER(0, timeout_ns); 
    }
}