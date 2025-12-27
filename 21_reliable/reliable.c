#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rlib.h"

/* Sección 2: Variables Globales para mantener el estado del protocolo [cite: 133] */
static packet_t *last_packet;      /* Buffer para retransmisiones [cite: 39, 44] */
static size_t last_packet_size;    
static int waiting_for_ack;        /* Control de flujo: 1 si espera ACK, 0 si no [cite: 23] */
static uint32_t next_seqno;        /* Siguiente número de secuencia a enviar */
static uint32_t expected_seqno;    /* Número de secuencia que el receptor espera [cite: 61] */
static long timeout_val;           

/* Sección 3: Implementación de Callbacks [cite: 111, 134] */

void connection_initialization(int window_size, long timeout_in_ns) {
    last_packet = (packet_t *) malloc(sizeof(packet_t));
    memset(last_packet, 0, sizeof(packet_t));
    
    waiting_for_ack = 0;
    next_seqno = 1;      
    expected_seqno = 1; /* El receptor espera inicialmente la trama 1 [cite: 61] */
    timeout_val = timeout_in_ns; /* Valor por defecto de 10ms [cite: 138] */
}

void receive_callback(packet_t *pkt, size_t pkt_size) {
    /* 1. Validar integridad: si falla, el framework registra "corrupt" [cite: 221, 222] */
    if (!VALIDATE_CHECKSUM(pkt)) {
        return; 
    }

    /* 2. Lógica del Emisor: Recibimos un ACK (RR - Receiver Ready) [cite: 20, 61] */
    if (pkt->ackno > 0) {
        /* RR indica la SIGUIENTE trama que se espera recibir [cite: 61] */
        if (pkt->ackno > last_packet->seqno) {
            SET_TIMER(0, -1);           /* Detener temporizador [cite: 185, 186] */
            waiting_for_ack = 0;
            RESUME_TRANSMISSION();      /* Desbloquear aplicación [cite: 153] */
        }
    } 
    /* 3. Lógica del Receptor: Recibimos Datos [cite: 182] */
    else {
        if (pkt->seqno == expected_seqno) {
            ACCEPT_DATA(pkt->data, pkt->len); /* Pasar datos al nivel de aplicación [cite: 182, 193] */
            expected_seqno++; /* Actualizar la siguiente trama esperada [cite: 61] */
        }
        
        /* Enviar confirmación (ACK) [cite: 20, 197] */
        packet_t ack_pkt;
        memset(&ack_pkt, 0, sizeof(packet_t));
        ack_pkt.ackno = expected_seqno;
        ack_pkt.len = 8; /* Tamaño de cabecera de ACK */
        
        /* CÁLCULO DEL CHECKSUM: Fundamental para evitar el 100% corrupt  */
        ack_pkt.cksum = 0;
        ack_pkt.cksum = cksum(&ack_pkt, 8);
        
        SEND_PACKET(&ack_pkt, 8);
    }
}

void send_callback() {
    /* Si el protocolo no permite enviar más datos, pausar aplicación [cite: 23, 152] */
    if (waiting_for_ack) {
        PAUSE_TRANSMISSION(); 
        return;
    }

    /* Obtener datos usando MAX_PAYLOAD */
    int bytes_read = READ_DATA_FROM_APP_LAYER(last_packet->data, MAX_PAYLOAD);

    if (bytes_read > 0) {
        last_packet->seqno = next_seqno;
        last_packet->ackno = 0;
        last_packet->len = bytes_read;
        last_packet_size = bytes_read + 12; /* 12 bytes de cabecera de datos */

        /* CÁLCULO DEL CHECKSUM: El receptor descartará la trama si esto falta  */
        last_packet->cksum = 0;
        last_packet->cksum = cksum(last_packet, last_packet_size);

        SEND_PACKET(last_packet, last_packet_size);
        SET_TIMER(0, timeout_val); /* Activar temporizador de retransmisión [cite: 44, 183] */
        
        next_seqno++;
        waiting_for_ack = 1; 
        PAUSE_TRANSMISSION(); /* Parada y espera: no enviar más hasta recibir ACK [cite: 23, 152] */
    }
}

void timer_callback(int timer_number) {
    /* Si el temporizador de retransmisión expira, reenviar la trama [cite: 44, 183] */
    if (timer_number == 0 && waiting_for_ack) {
        SEND_PACKET(last_packet, last_packet_size);
        SET_TIMER(0, timeout_val); /* Reiniciar temporizador [cite: 44] */
    }
}