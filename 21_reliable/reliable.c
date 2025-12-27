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

/*
    Sección 2: Declaración de variables globales
    Las variables deben ser persistentes entre llamadas[cite: 133].
--------------------------------------------------------------------------------
*/
static packet_t *last_packet;      // Buffer para almacenar el paquete para retransmitir
static size_t last_packet_len;     // Tamaño del último paquete enviado
static int is_waiting_for_ack;     // Estado: 1 si esperamos ACK, 0 si no [cite: 152]
static uint32_t current_seqno;     // Número de secuencia actual del emisor
static uint32_t expected_seqno;    // Número de secuencia que espera el receptor
static long timeout_duration;      // Valor del timeout configurado [cite: 138]

/*
    Sección 3: Implementación de las callbacks [cite: 134]
--------------------------------------------------------------------------------
*/

void connection_initialization(int window_size, long timeout_in_ns)
{
    last_packet = (packet_t *) malloc(sizeof(packet_t));
    memset(last_packet, 0, sizeof(packet_t));

    is_waiting_for_ack = 0;
    current_seqno = 1;
    expected_seqno = 1;
    timeout_duration = timeout_in_ns; // Por defecto 10.000.000 ns [cite: 138]
}

void receive_callback(packet_t *pkt, size_t pkt_size)
{
    // 1. Validar integridad de la trama [cite: 221]
    if (!VALIDATE_CHECKSUM(pkt)) { // Usando solo el puntero pkt como pide tu VM
        return; 
    }

    // 2. Lógica como Emisor: Recibimos una confirmación (ACK)
    if (pkt->ackno > 0) {
        if (pkt->ackno > last_packet->seqno) {
            is_waiting_for_ack = 0;
            // Para detener el temporizador usamos un valor negativo según el estándar del framework [cite: 154]
            SET_TIMER(0, -1);           
            RESUME_TRANSMISSION();     // Permitimos que send_callback sea llamado de nuevo [cite: 153]
            current_seqno = pkt->ackno;
        }
    } 
    // 3. Lógica como Receptor: Recibimos datos
    else {
        // Aceptamos solo paquetes en orden [cite: 161]
        if (pkt->seqno == expected_seqno) {
            ACCEPT_DATA(pkt->data, pkt->len); // Entregamos datos a la aplicación [cite: 193]
            expected_seqno++;
        }
        
        // Enviamos siempre confirmación con la siguiente trama esperada [cite: 61, 198]
        packet_t ack_pkt;
        memset(&ack_pkt, 0, sizeof(packet_t));
        ack_pkt.ackno = expected_seqno;
        ack_pkt.seqno = 0;
        ack_pkt.len = 0;
        SEND_PACKET(&ack_pkt, ACK_PACKET_SIZE); // Función de envío de la API 
    }
}

void send_callback()
{
    // Si ya estamos esperando un ACK, pausamos [cite: 152]
    if (is_waiting_for_ack) {
        PAUSE_TRANSMISSION();
        return;
    }

    // Leer datos usando MAX_PAYLOAD definido en tu rlib.h
    int bytes_read = READ_DATA_FROM_APP_LAYER(last_packet->data, MAX_PAYLOAD);

    if (bytes_read > 0) {
        last_packet->seqno = current_seqno;
        last_packet->ackno = 0;
        last_packet->len = bytes_read;
        last_packet_len = bytes_read + DATA_PACKET_HEADER;

        // Enviamos la trama y activamos temporizador de retransmisión [cite: 154, 191]
        SEND_PACKET(last_packet, last_packet_len);
        SET_TIMER(0, timeout_duration); 
        
        is_waiting_for_ack = 1; 
        PAUSE_TRANSMISSION(); // Bloqueamos hasta recibir el ACK [cite: 152]
    }
}

void timer_callback(int timer_number)
{
    // Si el temporizador expira y no hay ACK, retransmitimos [cite: 44, 183]
    if (timer_number == 0 && is_waiting_for_ack) {
        SEND_PACKET(last_packet, last_packet_len);
        SET_TIMER(0, timeout_duration); // Reiniciamos el temporizador [cite: 154]
    }
}