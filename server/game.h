#ifndef GAME_H
#define GAME_H

// Motor del juego Pong: logica pura, sin sockets ni hilos.
// Coordenadas: (0,0) es la esquina superior izquierda, y crece hacia abajo.
// La pelota es un cuadrado; (pelota_x, pelota_y) es su esquina superior izquierda.
// paleta_y es el borde superior de cada paleta.

#define CAMPO_ANCHO    800
#define CAMPO_ALTO     600
#define PALETA_ANCHO   10
#define PALETA_ALTO    100
#define PALETA_MARGEN  20   // distancia de cada paleta a su borde lateral
#define PALETA_VEL     8    // pixeles por tick
#define PELOTA_TAM     10
#define PELOTA_VEL     5    // pixeles por tick en cada eje
#define TICK_US        16666  // ~60 ticks por segundo

// Borde x de la cara de cada paleta que mira hacia el centro
#define PALETA1_X_FRENTE (PALETA_MARGEN + PALETA_ANCHO)
#define PALETA2_X_FRENTE (CAMPO_ANCHO - PALETA_MARGEN - PALETA_ANCHO)

typedef struct {
    int pelota_x, pelota_y;
    int vel_x, vel_y;
    int paleta_y[2];    // [0] = jugador 1 (izquierda), [1] = jugador 2 (derecha)
    int paleta_dir[2];  // -1 = arriba, 0 = quieto, 1 = abajo (lo fija RF-10)
    int marcador[2];
} EstadoJuego;

// Pone la pelota en el centro y la lanza hacia 'dir_x' (-1 izquierda, 1 derecha)
static inline void juego_saque(EstadoJuego *j, int dir_x) {
    j->pelota_x = (CAMPO_ANCHO - PELOTA_TAM) / 2;
    j->pelota_y = (CAMPO_ALTO - PELOTA_TAM) / 2;
    j->vel_x = dir_x * PELOTA_VEL;
    j->vel_y = PELOTA_VEL;
}

static inline void juego_iniciar(EstadoJuego *j) {
    for (int i = 0; i < 2; i++) {
        j->paleta_y[i] = (CAMPO_ALTO - PALETA_ALTO) / 2;
        j->paleta_dir[i] = 0;
        j->marcador[i] = 0;
    }
    juego_saque(j, 1);
}

// 1 si la pelota se cruza verticalmente con la paleta del jugador i
static inline int pelota_toca_paleta(EstadoJuego *j, int i) {
    return j->pelota_y + PELOTA_TAM >= j->paleta_y[i] &&
           j->pelota_y <= j->paleta_y[i] + PALETA_ALTO;
}

// Avanza el juego un tick.
// Devuelve 0 si no hubo punto, 1 o 2 si anoto ese jugador.
static inline int juego_tick(EstadoJuego *j) {
    // Paletas: se mueven segun su direccion y no salen del campo
    for (int i = 0; i < 2; i++) {
        j->paleta_y[i] += j->paleta_dir[i] * PALETA_VEL;
        if (j->paleta_y[i] < 0) j->paleta_y[i] = 0;
        if (j->paleta_y[i] > CAMPO_ALTO - PALETA_ALTO) j->paleta_y[i] = CAMPO_ALTO - PALETA_ALTO;
    }

    j->pelota_x += j->vel_x;
    j->pelota_y += j->vel_y;

    // Rebote en bordes superior e inferior
    if (j->pelota_y <= 0) {
        j->pelota_y = 0;
        j->vel_y = -j->vel_y;
    } else if (j->pelota_y + PELOTA_TAM >= CAMPO_ALTO) {
        j->pelota_y = CAMPO_ALTO - PELOTA_TAM;
        j->vel_y = -j->vel_y;
    }

    // Rebote en paleta 1 (izquierda): solo si va hacia ella y aun no la paso
    if (j->vel_x < 0 &&
        j->pelota_x <= PALETA1_X_FRENTE &&
        j->pelota_x + PELOTA_TAM >= PALETA_MARGEN &&
        pelota_toca_paleta(j, 0)) {
        j->pelota_x = PALETA1_X_FRENTE;
        j->vel_x = -j->vel_x;
    }

    // Rebote en paleta 2 (derecha)
    if (j->vel_x > 0 &&
        j->pelota_x + PELOTA_TAM >= PALETA2_X_FRENTE &&
        j->pelota_x <= CAMPO_ANCHO - PALETA_MARGEN &&
        pelota_toca_paleta(j, 1)) {
        j->pelota_x = PALETA2_X_FRENTE - PELOTA_TAM;
        j->vel_x = -j->vel_x;
    }

    // Punto: la pelota salio por un lado. Se saca hacia quien recibio el punto.
    if (j->pelota_x + PELOTA_TAM < 0) {
        j->marcador[1]++;
        juego_saque(j, -1);
        return 2;
    }
    if (j->pelota_x > CAMPO_ANCHO) {
        j->marcador[0]++;
        juego_saque(j, 1);
        return 1;
    }
    return 0;
}

#endif
