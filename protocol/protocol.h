#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <sys/socket.h>
#include <string.h>
#include <stdint.h>

#define PAYLOAD_SIZE 64

// Tipos de mensaje
typedef enum {
    REGISTER_REQ  = 0x01,
    REGISTER_OK   = 0x02,
    MOVE_REQ      = 0x03,
    STATE_UPDATE  = 0x04,
    GAME_OVER     = 0x05,
    MSG_ERROR     = 0xFF
} MessageType;

// Mensaje genérico (66 bytes fijos: 1 + 1 + 64)
typedef struct {
    uint8_t type;
    uint8_t seq;
    uint8_t payload[PAYLOAD_SIZE];
} Message;

// Payload de REGISTER_REQ
typedef struct {
    char nickname[20];
    char email[40];
} RegisterPayload;

// Payload de REGISTER_OK
typedef struct {
    uint8_t player_id;
} RegisterOkPayload;

// Payload de MOVE_REQ
typedef struct {
    uint8_t direction; // 0=arriba, 1=abajo, 2=quieto
} MovePayload;

// Payload de STATE_UPDATE
typedef struct {
    int32_t ball_x;
    int32_t ball_y;
    int32_t paddle1_y;
    int32_t paddle2_y;
    uint8_t score1;
    uint8_t score2;
} StatePayload;

// Payload de GAME_OVER
typedef struct {
    uint8_t winner_id;
} GameOverPayload;

// Payload de ERROR
typedef struct {
    uint8_t error_code;
} ErrorPayload;

#endif

// Envía un Message completo (66 bytes fijos) por el socket
static inline int send_message(int sock, Message *msg) {
    return send(sock, msg, sizeof(Message), 0);
}

// Recibe un Message completo (66 bytes fijos) del socket
static inline int recv_message(int sock, Message *msg) {
    return recv(sock, msg, sizeof(Message), 0);
}

// Arma un Message vacío con el tipo dado, listo para llenar payload
static inline void init_message(Message *msg, uint8_t type, uint8_t seq) {
    memset(msg, 0, sizeof(Message));
    msg->type = type;
    msg->seq = seq;
}