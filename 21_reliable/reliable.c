#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rlib.h"

/* Sección 2: Variables Globales */
static packet_t *last_packet;      
static size_t last_packet_len;     
static int is_waiting_for_ack;     
static uint32_t current_seqno;     
static uint32_t expected_seqno;    
static long timeout_duration;      

/* Sección 3: Implementación de Callbacks */

void connection_initialization(int window_size, long timeout_in_ns) {
    last_packet = (packet_t *) malloc(sizeof(packet_t));
    memset(last_packet, 0, sizeof(packet_t));

    is_waiting_for_ack = 0;
    current_seqno = 1; 
    expected_seqno = 1;
    timeout_duration = timeout_in_ns; 
}

void receive_callback(packet_t *pkt, size_t pkt_size) {
    // 1. Validar integridad según pide la práctica 
    if (!VALIDATE_CHECKSUM(pkt)) {
        return; 
    }

    // 2. Lógica del Emisor: Recibimos un ACK
    if (pkt->ackno > 0) {
        if (pkt->ackno > last_packet->seqno) {
            SET_TIMER(0, -1);           
            is_waiting_for_ack = 0;
            current_seqno = pkt->ackno; 
            RESUME_TRANSMISSION();      
        }
    } 
    // 3. Lógica del Receptor: Recibimos Datos
    else {
        if (pkt->seqno == expected_seqno) {
            ACCEPT_DATA(pkt->data, pkt->len); 
            expected_seqno++;
        }
        
        // Enviar confirmación (ACK)
        packet_t ack_pkt;
        memset(&ack_pkt, 0, sizeof(packet_t));
        ack_pkt.ackno = expected_seqno;
        ack_pkt.len = 0;
        
        // El framework calcula el checksum automáticamente al usar SEND_PACKET 
        // en la mayoría de sus versiones internas.
        SEND_PACKET(&ack_pkt, ACK_PACKET_SIZE);
    }
}

void send_callback() {
    if (is_waiting_for_ack) {
        PAUSE_TRANSMISSION(); 
        return;
    }

    // Usar MAX_PAYLOAD como sugirió tu compilador
    int bytes_read = READ_DATA_FROM_APP_LAYER(last_packet->data, MAX_PAYLOAD);

    if (bytes_read > 0) {
        last_packet->seqno = current_seqno;
        last_packet->ackno = 0;
        last_packet->len = bytes_read;
        last_packet_len = bytes_read + DATA_PACKET_HEADER;

        // Enviar trama y fijar temporizador [cite: 186]
        SEND_PACKET(last_packet, last_packet_len);
        SET_TIMER(0, timeout_duration); 
        
        is_waiting_for_ack = 1; 
        PAUSE_TRANSMISSION(); 
    }
}

void timer_callback(int timer_number) {
    // Retransmisión por pérdida o error [cite: 44]
    if (timer_number == 0 && is_waiting_for_ack) {
        SEND_PACKET(last_packet, last_packet_len);
        SET_TIMER(0, timeout_duration);
    }
}