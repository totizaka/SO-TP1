#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define SHM_NAME_STATE "/game_state"
#define SHM_NAME_SYNC "/game_sync"

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

typedef struct {
    sem_t view_pending; // Se usa para indicarle a la vista que hay cambios por imprimir
    sem_t view_done; // Se usa para indicarle al master que la vista terminó de imprimir
    sem_t master_mutex; // Mutex para evitar inanición del master al acceder al estado
    sem_t game_state_mutex; // Mutex para el estado del juego
    sem_t game_player_mutex; // Mutex para la siguiente variable
    unsigned int players_reading; // Cantidad de jugadores leyendo el estado
} Semaphores;
    

int main(int argc, char const *argv[])
{
    printf("hola view");

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
        return 1;
    }

    unsigned short width = atoi(argv[1]);
    unsigned short height = atoi(argv[2]);

    // Calcular el tamaño total de la memoria compartida
    size_t shm_size = sizeof(GameMap) + (width * height * sizeof(int));

    // Abrir la memoria compartida (sin O_CREAT porque ya esta creada)
    int shm_state = shm_open("/game_state", O_RDONLY, 0666);
    if (shm_state == -1) {
        perror("shm_state open fail");
        exit(EXIT_FAILURE);
    }

    int shm_sync = shm_open("/game_sync", O_RDWR, 0666);
    if (shm_sync == -1) {
        perror("shm_sync open fail");
        exit(EXIT_FAILURE);
    }

    // Mapear la memoria compartida
    GameMap *game = mmap(NULL, shm_size, PROT_READ, MAP_SHARED, shm_state, 0);
    if (game == MAP_FAILED) {
        perror("shm_state fail to mmap");
        exit(EXIT_FAILURE);
    }

    Semaphores *sems = mmap(NULL, sizeof(Semaphores), PROT_READ | PROT_WRITE, MAP_SHARED, shm_sync, 0);
    if (sems == MAP_FAILED) {
        perror("shm_sync fail to mmap");
        exit(EXIT_FAILURE);
    }

    while(1){
        //Esperamos
        sem_wait(&sems->view_pending);

        //IMPRESION

        // Imprimir estado
        printf("Tablero de %dx%d\n", game->width, game->height);
        printf("Jugadores: %d\n", game->num_players);
        
        for (int i = 0; i < game->num_players; i++) {
            printf("Jugador %d: %s - Puntos: %d - Posición: (%d, %d)\n",
                i, game->players[i].player_name, game->players[i].points,
                game->players[i].x, game->players[i].y);
        }

        //Imprimir Tablero
        printf("\nTablero:\n");
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                printf("| %d ", game->board[y * width + x]);
            }
            printf("|\n");
            for (int x = 0; x < width; x++) {
                printf("----");
            }
            printf("\n");
        }

        //Seguimos
        sem_post(&sems->view_done);
    }

    close(shm_state);
    close(shm_sync);
    return 0;
}
