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
static size_t last_packet_full_size; 
static int waiting_for_ack;        
static uint32_t next_seqno;        
static uint32_t expected_seqno;    
static long timeout_val;           

/* Sección 3: Implementación de Callbacks */

void connection_initialization(int window_size, long timeout_in_ns) {
    last_packet = (packet_t *) malloc(sizeof(packet_t));
    memset(last_packet, 0, sizeof(packet_t));
    
    waiting_for_ack = 0;
    next_seqno = 1;      
    expected_seqno = 1; 
    timeout_val = timeout_in_ns; 
}

void receive_callback(packet_t *pkt, size_t pkt_size) {
    /* 1. Validar integridad: Si esto falla, verás "corrupt" en las estadísticas */
    if (!VALIDATE_CHECKSUM(pkt)) {
        return; 
    }

    /* 2. Lógica del Emisor: Recibimos un ACK */
    if (pkt->ackno > 0) {
        /* En el framework, las confirmaciones indican la siguiente trama esperada [cite: 61] */
        if (pkt->ackno > last_packet->seqno) {
            SET_TIMER(0, -1);           /* Detener temporizador */
            waiting_for_ack = 0;
            RESUME_TRANSMISSION();      /* Desbloquear aplicación [cite: 153] */
        }
    } 
    /* 3. Lógica del Receptor: Recibimos Datos */
    else {
        if (pkt->seqno == expected_seqno) {
            ACCEPT_DATA(pkt->data, pkt->len); 
            expected_seqno++; 
        }
        
        /* Generar y enviar confirmación (ACK) */
        packet_t ack_pkt;
        memset(&ack_pkt, 0, sizeof(packet_t));
        ack_pkt.ackno = expected_seqno;
        ack_pkt.seqno = 0;
        ack_pkt.len = 0; /* Un ACK no tiene carga útil (payload) */
        
        /* CÁLCULO DE INTEGRIDAD: El tamaño debe ser exactamente ACK_PACKET_SIZE (12 bytes) */
        ack_pkt.cksum = 0;
        ack_pkt.cksum = cksum(&ack_pkt, ACK_PACKET_SIZE);
        
        SEND_PACKET(&ack_pkt, ACK_PACKET_SIZE);
    }
}

void send_callback() {
    if (waiting_for_ack) {
        PAUSE_TRANSMISSION(); 
        return;
    }

    /* Usamos MAX_PAYLOAD para leer los datos de la aplicación [cite: 150] */
    int bytes_read = READ_DATA_FROM_APP_LAYER(last_packet->data, MAX_PAYLOAD);

    if (bytes_read > 0) {
        last_packet->seqno = next_seqno;
        last_packet->ackno = 0;
        last_packet->len = bytes_read;
        
        /* El tamaño total es el payload + la cabecera (12 bytes) [cite: 132] */
        last_packet_full_size = bytes_read + DATA_PACKET_HEADER;

        /* CÁLCULO DE INTEGRIDAD: cksum debe ser 0 antes de calcularlo sobre TODO el paquete */
        last_packet->cksum = 0;
        last_packet->cksum = cksum(last_packet, last_packet_full_size);

        SEND_PACKET(last_packet, last_packet_full_size);
        SET_TIMER(0, timeout_val); 
        
        next_seqno++;
        waiting_for_ack = 1; 
        PAUSE_TRANSMISSION(); /* Parada y espera [cite: 23] */
    }
}

void timer_callback(int timer_number) {
    /* Retransmisión por pérdida o error [cite: 44] */
    if (timer_number == 0 && waiting_for_ack) {
        SEND_PACKET(last_packet, last_packet_full_size);
        SET_TIMER(0, timeout_val); 
    }
}