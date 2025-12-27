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

/* SECCIÓN 2: Variables Globales */
static packet_t *last_sent_packet;
static size_t last_packet_full_size;
static int waiting_for_ack;
static uint32_t next_seq_to_send;
static uint32_t expected_seq_num;
static long timeout_ns;

/* SECCIÓN 3: Implementación de las Callbacks */

void connection_initialization(int window_size, long timeout_in_ns) {
    last_sent_packet = (packet_t *) malloc(sizeof(packet_t));
    memset(last_sent_packet, 0, sizeof(packet_t));
    
    waiting_for_ack = 0;
    next_seq_to_send = 1;      
    expected_seq_num = 1;      
    timeout_ns = timeout_in_ns; 
}

void receive_callback(packet_t *pkt, size_t pkt_size) {
    /* 1. Validar integridad: esto soluciona el "100.0% corrupt" */
    if (!VALIDATE_CHECKSUM(pkt)) {
        return; 
    }

    /* 2. Lógica del Emisor: Recibimos un ACK */
    if (pkt->ackno > 0) {
        if (pkt->ackno >= next_seq_to_send) {
            SET_TIMER(0, -1);           /* Detener temporizador */
            waiting_for_ack = 0;
            RESUME_TRANSMISSION();      /* Desbloquear aplicación */
        }
    } 
    /* 3. Lógica del Receptor: Recibimos datos */
    else {
        if (pkt->seqno == expected_seq_num) {
            ACCEPT_DATA(pkt->data, pkt->len); /* Entregar datos a la aplicación */
            expected_seq_num++;
        }
        
        /* Enviar confirmación (ACK) */
        packet_t ack_pkt;
        memset(&ack_pkt, 0, sizeof(packet_t));
        ack_pkt.ackno = expected_seq_num; 
        ack_pkt.len = 0;
        
        /* IMPORTANTE: Calcular checksum antes de enviar */
        rel_recompute_checksum(&ack_pkt); 
        
        /* Usar la función de envío que ya te funcionó en el make anterior */
        rel_sendpkt(0, &ack_pkt, ACK_PACKET_SIZE);
    }
}

void send_callback() {
    if (waiting_for_ack) {
        PAUSE_TRANSMISSION(); 
        return;
    }

    /* Usamos MAX_PAYLOAD que es la constante que reconoció tu compilador */
    int bytes_read = READ_DATA_FROM_APP_LAYER(last_sent_packet->data, MAX_PAYLOAD);

    if (bytes_read > 0) {
        last_sent_packet->seqno = next_seq_to_send;
        last_sent_packet->ackno = 0;
        last_sent_packet->len = bytes_read;
        last_packet_full_size = bytes_read + DATA_PACKET_HEADER;

        /* Calcular checksum para que el receptor no lo vea como corrupto */
        rel_recompute_checksum(last_sent_packet);

        rel_sendpkt(0, last_sent_packet, last_packet_full_size);
        SET_TIMER(0, timeout_ns); 
        
        next_seq_to_send++;
        waiting_for_ack = 1; 
        PAUSE_TRANSMISSION(); 
    }
}

void timer_callback(int timer_number) {
    if (timer_number == 0 && waiting_for_ack) {
        rel_sendpkt(0, last_sent_packet, last_packet_full_size);
        SET_TIMER(0, timeout_ns);
    }
}