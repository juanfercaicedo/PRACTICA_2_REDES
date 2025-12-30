#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "rlib.h"

static struct { char data[MAX_PAYLOAD]; int len; } window_buffer[16];
static uint32_t base_seq; 
static uint32_t next_seq; 
static uint32_t expected_seq;
static int w_size;
static long t_val;

void connection_initialization(int window_size, long timeout_in_ns) {
    base_seq = 1; 
    next_seq = 1; 
    expected_seq = 1;
    w_size = window_size; 
    t_val = timeout_in_ns;
    memset(window_buffer, 0, sizeof(window_buffer));
}

void receive_callback(packet_t *pkt, size_t pkt_size) {
    if (VALIDATE_CHECKSUM(pkt) == 0) return; [cite: 221]

    if (IS_ACK_PACKET(pkt)) {
        if (pkt->ackno > base_seq) {
            for (uint32_t i = base_seq; i < pkt->ackno; i++) {
                CLEAR_TIMER(i % 16);
            }
            base_seq = pkt->ackno;
            RESUME_TRANSMISSION(); 
        }
    } else {
        if (pkt->seqno == expected_seq) {
            ACCEPT_DATA(pkt->data, pkt->len - DATA_PACKET_HEADER); [cite: 139]
            expected_seq++;
        }
        SEND_ACK_PACKET(expected_seq); 
    }
}

void send_callback() {
    while (next_seq < base_seq + w_size) {
        char buf[MAX_PAYLOAD];
        int r = READ_DATA_FROM_APP_LAYER(buf, MAX_PAYLOAD); [cite: 150]
        
        if (r <= 0) break;

        int idx = next_seq % 16;
        memcpy(window_buffer[idx].data, buf, r);
        window_buffer[idx].len = r;

        SEND_DATA_PACKET(r + DATA_PACKET_HEADER, 0, next_seq, window_buffer[idx].data); [cite: 122]
        SET_TIMER(idx, t_val); [cite: 154]
        next_seq++;
    }

    if (next_seq == base_seq + w_size) {
        PAUSE_TRANSMISSION(); [cite: 152]
    }
}

void timer_callback(int timer_number) {
    for (uint32_t i = base_seq; i < next_seq; i++) {
        int idx = i % 16;
        SEND_DATA_PACKET(window_buffer[idx].len + DATA_PACKET_HEADER, 0, i, window_buffer[idx].data);
        SET_TIMER(idx, t_val); 
    }
}