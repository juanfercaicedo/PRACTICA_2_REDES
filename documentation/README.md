```mermaid
participant "Maquina 1" as MaquinaVirtual1
participant "Emisor" as Emisor
participant "Red" as Red
participant "Receptor" as Receptor
participant "Maquina 2" as MaquinaVirtual2

== Fase de Inicialización ==
note over Emisor, Receptor: Se ejecuta connection_initialization()\nVariables: next_seqno=1, expected_seqno=1

== Ciclo de Transmisión ==

MaquinaVirtual1 -> Emisor: send_callback()
activate Emisor
    Emisor -> Emisor: READ_DATA_FROM_APP_LAYER()
    Emisor -> Emisor: Copiar datos a last_payload
    Emisor -> Red: SEND_DATA_PACKET(seqno=1, len+12)
    Emisor -> Emisor: SET_TIMER(0, timeout)
    Emisor -> Emisor: **waiting_for_ack = 1**
    Emisor -> Emisor: **PAUSE_TRANSMISSION()**
deactivate Emisor

Red -> Receptor: receive_callback()
activate Receptor
    Receptor -> Receptor: VALIDATE_CHECKSUM()
    Receptor -> Receptor: pkt->seqno == expected_seqno (1 == 1?)
    Receptor -> MaquinaVirtual2: ACCEPT_DATA(payload)
    Receptor -> Receptor: **expected_seqno = 2**
    Receptor -> Red: SEND_ACK_PACKET(ackno=2)
deactivate Receptor

Red -> Emisor: receive_callback() (ACK)
activate Emisor
    Emisor -> Emisor: VALIDATE_CHECKSUM()
    Emisor -> Emisor: IS_ACK_PACKET? (Si)
    Emisor -> Emisor: pkt->ackno > (1)? (Si, es 2)
    Emisor -> Emisor: **CLEAR_TIMER(0)**
    Emisor -> Emisor: **waiting_for_ack = 0**
    Emisor -> Emisor: **RESUME_TRANSMISSION()**
deactivate Emisor

== Pérdida o Corrupción ==

MaquinaVirtual1 -> Emisor: send_callback()
activate Emisor
    Emisor -> Red: SEND_DATA_PACKET(seqno=2)
    Emisor -> Emisor: SET_TIMER(0)
deactivate Emisor

Red -x Red: <font color=red>Trama Perdida o Corrupta</font>

note over Emisor: Transcurre tiempo 'timeout_val'

Emisor -> Emisor: **timer_callback(0)**
activate Emisor
    Emisor -> Red: SEND_DATA_PACKET(seqno=2, last_payload)
    Emisor -> Emisor: SET_TIMER(0) (Reinicia temporizador)
deactivate Emisor
```
