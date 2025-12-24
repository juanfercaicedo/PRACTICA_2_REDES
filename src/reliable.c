#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rlib.h"

static packet_t *buffer_packet;    
static size_t last_payload_size;  
static int waiting_for_ack;       
static uint32_t next_seqno;       
static uint32_t expected_seqno;   
static long timeout_duration;     

void connection_initialization(int window_size, long timeout_in_ns) {
    buffer_packet = (packet_t *) malloc(sizeof(packet_t));
    memset(buffer_packet, 0, sizeof(packet_t));

    waiting_for_ack = 0;
    next_seqno = 1;      
    expected_seqno = 1;  
    timeout_duration = timeout_in_ns;
    
    fprintf(stderr, "Inicializado: Stop & Wait. Timeout: %ld ns\n", timeout_duration);
}

void send_callback() {
    if (waiting_for_ack) {
        PAUSE_TRANSMISSION();
        return;
    }

    char data_buffer[MAX_PAYLOAD_SIZE];
    int bytes_read = READ_DATA_FROM_APP_LAYER(data_buffer, MAX_PAYLOAD_SIZE);

    if (bytes_read > 0) {
        buffer_packet->seqno = next_seqno;
        buffer_packet->ackno = 0; 
        buffer_packet->len = bytes_read;
        memcpy(buffer_packet->data, data_buffer, bytes_read);

        last_payload_size = bytes_read;
        SEND_PACKET_TO_NETWORK(buffer_packet, bytes_read + HEADER_SIZE);
        SET_TIMER(0, timeout_duration); 

        waiting_for_ack = 1;
        PAUSE_TRANSMISSION(); 
    }
}

void receive_callback(packet_t *pkt, size_t pkt_size) {
    if (!VALIDATE_CHECKSUM(pkt, pkt_size)) {
        fprintf(stderr, "Trama corrupta recibida. Ignorando...\n");
        return; 
    }

    if (pkt->ackno > 0) {
        if (pkt->ackno > next_seqno) {
            CANCEL_TIMER(0);      
            waiting_for_ack = 0;
            next_seqno = pkt->ackno;
            RESUME_TRANSMISSION(); 
        }
    } 
    else if (pkt->seqno > 0) {
        if (pkt->seqno == expected_seqno) {
            ACCEPT_DATA(pkt->data, pkt->len); 
            expected_seqno++;
        }
        
        packet_t ack_pkt;
        memset(&ack_pkt, 0, sizeof(packet_t));
        ack_pkt.ackno = expected_seqno;
        ack_pkt.seqno = 0;
        ack_pkt.len = 0;
        SEND_PACKET_TO_NETWORK(&ack_pkt, HEADER_SIZE);
    }
}


void timer_callback(int timer_number) {
    if (timer_number == 0 && waiting_for_ack) {
        fprintf(stderr, "Timeout! Retransmitiendo secuencia %u\n", buffer_packet->seqno);
        
        SEND_PACKET_TO_NETWORK(buffer_packet, last_payload_size + HEADER_SIZE);
        
        SET_TIMER(0, timeout_duration);
    }
}