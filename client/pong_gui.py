"""Cliente grafico de Pong (pygame). Habla el mismo protocolo binario que pong_client.c.

Uso: python pong_gui.py <SERVER_IP> <PORT> <nickname> <email>
Controles: W / flecha arriba, S / flecha abajo. Al soltar la tecla la paleta se detiene.
"""
import socket
import struct
import sys
import threading

import pygame

# Tipos de mensaje (protocol.h)
REGISTER_REQ = 0x01
REGISTER_OK = 0x02
MOVE_REQ = 0x03
STATE_UPDATE = 0x04
GAME_OVER = 0x05
GAME_START = 0x06
MSG_ERROR = 0xFF

# Codigos de MSG_ERROR (protocol.h)
ERRORES = {
    1: "el servidor esta lleno",
    2: "el oponente se desconecto",
    3: "nickname vacio o email invalido",
    4: "ese nickname ya esta en uso",
}

MSG_SIZE = 66  # type(1) + seq(1) + payload(64)

# Direcciones de MOVE_REQ (protocol.h)
ARRIBA, ABAJO, QUIETO = 0, 1, 2

# Medidas del campo (server/game.h)
CAMPO_ANCHO, CAMPO_ALTO = 800, 600
PALETA_ANCHO, PALETA_ALTO, PALETA_MARGEN = 10, 100, 20
PELOTA_TAM = 10


def empaquetar(tipo, seq, payload=b""):
    return struct.pack("BB64s", tipo, seq, payload)


def recibir_mensaje(sock):
    """Lee exactamente 66 bytes (TCP puede entregarlos partidos). None si se cerro."""
    datos = b""
    while len(datos) < MSG_SIZE:
        try:
            trozo = sock.recv(MSG_SIZE - len(datos))
        except OSError:  # el socket se cerro (por ejemplo, al cerrar la ventana)
            return None
        if not trozo:
            return None
        datos += trozo
    return datos[0], datos[2:]  # tipo, payload


class Estado:
    """Lo que el hilo de red recibe y la ventana dibuja."""
    def __init__(self):
        self.lock = threading.Lock()
        self.texto = "Conectando..."
        self.slot = 0
        self.rival = ""
        self.pelota = (CAMPO_ANCHO // 2, CAMPO_ALTO // 2)
        self.paletas = [(CAMPO_ALTO - PALETA_ALTO) // 2] * 2
        self.score = (0, 0)
        self.jugando = False
        self.terminado = False


def hilo_red(sock, estado):
    """Recibe GAME_START, STATE_UPDATE y ERROR del servidor y actualiza el estado."""
    while True:
        msg = recibir_mensaje(sock)
        with estado.lock:
            if msg is None:
                estado.texto = "El servidor cerro la conexion"
                estado.jugando = False
                estado.terminado = True
                return
            tipo, payload = msg
            if tipo == GAME_START:
                slot, rival = struct.unpack_from("B20s", payload)
                estado.slot = slot
                estado.rival = rival.split(b"\0")[0].decode(errors="replace")
                estado.jugando = True
            elif tipo == STATE_UPDATE:
                # StatePayload: 4 int32 en orden de red + 2 uint8
                bx, by, p1, p2, s1, s2 = struct.unpack_from("!iiiiBB", payload)
                estado.pelota = (bx, by)
                estado.paletas = [p1, p2]
                estado.score = (s1, s2)
            elif tipo == GAME_OVER:
                ganador = payload[0]
                estado.texto = ("Ganaste!" if ganador == estado.slot
                                else f"Gano el jugador {ganador}")
                estado.jugando = False
                estado.terminado = True
                return
            elif tipo == MSG_ERROR:
                codigo = payload[0]
                estado.texto = ERRORES.get(codigo, f"ERROR del servidor, codigo={codigo}")
                estado.jugando = False
                estado.terminado = True
                return


def main():
    if len(sys.argv) < 5:
        print(f"Uso: python {sys.argv[0]} <SERVER_IP> <PORT> <nickname> <email>")
        sys.exit(1)

    ip, puerto, nick, email = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((ip, puerto))

    # Registro: RegisterPayload = nickname[20] + email[40]
    reg = struct.pack("20s40s", nick.encode()[:19], email.encode()[:39])
    sock.sendall(empaquetar(REGISTER_REQ, 1, reg))
    msg = recibir_mensaje(sock)
    if msg is None or msg[0] != REGISTER_OK:
        motivo = ERRORES.get(msg[1][0], "") if msg and msg[0] == MSG_ERROR else ""
        print("Registro fallido" + (f": {motivo}" if motivo else ""))
        sock.close()
        sys.exit(1)

    estado = Estado()
    estado.texto = f"Registrado como {nick} (player_id={msg[1][0]}). Esperando pareja..."
    threading.Thread(target=hilo_red, args=(sock, estado), daemon=True).start()

    pygame.init()
    pantalla = pygame.display.set_mode((CAMPO_ANCHO, CAMPO_ALTO))
    pygame.display.set_caption(f"Pong - {nick}")
    fuente = pygame.font.SysFont(None, 36)
    reloj = pygame.time.Clock()

    ultima_dir = QUIETO
    corriendo = True
    while corriendo:
        for evento in pygame.event.get():
            if evento.type == pygame.QUIT:
                corriendo = False

        with estado.lock:
            jugando = estado.jugando
            terminado = estado.terminado
            texto = estado.texto
            slot, rival = estado.slot, estado.rival
            pelota, paletas, score = estado.pelota, list(estado.paletas), estado.score

        # Solo se envia MOVE_REQ cuando cambia la direccion, no en cada frame
        if jugando:
            teclas = pygame.key.get_pressed()
            if teclas[pygame.K_w] or teclas[pygame.K_UP]:
                direccion = ARRIBA
            elif teclas[pygame.K_s] or teclas[pygame.K_DOWN]:
                direccion = ABAJO
            else:
                direccion = QUIETO
            if direccion != ultima_dir:
                try:
                    sock.sendall(empaquetar(MOVE_REQ, 0, bytes([direccion])))
                except OSError:  # el servidor ya cerro la conexion; el hilo de red lo avisa
                    pass
                ultima_dir = direccion

        pantalla.fill((0, 0, 0))
        if jugando or terminado:
            # Linea central
            for y in range(0, CAMPO_ALTO, 30):
                pygame.draw.rect(pantalla, (80, 80, 80), (CAMPO_ANCHO // 2 - 1, y, 2, 15))
            # Paletas: la propia en verde
            x_paletas = [PALETA_MARGEN, CAMPO_ANCHO - PALETA_MARGEN - PALETA_ANCHO]
            for i in range(2):
                color = (0, 220, 0) if i + 1 == slot else (255, 255, 255)
                pygame.draw.rect(pantalla, color, (x_paletas[i], paletas[i], PALETA_ANCHO, PALETA_ALTO))
            pygame.draw.rect(pantalla, (255, 255, 255), (pelota[0], pelota[1], PELOTA_TAM, PELOTA_TAM))
            marcador = fuente.render(f"{score[0]}   {score[1]}", True, (255, 255, 255))
            pantalla.blit(marcador, (CAMPO_ANCHO // 2 - marcador.get_width() // 2, 10))
            lado = "izquierda" if slot == 1 else "derecha"
            info = fuente.render(f"Tu: jugador {slot} ({lado})  vs  {rival}", True, (150, 150, 150))
            pantalla.blit(info, (10, CAMPO_ALTO - 35))
        if not jugando:
            aviso = fuente.render(texto, True, (255, 200, 0))
            pantalla.blit(aviso, (CAMPO_ANCHO // 2 - aviso.get_width() // 2, CAMPO_ALTO // 2 - 60))
            if terminado:
                cierre = fuente.render("Cierra la ventana para salir", True, (150, 150, 150))
                pantalla.blit(cierre, (CAMPO_ANCHO // 2 - cierre.get_width() // 2, CAMPO_ALTO // 2 - 20))

        pygame.display.flip()
        reloj.tick(60)

    sock.close()
    pygame.quit()


if __name__ == "__main__":
    main()
