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

    
    pid_t pid= getpid();
    srand(pid);//Utilizamos el pid para inicializar la semilla

    // Buscar al jugador actual
    Player *player = NULL;
    for (int i = 0; i < game->num_players; i++) {
        if (game->players[i].pid == pid) {
            player = &game->players[i];
            break;
        }
    }


    if (player == NULL) {
        fprintf(stderr, "Jugador no encontrado en la memoria compartida\n");
        return 1;
    }


    move_randomly(player, game);


    
    close(shm_state);
    close(shm_sync);
    return 0;
}


void  move_random(Player *player, GameMap *game){
        // Direcciones posibles (x, y): arriba, abajo, izquierda, derecha, y 4 diagonales
        int directions[8][2] = {
            {0, -1}, // Arriba
            {0, 1},  // Abajo
            {-1, 0}, // Izquierda
            {1, 0},  // Derecha
            {-1, -1}, // Arriba izquierda
            {-1, 1},  // Abajo izquierda
            {1, -1},  // Arriba derecha
            {1, 1}    // Abajo derecha
        };

        // Elegir una dirección aleatoria
        int move = rand() % 8;
        int new_x = player->x + directions[move][0];
        int new_y = player->y + directions[move][1];

        // Verificar que el movimiento sea válido
        if (new_x >= 0 && new_x < game->width && new_y >= 0 && new_y < game->height) {
            // Verificar si la casilla está ocupada
            bool valid_move = true;
            for (int i = 0; i < game->num_players; i++) {
                if (game->players[i].x == new_x && game->players[i].y == new_y) {
                    valid_move = false;
                    break;
                } 
            }

            // Si el movimiento es válido, realizar el movimiento
            if (valid_move) {
                player->x = new_x;
                player->y = new_y;
                printf("Jugador %s se movió a la posición (%d, %d)\n", player->player_name, player->x, player->y);
            } else {
                printf("Movimiento inválido: La casilla (%d, %d) ya está ocupada.\n", new_x, new_y);
                player->invalid_moves++;
            }
        } else {
            printf("Movimiento fuera de los límites del tablero.\n");
            player->invalid_moves++;
        }
    }



