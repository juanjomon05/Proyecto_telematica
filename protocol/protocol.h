#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <sys/socket.h>
#include <string.h>
#include <stdint.h>

#define PAYLOAD_SIZE 64

// Todas las estructuras van "packed": sin relleno del compilador, de modo que
// lo que viaja por la red es exactamente lo que especifica el README.

// Tipos de mensaje
typedef enum {
    REGISTER_REQ  = 0x01,
    REGISTER_OK   = 0x02,
    MOVE_REQ      = 0x03,
    STATE_UPDATE  = 0x04,
    GAME_OVER     = 0x05,
    GAME_START    = 0x06,
    MSG_ERROR     = 0xFF
} MessageType;

// Codigos de MSG_ERROR
typedef enum {
    ERR_SERVIDOR_LLENO         = 1,
    ERR_OPONENTE_DESCONECTADO  = 2,
    ERR_DATOS_INVALIDOS        = 3,  // nickname vacio o email sin '@'
    ERR_NICK_EN_USO            = 4
} ErrorCode;

// Mensaje generico (66 bytes fijos: 1 + 1 + 64)
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t seq;
    uint8_t payload[PAYLOAD_SIZE];
} Message;

// Payload de REGISTER_REQ (60 bytes)
typedef struct __attribute__((packed)) {
    char nickname[20];
    char email[40];
} RegisterPayload;

// Payload de REGISTER_OK (1 byte)
typedef struct __attribute__((packed)) {
    uint8_t player_id;
} RegisterOkPayload;

// Payload de MOVE_REQ (1 byte)
typedef struct __attribute__((packed)) {
    uint8_t direction; // 0=arriba, 1=abajo, 2=quieto
} MovePayload;

// Payload de STATE_UPDATE (18 bytes). Los int32 viajan en orden de red (big-endian).
typedef struct __attribute__((packed)) {
    int32_t ball_x;
    int32_t ball_y;
    int32_t paddle1_y;
    int32_t paddle2_y;
    uint8_t score1;
    uint8_t score2;
} StatePayload;

// Payload de GAME_OVER (1 byte)
typedef struct __attribute__((packed)) {
    uint8_t winner_id;  // numero de jugador ganador: 1 o 2
} GameOverPayload;

// Payload de MSG_ERROR (1 byte)
typedef struct __attribute__((packed)) {
    uint8_t error_code;
} ErrorPayload;

// Payload de GAME_START (21 bytes)
typedef struct __attribute__((packed)) {
    uint8_t slot;            // 1 o 2: que jugador eres
    char opponent_nick[20];  // nickname del rival
} GameStartPayload;

// Si alguno de estos tamanos cambia, el protocolo dejo de coincidir con la especificacion
_Static_assert(sizeof(Message) == 66, "Message debe medir 66 bytes");
_Static_assert(sizeof(RegisterPayload) == 60, "RegisterPayload debe medir 60 bytes");
_Static_assert(sizeof(StatePayload) == 18, "StatePayload debe medir 18 bytes");
_Static_assert(sizeof(GameStartPayload) == 21, "GameStartPayload debe medir 21 bytes");

// Envía un Message completo (66 bytes fijos) por el socket
static inline int send_message(int sock, Message *msg) {
    return send(sock, msg, sizeof(Message), 0);
}

// Recibe un Message completo (66 bytes fijos) del socket.
// MSG_WAITALL: TCP es un flujo y puede entregar un mensaje partido; asi recv espera los 66 bytes.
static inline int recv_message(int sock, Message *msg) {
    return recv(sock, msg, sizeof(Message), MSG_WAITALL);
}

// Arma un Message vacío con el tipo dado, listo para llenar payload
static inline void init_message(Message *msg, uint8_t type, uint8_t seq) {
    memset(msg, 0, sizeof(Message));
    msg->type = type;
    msg->seq = seq;
}
#endif
