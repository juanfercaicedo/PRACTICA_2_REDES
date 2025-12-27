#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rlib.h"

/* Sección 2: Variables Globales */
static packet_t *last_packet;      
static size_t last_packet_len;     
static int is_waiting_for_ack;     
static uint32_t current_seqno;     
static uint32_t expected_seqno;    
static long timeout_duration;      

/* Sección 3: Callbacks */

void connection_initialization(int window_size, long timeout_in_ns) {
    last_packet = (packet_t *) malloc(sizeof(packet_t));
    memset(last_packet, 0, sizeof(packet_t));

    is_waiting_for_ack = 0;
    current_seqno = 1;      // Empezamos en 1 [cite: 192]
    expected_seqno = 1;
    timeout_duration = timeout_in_ns; 
}

void receive_callback(packet_t *pkt, size_t pkt_size) {
    // 1. Validar integridad: Si falla, el paquete es "corrupt" 
    if (!VALIDATE_CHECKSUM(pkt)) {
        return; 
    }

    // 2. Lógica del Emisor: Recibimos un ACK (RR) [cite: 61]
    if (pkt->ackno > 0) {
        // RR indica la SIGUIENTE trama que espera el receptor [cite: 61]
        if (pkt->ackno > last_packet->seqno) {
            SET_TIMER(0, -1);           // Detener timer
            is_waiting_for_ack = 0;
            current_seqno = pkt->ackno; // Actualizar secuencia
            RESUME_TRANSMISSION();      // [cite: 153]
        }
    } 
    // 3. Lógica del Receptor: Recibimos datos
    else {
        if (pkt->seqno == expected_seqno) {
            ACCEPT_DATA(pkt->data, pkt->len); // [cite: 193]
            expected_seqno++;
        }
        
        // Enviar confirmación (ACK) siempre con el checksum calculado
        packet_t ack_pkt;
        memset(&ack_pkt, 0, sizeof(packet_t));
        ack_pkt.ackno = expected_seqno;
        
        // IMPORTANTE: Calcular checksum antes de enviar
        RECOMPUTE_CHECKSUM(&ack_pkt); 
        
        SEND_PACKET(&ack_pkt, ACK_PACKET_SIZE);
    }
}

void send_callback() {
    if (is_waiting_for_ack) {
        PAUSE_TRANSMISSION(); // [cite: 152]
        return;
    }

    // Usar MAX_PAYLOAD como sugirió tu compilador
    int bytes_read = READ_DATA_FROM_APP_LAYER(last_packet->data, MAX_PAYLOAD);

    if (bytes_read > 0) {
        last_packet->seqno = current_seqno;
        last_packet->ackno = 0;
        last_packet->len = bytes_read;
        last_packet_len = bytes_read + DATA_PACKET_HEADER;

        // IMPORTANTE: Calcular checksum antes de enviar 
        RECOMPUTE_CHECKSUM(last_packet);

        SEND_PACKET(last_packet, last_packet_len);
        SET_TIMER(0, timeout_duration); // [cite: 154, 186]
        
        is_waiting_for_ack = 1; 
        PAUSE_TRANSMISSION(); // [cite: 152]
    }
}

void timer_callback(int timer_number) {
    if (timer_number == 0 && is_waiting_for_ack) {
        // Retransmitir si el timer expira [cite: 44, 183]
        SEND_PACKET(last_packet, last_packet_len);
        SET_TIMER(0, timeout_duration);
    }
}