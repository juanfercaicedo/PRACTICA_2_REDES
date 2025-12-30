#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "rlib.h"

static char last_payload[MAX_PAYLOAD]; 
static int last_payload_size;          
static int waiting_for_ack;            
static uint32_t next_seqno;            
static uint32_t expected_seqno;        
static long timeout_val;               


void connection_initialization(int window_size, long timeout_in_ns) {
    waiting_for_ack = 0;
    next_seqno = 1;      
    expected_seqno = 1; 
    timeout_val = timeout_in_ns; 
    last_payload_size = 0;
}

void receive_callback(packet_t *pkt, size_t pkt_size) {
    if (VALIDATE_CHECKSUM(pkt) == 0) {
        return; 
    }

    if (IS_ACK_PACKET(pkt)) {
        if (waiting_for_ack && pkt->ackno > next_seqno - 1) {
            CLEAR_TIMER(0);           
            waiting_for_ack = 0;
            RESUME_TRANSMISSION();    
        }
    } 
    else {
        if (pkt->seqno == expected_seqno) {
            ACCEPT_DATA(pkt->data, pkt->len - DATA_PACKET_HEADER); 
            expected_seqno++; 
        }
        
        SEND_ACK_PACKET(expected_seqno);
    }
}

void send_callback() {
    if (waiting_for_ack) {
        PAUSE_TRANSMISSION(); 
        return;
    }

    char buffer[MAX_PAYLOAD];
    int bytes_read = READ_DATA_FROM_APP_LAYER(buffer, MAX_PAYLOAD);

    if (bytes_read > 0) {
        memcpy(last_payload, buffer, bytes_read);
        last_payload_size = bytes_read;

        SEND_DATA_PACKET(bytes_read + DATA_PACKET_HEADER, 0, next_seqno, last_payload);
        
        SET_TIMER(0, timeout_val); 
        
        next_seqno++;
        waiting_for_ack = 1; 
        PAUSE_TRANSMISSION(); 
    }
}

void timer_callback(int timer_number) {
    if (timer_number == 0 && waiting_for_ack) {
        SEND_DATA_PACKET(last_payload_size + DATA_PACKET_HEADER, 0, next_seqno - 1, last_payload);
        SET_TIMER(0, timeout_val); 
    }
}