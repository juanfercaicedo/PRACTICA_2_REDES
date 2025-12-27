#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rlib.h"

/* SECCIÓN 2: Variables Globales */
static packet_t *last_sent_packet;
static size_t last_packet_full_size;
static int waiting_for_ack;
static uint32_t next_seq_to_send;
static uint32_t expected_seq_num;
static long timeout_ns;

/* SECCIÓN 3: Implementación de Callbacks */

void connection_initialization(int window_size, long timeout_in_ns) {
    // Reservamos memoria para el buffer de retransmisión
    last_sent_packet = (packet_t *) malloc(sizeof(packet_t));
    memset(last_sent_packet, 0, sizeof(packet_t));
    
    waiting_for_ack = 0;
    next_seq_to_send = 1;      // Los números de secuencia empiezan en 1 [cite: 192]
    expected_seq_num = 1;      // Esperamos el primer paquete del otro lado
    timeout_ns = timeout_in_ns; // Por defecto 10ms [cite: 138]
}

void receive_callback(packet_t *pkt, size_t pkt_size) {
    // 1. Validar integridad: Si esto falla, verás "corrupt" en las estadísticas 
    if (!VALIDATE_CHECKSUM(pkt)) {
        return; 
    }

    // 2. Lógica del Emisor: Recibimos una confirmación (ACK) [cite: 20, 198]
    if (pkt->ackno > 0) {
        if (pkt->ackno >= next_seq_to_send) {
            SET_TIMER(0, -1);           // Detener el temporizador de retransmisión [cite: 154]
            waiting_for_ack = 0;
            RESUME_TRANSMISSION();      // Desbloquear para permitir más envíos [cite: 153]
        }
    } 
    // 3. Lógica del Receptor: Recibimos datos [cite: 192]
    else {
        if (pkt->seqno == expected_seq_num) {
            ACCEPT_DATA(pkt->data, pkt->len); // Entregar datos a la aplicación [cite: 193]
            expected_seq_num++;
        }
        
        // Enviar confirmación (ACK) indicando qué paquete esperamos ahora [cite: 198]
        packet_t ack_pkt;
        memset(&ack_pkt, 0, sizeof(packet_t));
        ack_pkt.ackno = expected_seqno; 
        ack_pkt.len = 0;
        
        // Calculamos el checksum antes de enviar para evitar el "100% corrupt"
        rel_recompute_checksum(&ack_pkt); 
        
        SEND_PACKET(&ack_pkt, ACK_PACKET_SIZE);
    }
}

void send_callback() {
    // Si ya estamos esperando un ACK, no enviamos el siguiente [cite: 152]
    if (waiting_for_ack) {
        PAUSE_TRANSMISSION(); 
        return;
    }

    // Usamos MAX_PAYLOAD que es la constante que ya te reconoció el make
    int bytes_read = READ_DATA_FROM_APP_LAYER(last_sent_packet->data, MAX_PAYLOAD); [cite: 150]

    if (bytes_read > 0) {
        last_sent_packet->seqno = next_seq_to_send;
        last_sent_packet->ackno = 0;
        last_sent_packet->len = bytes_read;
        last_packet_full_size = bytes_read + DATA_PACKET_HEADER;

        // Calculamos el checksum antes de enviar para que el otro lado lo acepte
        rel_recompute_checksum(last_sent_packet);

        SEND_PACKET(last_sent_packet, last_packet_full_size);
        SET_TIMER(0, timeout_ns); // Activar temporizador de retransmisión [cite: 154]
        
        next_seq_to_send++;
        waiting_for_ack = 1; 
        PAUSE_TRANSMISSION(); // Bloquear hasta recibir el ACK [cite: 152]
    }
}

void timer_callback(int timer_number) {
    // Si expira el tiempo, retransmitimos la trama guardada [cite: 44, 183]
    if (timer_number == 0 && waiting_for_ack) {
        SEND_PACKET(last_sent_packet, last_packet_full_size);
        SET_TIMER(0, timeout_ns);
    }
}