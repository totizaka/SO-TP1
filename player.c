#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <stdbool.h>

#define SHM_NAME "/game_state"

typedef struct {
    char player_name[16];   // Nombre del jugador
    unsigned int points;     // Puntaje
    unsigned int invalid_moves; // Cantidad de movimientos inválidos
    unsigned int valid_moves;   // Cantidad de movimientos válidos
    unsigned short x, y;        // Coordenadas en el tablero
    pid_t pid;             // Identificador del proceso del jugador
    bool blocked;          // Indica si el jugador está bloqueado
} Player;

typedef struct {
    unsigned short width;     // Ancho del tablero
    unsigned short height;    // Alto del tablero
    unsigned int num_players; // Cantidad de jugadores
    Player players[9];        // Lista de jugadores (máximo 9)
    bool game_over;           // Indica si el juego terminó
    int board[];              // Tablero (arreglo flexible al final)
} GameMap;


int main(int argc, char const *argv[])
{
    printf("hola player");

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
        return 1;
    }

    unsigned short width = atoi(argv[1]);
    unsigned short height = atoi(argv[2]);

    // Calcular el tamaño total de la memoria compartida
    size_t shm_size = sizeof(GameMap) + (width * height * sizeof(int));

   

// Abrir la memoria compartida (sin O_CREAT porque ya esta creada)
    int fd = shm_open("/game_state", O_RDWR, 0666);
    if (fd == -1) {
    perror("shm_open");
    exit(1);
}

// Mapear la memoria compartida
GameMap *game = mmap(NULL, shm_size, PROT_READ, MAP_SHARED, fd, 0);
//en player como mapeo sin tener el tamaño del tablero??
if (game == MAP_FAILED) {
    perror("mmap");
    exit(1);
}

    return 0;
}



