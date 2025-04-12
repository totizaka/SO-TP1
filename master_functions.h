#ifndef MASTER_FUNCTIONS

#include "game_structs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <sys/select.h>
#include <math.h>
#include <time.h>
#include <math.h>

#define M_PI 3.14159265358979323846

#define MAX_PLAYERS 9
#define DEFAULT_WIDTH 10
#define DEFAULT_HEIGHT 10
#define DEFAULT_DELAY 200
#define DEFAULT_TIMEOUT 10

// Funciones de argumentos

// Indicar argumentos
void print_usage(const char *prog_name);

// Manejo de argumentos
void parse_arguments(int argc, char *argv[], unsigned int *delay, unsigned int *timeout, unsigned int *seed,
    char **view_path, char *player_paths[], int *num_players, unsigned short *width, unsigned short *height);


// Memoria compartida
void create_shared_memory(int width, int height, int *shm_state, int *shm_sync, Game_map **game, Semaphores **sems);


// Funciones de inicializacion del juego

// Inicialización del tablero
void initialize_board(Game_map *game, int width, int height, unsigned int seed);

// Inicialización de semáforos
void initialize_semaphores(Semaphores *sems);

// Shuffle de índices
void shuffle(int *array, int n);

// Inicialización de jugadores
void initialize_players(Game_map *game, char *player_paths[], int num_players, int width, int height);

// Seteo del juego
void setup_game(int width, int height, unsigned int seed, int *shm_state, int *shm_sync, Game_map **game, Semaphores **sems, char *player_paths[], int num_players);


// Funciones de lanzamiento de procesos

// Lanzar proceso de vista
void launch_view_process(const char *view_path, int width, int height, pid_t *view_pid);

// Crear jugador
pid_t create_player(int * player_pipe, char* player_path, int width, int height);

// Lanzar procesos de jugadores
void launch_player_processes(Game_map *game, Semaphores *sems, int shm_state, int shm_sync, 
    char *player_paths[], int num_players, int width, int height, int player_pipes[MAX_PLAYERS][2]);

// Seteo de pipes para comuncacion
void set_reading_pipes(fd_set *read_fds, int *max_fd, Game_map *game, int player_pipes[][2], int num_players);


// Funciones de chequeo del finalizacion de juego

// Timeout
bool check_timeout(time_t start_time, int timeout);

// Verifica si todos los jugadores están bloqueados
bool players_all_blocked(Game_map *game, int num_players);

// Chequeo de finalizacion del juego
bool end_game(Game_map *game, Semaphores *sems, time_t start_time, unsigned int timeout, int num_players);


// Funciones de manejo del movimiento

// Validación de movimiento
bool validate_move(Game_map *game, int player_index, unsigned char move);

// Aplicar movimiento
void apply_move(Game_map *game, int player_index, unsigned char move);

// Bloquear jugador
bool block_player(Game_map* game, int player_num);

// Handler para gestionar el movimiento de los players
void movement_handler(Game_map *game, Semaphores *sems, fd_set *read_fds, int player_pipes[][2], int num_players, int *start, time_t *start_time);


// Funciones para la finalizacion del juego 

// Esperar a los prcesos hijos
void wait_for_players_processes(Game_map *game, int num_players);

// Esperar a que un proceso termine
void wait_for_process(pid_t pid, const char *desc);

// Cierre de pipes
void close_pipes(int player_pipes[MAX_PLAYERS][2], int num_players);

// Imprimir resumen del juego
void print_game_ending(Game_map *game, int num_players);

#endif
