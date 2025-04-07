//game_functions.h
#ifndef GAME_FUNCTIONS

#include <semaphore.h>
#include <stdbool.h>
#include <sys/types.h>
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


//Definiciones 
#define SHM_NAME_STATE "/game_state"
#define SHM_NAME_SYNC "/game_sync"


//Estructuras 


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
    sem_t master_mutex; // Mutex para evitar inanición del master al acceder al estado <-- jugadores con master         writer
    sem_t game_state_mutex; // Mutex para el estado del juego <-- jugadores con master                                  mutex
    sem_t game_player_mutex; // Mutex para la siguiente variable <-- acceder a la variable de a un jugador              readers_count_mutex
    unsigned int players_reading; // Cantidad de jugadores leyendo el estado del juego
} Semaphores;

extern const int dx[8];  // Solo declaración
extern const int dy[8];  // Solo declaración

//Funciones de manejo de memoria compartida


// shm_handler: Maneja la creación y apertura de memoria compartida
int shm_handler(char *name, int flag, mode_t mode,const char *desc, int auth_flag, const char *to_clean);

// shm_map: Mapea la memoria compartida a la dirección de memoria del proceso
void *shm_map(int fd, size_t size, int prot, const char *desc);

// shm_closer: Cierra y libera la memoria compartida y los semáforos
void shm_closer(GameMap *game, size_t game_size, Semaphores *sems, int shm_state, int shm_sync, int auth_flag);

#endif

