// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
#include "game_structs.h"


#define MAX_PLAYERS 9
#define DEFAULT_WIDTH 10
#define DEFAULT_HEIGHT 10
#define DEFAULT_DELAY 200
#define DEFAULT_TIMEOUT 10

int cabeza = 10; // Carácter que representa la cabeza del jugador


void print_usage(const char *prog_name) {
    fprintf(stderr, "Uso: %s [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v view] -p player1 [player2 ...]\n", prog_name);
    exit(EXIT_FAILURE);
}

void cleanup_resources(GameMap *game, Semaphores *sems, int shm_state, int shm_sync) {
    if (game) munmap(game, sizeof(GameMap) + (game->width * game->height * sizeof(int)));
    if (sems) munmap(sems, sizeof(Semaphores));

    close(shm_state);
    close(shm_sync);

    shm_unlink(SHM_NAME_STATE);
    shm_unlink(SHM_NAME_SYNC);
}


int validate_move(GameMap *game, int player_index, unsigned char move) {

    Player *player = &game->players[player_index];
    int new_x = player->x + dx[move];
    int new_y = player->y + dy[move];

    // Verificar límites del tablero
    if (new_x < 0 || new_x >= game->width || new_y < 0 || new_y >= game->height) {
        return 0; // Movimiento inválido
    }

    // Verificar si la celda está ocupada
    int cell_value = game->board[new_y * game->width + new_x];
    if (cell_value > 0 && cell_value <10) {
        return 1; // Celda valida
    }

    return 0; // Celda
}

void apply_move(GameMap *game, int player_index, unsigned char move) {

    Player *player = &game->players[player_index];
    int new_x = player->x + dx[move];
    int new_y = player->y + dy[move];

    //Actualizar la celda anterior
    game->board[player->y * game->width + player->x] = -(player_index); // Marcar la celda anterior

    // Capturar la celda
    int reward = game->board[new_y * game->width + new_x];
    if(cabeza*player_index == 0){
        game->board[new_y * game->width + new_x] = 11; // Marcar la celda como ocupada por el jugador
    }
    else{
        game->board[new_y * game->width + new_x] = cabeza*player_index; // Poner la cabeza del jugador en la nueva celda
    }

    // Actualizar el puntaje y la posición del jugador
    player->points += reward;
    player->x = new_x;
    player->y = new_y;
    player->valid_moves++;

}

pid_t create_player(int * player_pipe, char* player_path, int width, int height){
    pid_t pid = fork();
        if (pid == 0) {
            close(player_pipe[0]); // Cerrar extremo de lectura
            dup2(player_pipe[1], STDOUT_FILENO); // Redirigir stdout al pipe
            close(player_pipe[1]);

            char width_str[10], height_str[10];
            sprintf(width_str, "%d", width);
            sprintf(height_str, "%d", height);
            execl(player_path, player_path, width_str, height_str, NULL); 
            perror("Error ejecutando el jugador");
            exit(EXIT_FAILURE);
        }
        close(player_pipe[1]); // Cerrar extremo de escritura en el máster

    return pid;
}

bool block_player(GameMap* game, int player_num){
    for (int j = 0; j < 8; j++){
        if(validate_move(game, player_num, j)){
           return false;
        }
    }
    return true;


}
    
void parse_arguments(int argc, char *argv[], unsigned int *delay, unsigned int *timeout, unsigned int *seed,
    char **view_path, char *player_paths[], int *num_players, unsigned short *width, unsigned short *height) {

    *delay = DEFAULT_DELAY;
    *timeout = DEFAULT_TIMEOUT;
    *seed = time(NULL);
    *view_path = NULL;
    *num_players = 0;
    *width = DEFAULT_WIDTH;
    *height = DEFAULT_HEIGHT;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            *width = atoi(argv[++i]);
            if (*width < DEFAULT_WIDTH) *width = DEFAULT_WIDTH;
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            *height = atoi(argv[++i]);
            if (*height < DEFAULT_HEIGHT) *height = DEFAULT_HEIGHT;
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            *delay = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            *timeout = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            *seed = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) {
            *view_path = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0) {
            while (i + 1 < argc && *num_players < MAX_PLAYERS && argv[i + 1][0] != '-') {
                player_paths[(*num_players)++] = argv[++i];
            }
        } else {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (*num_players == 0) {
        fprintf(stderr, "Error: Debe especificar al menos un jugador con -p.\n");
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
}


void create_shared_memory(int width, int height, int *shm_state, int *shm_sync, GameMap **game, Semaphores **sems) {
    size_t shm_size = sizeof(GameMap) + (width * height * sizeof(int));

    // Crear memoria compartida para los semáforos
    *shm_state = shm_handler(SHM_NAME_STATE, O_CREAT | O_RDWR, 0666, "shm_state", 0, NULL);
    ftruncate(*shm_state, shm_size);


    *shm_sync  = shm_handler(SHM_NAME_SYNC,  O_CREAT | O_RDWR, 0666, "shm_sync", 1, SHM_NAME_STATE);
    ftruncate(*shm_sync, sizeof(Semaphores));

    // Mapear memoria compartida
    *game = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, *shm_state, 0);
    if (*game == MAP_FAILED) {
        perror("Error mapeando shm_state");
        cleanup_resources(NULL, NULL, *shm_state, *shm_sync);
        exit(EXIT_FAILURE);
    }
    

    *sems = mmap(NULL, sizeof(Semaphores), PROT_READ | PROT_WRITE, MAP_SHARED, *shm_sync, 0);
    if (*sems == MAP_FAILED) {
        perror("Error mapeando shm_sync");
        cleanup_resources(*game, NULL, *shm_state, *shm_sync);
        exit(EXIT_FAILURE);
    }
}

void initialize_board(GameMap *game, int width, int height, unsigned int seed) {
    srand(seed); // Usar la semilla proporcionada para generar números aleatorios
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            game->board[y * width + x] = (rand() % 9) + 1; // Generar un número entre 1 y 9
        }
    }
}


void distribute_players(GameMap *game, char *player_paths[], int num_players, int width, int height) {
    int positions[9][2] = {0}; // Para almacenar las posiciones
    
    if (num_players == 1) {
        // Un solo jugador al centro
        positions[0][0] = width / 2;
        positions[0][1] = height / 2;
    } 
    else if (num_players == 2) {
        // Dos jugadores en extremos opuestos
        positions[0][0] = width / 4; positions[0][1] = height / 2;
        positions[1][0] = 3 * width / 4; positions[1][1] = height / 2;
    } 
    else if (num_players == 3 || num_players == 4) {
        // Tres o cuatro jugadores en esquinas
        positions[0][0] = width / 4; positions[0][1] = height / 4;
        positions[1][0] = 3 * width / 4; positions[1][1] = height / 4;
        positions[2][0] = width / 4; positions[2][1] = 3 * height / 4;
        if (num_players == 4) positions[3][0] = 3 * width / 4, positions[3][1] = 3 * height / 4;
    } 
    else if (num_players >= 5 && num_players <= 6) {
        // Cinco o seis jugadores en hexágono
        positions[0][0] = width / 4; positions[0][1] = height / 4;
        positions[1][0] = 3 * width / 4; positions[1][1] = height / 4;
        positions[2][0] = width / 4; positions[2][1] = 3 * height / 4;
        positions[3][0] = 3 * width / 4; positions[3][1] = 3 * height / 4;
        positions[4][0] = width / 2; positions[4][1] = height / 2;
        if (num_players == 6) positions[5][0] = width / 2, positions[5][1] = height / 4;
    } 
    else {
        // Siete a nueve jugadores en una cuadrícula espaciosa
        int step_x = width / 4, step_y = height / 4;
        int index = 0;
        for (int i = 1; i <= 3 && index < num_players; i++) {
            for (int j = 1; j <= 3 && index < num_players; j++) {
                positions[index][0] = j * step_x;
                positions[index][1] = i * step_y;
                index++;
            }
        }
    }

    // Asignar posiciones a los jugadores
    for (int i = 0; i < num_players; i++) {
        int x = positions[i][0];
        int y = positions[i][1];

        game->players[i].x = x;
        game->players[i].y = y;
        game->players[i].points = 0;
        game->players[i].valid_moves = 0;
        game->players[i].invalid_moves = 0;
        game->players[i].blocked = false;
        strncpy(game->players[i].player_name, player_paths[i], sizeof(game->players[i].player_name) - 1);
        game->players[i].player_name[sizeof(game->players[i].player_name) - 1] = '\0';
 
        // Marcar la celda inicial del jugador con su índice
        game->board[y * width + x] = -i;
    }
}


void initialize_semaphores(Semaphores *sems) {
    sem_init(&sems->view_pending, 1, 0);
    sem_init(&sems->view_done, 1, 0);
    sem_init(&sems->master_mutex, 1, 1);
    sem_init(&sems->game_state_mutex, 1, 1);
    sem_init(&sems->game_player_mutex, 1, 1);
    sems->players_reading = 0;
}

void launch_view_process(const char *view_path, int width, int height, pid_t *view_pid) {
    if (view_path) {
        *view_pid = fork();
        if (*view_pid == 0) {
            // Convertir width y height a strings
            char width_str[10], height_str[10];
            sprintf(width_str, "%d", width);
            sprintf(height_str, "%d", height);

            // Ejecutar el proceso de la vista
            execl(view_path, view_path, width_str, height_str, NULL);
            
            // Si execl falla
            perror("Error ejecutando la vista");
            exit(EXIT_FAILURE);
        }
    }
}

void create_player_processes(GameMap *game, Semaphores *sems, int shm_state, int shm_sync, 
    char *player_paths[], int num_players, int width, int height, int player_pipes[MAX_PLAYERS][2]) {

    pid_t player_pid;

    for (int i = 0; i < num_players; i++) {
        // Crear un pipe para la comunicación con el jugador
        if (pipe(player_pipes[i]) == -1) {
            perror("Error creando pipe");
            cleanup_resources(game, sems, shm_state, shm_sync);
            exit(EXIT_FAILURE);
        }

        // Crear el proceso del jugador
        player_pid = create_player(player_pipes[i], player_paths[i], width, height);

        // Guardar el PID del jugador en la estructura del juego
        game->players[i].pid = player_pid;
    }
}


int main(int argc, char *argv[]) {

    // Creamos variables para los argumentos
    unsigned int delay, timeout, seed;
    char *view_path;
    char *player_paths[MAX_PLAYERS];
    int num_players;
    unsigned short width, height;

    // Definimos los argumentos por defecto y los parseamos
    parse_arguments(argc, argv, &delay, &timeout, &seed, &view_path, player_paths, &num_players, &width, &height);
   
    // Crear memoria compartida para el estado del juego y los semáforos
    GameMap *game;
    Semaphores *sems;
    int shm_state, shm_sync;
    create_shared_memory(width, height, &shm_state, &shm_sync, &game, &sems);

    // Inicializar el estado del juego
    game->width = width;
    game->height = height;
    game->num_players = num_players;
    game->game_over = false;

    // Inicializar el tablero con valores aleatorios entre 1 y 9
    initialize_board(game, width, height, seed);
   
    // Inicializar semáforos
    initialize_semaphores(sems);

    // Distribuir jugadores en el tablero                         
    distribute_players(game, player_paths, num_players, width, height);

    // Crear proceso para la vista
    pid_t viewPid;
    launch_view_process(view_path, width, height, &viewPid);

    // Crear pipes y el proceso para cada jugador
    int player_pipes[MAX_PLAYERS][2];
    create_player_processes(game, sems, shm_state, shm_sync, player_paths, num_players, width, height, player_pipes);

    // Bucle principal del máster
    time_t start_time = time(NULL);
    fd_set read_fds;
    while (!game->game_over) {

        // Verificar timeout
        if (time(NULL) - start_time > timeout) {
            game->game_over = true;
            //Para que la vista termine
            sem_post(&sems->view_pending);
            break;
        }

        int amountBlocked = 0;
        for (int i = 0; i < num_players; i++){
            if(game->players[i].blocked){
                amountBlocked+=1;
            }
        }
        if (amountBlocked == num_players){
            game->game_over = true;
            //Para que la vista termine
            sem_post(&sems->view_pending);
            break;
        }
        
        // Notificar a la vista que hay cambios
        sem_post(&sems->view_pending);

        // Esperar a que la vista termine de imprimir
        sem_wait(&sems->view_done);
        

        // Configurar los pipes para lectura 
        FD_ZERO(&read_fds);
        int max_fd = -1;
        for (int i = 0; i < num_players; i++) {
            if (!game->players[i].blocked) {
                FD_SET(player_pipes[i][0], &read_fds);
                if (player_pipes[i][0] > max_fd) {
                    max_fd = player_pipes[i][0];
                }
            }
        }


        // Esperar solicitudes de movimientos
        struct timeval tv = {timeout, 0};
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ready == -1) {
            perror("Error en select");
            break;
        } else if (ready == 0) {
            // Timeout sin movimientos válidos
            game->game_over = true;
            break;
        }
        // sem_wait(&sems->master_mutex); //bloqueo lectura de players antes de procesar y ejecutar movimiento
        // sem_wait(&sems->game_state_mutex);
        // sem_post(&sems->master_mutex);
        // Procesar movimientos
        for (int i = 0; i < num_players; i++) {
            if (FD_ISSET(player_pipes[i][0], &read_fds)) {
                unsigned char move;
                int bytes_read;
                bytes_read = read(player_pipes[i][0], &move, sizeof(move));

                sem_wait(&sems->master_mutex); 
                sem_wait(&sems->game_state_mutex);
                sem_post(&sems->master_mutex);

                if (bytes_read == 0) {                          //preguntar
                    // Jugador bloqueado (EOF)
                    game->players[i].blocked = true;
                    printf("lo bloquie\n"); 
                } else if (bytes_read > 0) {
                    if (validate_move(game, i, move)) {
                        apply_move(game, i, move);
                        start_time = time(NULL); // Reiniciar timeout
                    } else {
                        game->players[i].invalid_moves++;
                        printf("Cantidad de movimientos inválidos del jugador %d (%s): %u\n", i, game->players[i].player_name, game->players[i].invalid_moves);
                        
                        game->players[i].blocked = block_player(game,i);// Bloquear al jugador si no hay movimientos válidos
                        
                        if(game->players[i].blocked){
                            printf("Bloqueo del jugador %d (%s)\n", i, game->players[i].player_name);
                        }
                             
                    }
                }
                sem_post(&sems->game_state_mutex);
            }
        }
        
        // sem_post(&sems->game_state_mutex);

        // Respetar el delay configurado
        usleep(delay * 1000); // Convertir a microsegundos
    }
    
    unsigned int max_points = 0;
    int winner_index = -1;
    bool tie = false;
    

    // Esperar a que la view termine
    int viewStatus;
    waitpid(viewPid, &viewStatus, 0);
    if (WIFEXITED(viewStatus)) {
        printf("El proceso de vista terminó con código de salida %d.\n", WEXITSTATUS(viewStatus));
    } else {
        printf("El proceso de vista no terminó correctamente.\n");
    }

    // Esperar a que los procesos hijo terminen
    for (int i = 0; i < num_players; i++) {
    int status;
    waitpid(game->players[i].pid, &status, 0);
    if(i<1){
           // Imprimir resumen final del juego
    printf("\n=== Resumen del juego ===\n");
    }
    printf("Jugador %d (%s): %u puntos\n", i, game->players[i].player_name, game->players[i].points);
    if (game->players[i].points > max_points) {
        max_points = game->players[i].points;
        winner_index = i;
        tie = false; // Reiniciar el estado de empate
    } else if (game->players[i].points == max_points) {
        tie = true; // Hay un empate
    }
    printf("Jugador %d (%s) terminó con código de salida %d.\n", i, game->players[i].player_name, WEXITSTATUS(status));
    }
    if (tie && max_points > 0) {
        printf("¡Hay un empate entre los jugadores con %u puntos!\n", max_points);
    } else if (winner_index != -1) {
        printf("¡El ganador es el Jugador %d (%s) con %u puntos!\n", winner_index, game->players[winner_index].player_name, max_points);
    } else {
        printf("No hay ganador.\n");
    }

    // Liberar recursos
    cleanup_resources(game, sems, shm_state, shm_sync);

    return 0;
}