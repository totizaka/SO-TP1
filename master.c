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

#define SHM_NAME_STATE "/game_state"
#define SHM_NAME_SYNC "/game_sync"
#define MAX_PLAYERS 9
#define DEFAULT_WIDTH 10
#define DEFAULT_HEIGHT 10
#define DEFAULT_DELAY 200
#define DEFAULT_TIMEOUT 10

typedef struct {
    char player_name[16];
    unsigned int points;
    unsigned int invalid_moves;
    unsigned int valid_moves;
    unsigned short x, y;
    pid_t pid;
    bool blocked;
} Player;

typedef struct {
    unsigned short width;
    unsigned short height;
    unsigned int num_players;
    Player players[MAX_PLAYERS];
    bool game_over;
    int board[];
} GameMap;

typedef struct {
    sem_t view_pending;
    sem_t view_done;
    sem_t master_mutex;
    sem_t game_state_mutex;
    sem_t game_player_mutex;
    unsigned int players_reading;
} Semaphores;

int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

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
    // Direcciones de movimiento (arriba, arriba-derecha, derecha, abajo-derecha, abajo, abajo-izquierda, izquierda, arriba-izquierda)
    // int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    // int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    Player *player = &game->players[player_index];
    int new_x = player->x + dx[move];
    int new_y = player->y + dy[move];

    // Verificar límites del tablero
    if (new_x < 0 || new_x >= game->width || new_y < 0 || new_y >= game->height) {
        return 0; // Movimiento inválido
    }

    // Verificar si la celda está ocupada
    int cell_value = game->board[new_y * game->width + new_x];
    if (cell_value <= 0) {
        return 0; // Celda ocupada
    }

    return 1; // Movimiento válido
}

void apply_move(GameMap *game, int player_index, unsigned char move) {
    // int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    // int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

    Player *player = &game->players[player_index];
    int new_x = player->x + dx[move];
    int new_y = player->y + dy[move];

    // Capturar la celda
    int reward = game->board[new_y * game->width + new_x];
    game->board[new_y * game->width + new_x] = -(player_index); // Marcar la celda con -(player_index + 1)

    // Actualizar el puntaje y la posición del jugador
    player->points += reward;
    player->x = new_x;
    player->y = new_y;
    player->valid_moves++;

    // int i=0;

    // for (i = 0; i < 8; i++){
    //     if(validate_move(game, player_index, i)){
    //        return; // Si hay un movimiento válido, salir de la función
    //     }
    // }
    // player[player_index].blocked = true; // Bloquear al jugador si no hay movimientos válidos

}

int main(int argc, char *argv[]) {
    // Parámetros por defecto
    unsigned short width = DEFAULT_WIDTH;
    unsigned short height = DEFAULT_HEIGHT;
    unsigned int delay = DEFAULT_DELAY;
    unsigned int timeout = DEFAULT_TIMEOUT;
    unsigned int seed = time(NULL);
    char *view_path = NULL;
    char *player_paths[MAX_PLAYERS];
    int num_players = 0;

    // Analizar argumentos manualmente
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            width = atoi(argv[++i]);
            if (width < DEFAULT_WIDTH) width = DEFAULT_WIDTH;
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            height = atoi(argv[++i]);
            if (height < DEFAULT_HEIGHT) height = DEFAULT_HEIGHT;
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            delay = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            timeout = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            seed = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) {
            view_path = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0) {
            while (i + 1 < argc && num_players < MAX_PLAYERS && argv[i + 1][0] != '-') {
                player_paths[num_players++] = argv[++i];
            }
        } else {
            print_usage(argv[0]);
        }
    }

    if (num_players == 0) {
        fprintf(stderr, "Error: Debe especificar al menos un jugador con -p.\n");
        print_usage(argv[0]);
    }

    // Calcular el tamaño total de la memoria compartida
    size_t shm_size = sizeof(GameMap) + (width * height * sizeof(int));

    // Crear memoria compartida para el estado del juego
    int shm_state = shm_open(SHM_NAME_STATE, O_CREAT | O_RDWR, 0666);
    if (shm_state == -1) {
        perror("Error creando shm_state");
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_state, shm_size);

    // Crear memoria compartida para los semáforos
    int shm_sync = shm_open(SHM_NAME_SYNC, O_CREAT | O_RDWR, 0666);
    if (shm_sync == -1) {
        perror("Error creando shm_sync");
        shm_unlink(SHM_NAME_STATE);
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_sync, sizeof(Semaphores));

    // Mapear memoria compartida
    GameMap *game = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_state, 0);
    if (game == MAP_FAILED) {
        perror("Error mapeando shm_state");
        cleanup_resources(NULL, NULL, shm_state, shm_sync);
        exit(EXIT_FAILURE);
    }

    Semaphores *sems = mmap(NULL, sizeof(Semaphores), PROT_READ | PROT_WRITE, MAP_SHARED, shm_sync, 0);
    if (sems == MAP_FAILED) {
        perror("Error mapeando shm_sync");
        cleanup_resources(game, NULL, shm_state, shm_sync);
        exit(EXIT_FAILURE);
    }

    // Inicializar el estado del juego
    game->width = width;
    game->height = height;
    game->num_players = num_players;
    game->game_over = false;

    // Inicializar el tablero con valores aleatorios entre 1 y 9
    srand(seed); // Usar la semilla proporcionada para generar números aleatorios
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            game->board[y * width + x] = (rand() % 9) + 1; // Generar un número entre 1 y 9
        }
    }

    // int * m= malloc(sizeof(int)); //para probar leaks con valgrind
   
    // Inicializar semáforos
    sem_init(&sems->view_pending, 1, 0);
    sem_init(&sems->view_done, 1, 1);
    sem_init(&sems->master_mutex, 1, 1);
    sem_init(&sems->game_state_mutex, 1, 1);
    sem_init(&sems->game_player_mutex, 1, 1);
    sems->players_reading = 0;

    // Distribuir jugadores en el tablero
    for (int i = 0; i < num_players; i++) {
        int x, y;
        do {
            x = rand() % width;
            y = rand() % height;
        } while (game->board[y * width + x] < 0); // Buscar una celda libre

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

    // Crear procesos para la vista y los jugadores
    if (view_path) {
        pid_t view_pid = fork();
        if (view_pid == 0) {
            char width_str[10], height_str[10];
            sprintf(width_str, "%d", width);
            sprintf(height_str, "%d", height);
            execl(view_path, view_path, width_str, height_str, NULL);
            perror("Error ejecutando la vista");
            exit(EXIT_FAILURE);
        }
    }

    int player_pipes[MAX_PLAYERS][2];
    for (int i = 0; i < num_players; i++) {
        if (pipe(player_pipes[i]) == -1) {
            perror("Error creando pipe");
            cleanup_resources(game, sems, shm_state, shm_sync);
            exit(EXIT_FAILURE);
        }

        pid_t player_pid = fork();
        if (player_pid == 0) {
            close(player_pipes[i][0]); // Cerrar extremo de lectura
            dup2(player_pipes[i][1], STDOUT_FILENO); // Redirigir stdout al pipe
            close(player_pipes[i][1]);

            char width_str[10], height_str[10];
            sprintf(width_str, "%d", width);
            sprintf(height_str, "%d", height);
            execl(player_paths[i], player_paths[i], width_str, height_str, NULL);
            perror("Error ejecutando el jugador");
            exit(EXIT_FAILURE);
        }
        close(player_pipes[i][1]); // Cerrar extremo de escritura en el máster
        game->players[i].pid = player_pid;
    }

    // Bucle principal del máster
    time_t start_time = time(NULL);
    fd_set read_fds;
    while (!game->game_over) {
        // Verificar timeout
        if (time(NULL) - start_time > timeout) {
            game->game_over = true;
            break;
        }

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

        // Procesar movimientos
        for (int i = 0; i < num_players; i++) {
            if (FD_ISSET(player_pipes[i][0], &read_fds)) {
                unsigned char move;
                int bytes_read;

                // Proteger el acceso al pipe con game_player_mutex
                sem_wait(&sems->game_player_mutex);
                bytes_read = read(player_pipes[i][0], &move, sizeof(move));
                sem_post(&sems->game_player_mutex);

                if (bytes_read == 0) {                          //preguntar
                    // Jugador bloqueado (EOF)
                    sem_wait(&sems->game_state_mutex);
                    game->players[i].blocked = true;
                    printf("lo bloquie\n");
                    sem_post(&sems->game_state_mutex);

                    // Actualizar players_reading
                    sem_wait(&sems->master_mutex);
                    sems->players_reading--;
                    sem_post(&sems->master_mutex);
                } else if (bytes_read > 0) {
                    sem_wait(&sems->game_state_mutex);
                    if (validate_move(game, i, move)) {
                        apply_move(game, i, move);
                        start_time = time(NULL); // Reiniciar timeout
                    } else {
                        game->players[i].invalid_moves++;
                        printf("Cantidad de movimientos inválidos del jugador %d (%s): %u\n", i, game->players[i].player_name, game->players[i].invalid_moves);
                        
                        int j=0;
                        int valido=0;

                        for (j = 0; j < 8; j++){
                            if(validate_move(game, i, j)){
                                valido=1;
                                break; // Si hay un movimiento válido, salir del for
                            }
                        }
                        if(!valido){
                            game->players[i].blocked = true; // Bloquear al jugador si no hay movimientos válidos
                            printf("Bloqueo del jugador %d (%s)\n", i, game->players[i].player_name);
                        }
                            
                    }
                    sem_post(&sems->game_state_mutex);
                }
            }
        }

        // Notificar a la vista que hay cambios
        sem_post(&sems->view_pending);

        // Esperar a que la vista termine de imprimir
        sem_wait(&sems->view_done);

        // Respetar el delay configurado
        usleep(delay+1000000);
    }

 
    /*for (int i = 0; i < num_players; i++) {
        printf("Jugador %d (%s): %d puntos\n", i, game->players[i].player_name, game->players[i].points);
        if (game->players[i].points > max_points) {
            max_points = game->players[i].points;
            winner_index = i;
            tie = false; // Reiniciar el estado de empate
        } else if (game->players[i].points == max_points) {
            tie = true; // Hay un empate
        }
    }*/
    
    unsigned int max_points = 0;
    int winner_index = -1;
    bool tie = false;
    

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