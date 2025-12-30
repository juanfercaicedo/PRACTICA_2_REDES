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
static char last_payload[MAX_PAYLOAD]; 
static int last_payload_size;          
static int waiting_for_ack;            
static uint32_t next_seqno;            
static uint32_t expected_seqno;        
static long timeout_val;               

/* Sección 3: Implementación de Callbacks */

void connection_initialization(int window_size, long timeout_in_ns) {
    waiting_for_ack = 0;
    next_seqno = 1;      /* El primer paquete debe tener seqno 1 */
    expected_seqno = 1; 
    timeout_val = timeout_in_ns; 
    last_payload_size = 0;
}

void receive_callback(packet_t *pkt, size_t pkt_size) {
    /* 1. Validar integridad usando la API oficial [cite: 139, 221] */
    if (VALIDATE_CHECKSUM(pkt) == 0) {
        return; /* Si falla, se ignora por completo [cite: 43] */
    }

    /* 2. Lógica del Emisor: Recibimos un ACK [cite: 20] */
    if (IS_ACK_PACKET(pkt)) {
        /* En este framework, el ACK indica la SIGUIENTE trama esperada [cite: 61] */
        if (waiting_for_ack && pkt->ackno > (next_seqno - 1)) {
            CLEAR_TIMER(0);           /* Detener retransmisión [cite: 186] */
            waiting_for_ack = 0;
            RESUME_TRANSMISSION();    /* Permitir nuevos datos de la App [cite: 153] */
        }
    } 
    /* 3. Lógica del Receptor: Recibimos Datos [cite: 148] */
    else {
        if (pkt->seqno == expected_seqno) {
            /* Aceptar solo el payload (tamaño total - cabecera de 12 bytes) [cite: 132] */
            ACCEPT_DATA(pkt->data, pkt->len - DATA_PACKET_HEADER); 
            expected_seqno++; 
        }
        
        /* Siempre enviar ACK indicando qué esperamos a continuación [cite: 24, 61] */
        SEND_ACK_PACKET(expected_seqno);
    }
}

void send_callback() {
    /* Implementación de Parada y Espera: No enviar si hay uno pendiente  */
    if (waiting_for_ack) {
        PAUSE_TRANSMISSION(); 
        return;
    }

    char buffer[MAX_PAYLOAD];
    int bytes_read = READ_DATA_FROM_APP_LAYER(buffer, MAX_PAYLOAD);

    if (bytes_read > 0) {
        /* Guardar copia para retransmisión en caso de pérdida [cite: 39] */
        memcpy(last_payload, buffer, bytes_read);
        last_payload_size = bytes_read;

        /* Enviar usando la API: longitud = datos + 12 bytes de cabecera */
        SEND_DATA_PACKET(bytes_read + DATA_PACKET_HEADER, 0, next_seqno, last_payload);
        
        /* Iniciar temporizador para retransmisión (Timer 0) [cite: 154, 186] */
        SET_TIMER(0, timeout_val); 
        
        next_seqno++;
        waiting_for_ack = 1; 
        PAUSE_TRANSMISSION(); 
    }
}

void timer_callback(int timer_number) {
    /* Retransmitir si el temporizador expira sin recibir ACK [cite: 44, 183] */
    if (timer_number == 0 && waiting_for_ack) {
        SEND_DATA_PACKET(last_payload_size + DATA_PACKET_HEADER, 0, next_seqno - 1, last_payload);
        SET_TIMER(0, timeout_val); 
    }
}