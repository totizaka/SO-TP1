// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <stdio.h>
#include "master_functions.h"


void print_usage(const char *prog_name) {
    fprintf(stderr, "Uso: %s [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v view] -p player1 [player2 ...]\n", prog_name);
    exit(EXIT_FAILURE);
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

void create_shared_memory(int width, int height, int *shm_state, int *shm_sync, Game_map **game, Semaphores **sems) {
    size_t shm_size = sizeof(Game_map) + (width * height * sizeof(int));

    // Crear memoria compartida para los semáforos
    *shm_state = shm_handler(SHM_NAME_STATE, O_CREAT | O_RDWR, 0666, "shm_state", 0, NULL);
    ftruncate(*shm_state, shm_size);

    *shm_sync  = shm_handler(SHM_NAME_SYNC,  O_CREAT | O_RDWR, 0666, "shm_sync", 1, SHM_NAME_STATE);
    ftruncate(*shm_sync, sizeof(Semaphores));

    // Mapear memoria compartida
    *game = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, *shm_state, 0);
    if (*game == MAP_FAILED) {
        perror("Error mapeando shm_state");
        shm_closer(NULL,0, NULL, *shm_state, *shm_sync,1);
        exit(EXIT_FAILURE);
    }
    
    *sems = mmap(NULL, sizeof(Semaphores), PROT_READ | PROT_WRITE, MAP_SHARED, *shm_sync, 0);
    if (*sems == MAP_FAILED) {
        perror("Error mapeando shm_sync");
        shm_closer(NULL, 0, NULL, *shm_state, *shm_sync, 1);
        exit(EXIT_FAILURE);
    }
}

void initialize_board(Game_map *game, int width, int height, unsigned int seed) {
    srand(seed); // Usar la semilla proporcionada para generar números aleatorios
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            game->board[y * width + x] = (rand() % 9) + 1; // Generar un número entre 1 y 9
        }
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

// Función para hacer shuffle de un array
void shuffle(int *array, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

void initialize_players(Game_map *game, char *player_paths[], int num_players, int width, int height) {
    float cx = width / 2.0;
    float cy = height / 2.0;
    float a = width / 3.0;     // semieje horizontal 
    float b = height / 3.0;    // semieje vertical   

    int shuffled_indices[9];
    for (int i = 0; i < num_players; i++){
        shuffled_indices[i] = i;
    }

    shuffle(shuffled_indices, num_players);

    for (int i = 0; i < num_players; i++) {
        int player_idx = shuffled_indices[i];
        float angle = 2 * M_PI * i / num_players;

        int x = (int)(cx + a * cos(angle));
        int y = (int)(cy + b * sin(angle));

        // Asegurar que (x, y) está dentro del tablero
        x = (x < 0) ? 0 : (x >= width) ? width - 1 : x;
        y = (y < 0) ? 0 : (y >= height) ? height - 1 : y;

        game->players[player_idx].x = x;
        game->players[player_idx].y = y;
        game->players[player_idx].points = 0;
        game->players[player_idx].valid_moves = 0;
        game->players[player_idx].invalid_moves = 0;
        game->players[player_idx].blocked = false;

        strncpy(game->players[player_idx].player_name, player_paths[player_idx], sizeof(game->players[player_idx].player_name) - 1);
        game->players[player_idx].player_name[sizeof(game->players[player_idx].player_name) - 1] = '\0';

        game->board[y * width + x] = -player_idx;
    }
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

void launch_player_processes(Game_map *game, Semaphores *sems, int shm_state, int shm_sync, 
    char *player_paths[], int num_players, int width, int height, int player_pipes[MAX_PLAYERS][2]) {

    pid_t player_pid;

    for (int i = 0; i < num_players; i++) {
        // Crear un pipe para la comunicación con el jugador
        if (pipe(player_pipes[i]) == -1) {
            perror("Error creando pipe");
            shm_closer(game,  sizeof(Game_map) + ( game->width * game->height * sizeof(int) ),sems,shm_state,shm_sync,1);
            exit(EXIT_FAILURE);
        }

        // Crear el proceso del jugador
        player_pid = create_player(player_pipes[i], player_paths[i], width, height);

        // Guardar el PID del jugador en la estructura del juego
        game->players[i].pid = player_pid;
    }
}

void set_reading_pipes(fd_set *read_fds, int *max_fd, Game_map *game, int player_pipes[][2], int num_players){
    for (int i = 0; i < num_players; i++) {
        if (!game->players[i].blocked) {
            FD_SET(player_pipes[i][0], read_fds);
            if (player_pipes[i][0] > *max_fd) {
                *max_fd = player_pipes[i][0];
            }
        }
    }
}

bool check_timeout(time_t start_time, int timeout){
    if (time(NULL) - start_time > timeout) {
        return true;
    }
    return false;
}

bool players_all_blocked(Game_map *game, int num_players){
    for (int i = 0; i < num_players; i++) {
        if (!game->players[i].blocked) {
            return false;
        }
    }
    return true;
}

bool validate_move(Game_map *game, int player_index, unsigned char move) {
    Player *player = &game->players[player_index];
    int new_x = player->x + dx[move];
    int new_y = player->y + dy[move];

    // Verificar límites del tablero
    if (new_x < 0 || new_x >= game->width || new_y < 0 || new_y >= game->height) {
        return 0; // Movimiento inválido
    }

    // Verificar si la celda está ocupada
    int cell_value = game->board[new_y * game->width + new_x];

    return (cell_value > 0 && cell_value <10) ;
}

void apply_move(Game_map *game, int player_index, unsigned char move) {
    Player *player = &game->players[player_index];
    int new_x = player->x + dx[move];
    int new_y = player->y + dy[move];

    // Capturar la celda
    int reward = game->board[new_y * game->width + new_x];
    game->board[new_y * game->width + new_x] = -(player_index);

    // Actualizar el puntaje y la posición del jugador
    player->points += reward;
    player->x = new_x;
    player->y = new_y;
    player->valid_moves++;
}

bool block_player(Game_map* game, int player_num){
    for (int j = 0; j < 8; j++){
        if(validate_move(game, player_num, j)){
           return false;
        }
    }
    return true;
}
    
void setup_game(int width, int height, unsigned int seed, int *shm_state, int *shm_sync, Game_map **game, Semaphores **sems, char *player_paths[], int num_players) {
    // Crear memoria compartida y semáforos
    create_shared_memory(width, height, shm_state, shm_sync, game, sems);

    // Inicializar estructura del juego
    (*game)->width = width;
    (*game)->height = height;
    (*game)->num_players = num_players;
    (*game)->game_over = false;

    // Inicializar el tablero con números aleatorios
    initialize_board(*game, width, height, seed);

    // Inicializar semáforos
    initialize_semaphores(*sems);

    // Inicializar y ubicar a los jugadores
    initialize_players(*game, player_paths, num_players, width, height);
}

bool end_game(Game_map *game, Semaphores *sems, time_t start_time, unsigned int timeout, int num_players){
    // Verificar timeout
    if (check_timeout(start_time, timeout)|| players_all_blocked(game, num_players)) {
        game->game_over = true;
        //Para que la vista termine
        post(&sems->view_pending);
        return true;
    }
    return false ;
}

void movement_handler(Game_map *game, Semaphores *sems, fd_set *read_fds, int player_pipes[][2], int num_players, int *start_rr, time_t *start_time) {
    for (int i = 0; i < num_players; i++) {
        int index = (*start_rr + i) % num_players;

        if (FD_ISSET(player_pipes[index][0], read_fds)) {
            unsigned char move;
            int bytes_read = read(player_pipes[index][0], &move, sizeof(move));

            wait_sem(&sems->master_mutex); 
            wait_sem(&sems->game_state_mutex);
            post(&sems->master_mutex);

            if (bytes_read == 0) {
                game->players[index].blocked = true; 
            } else if (bytes_read > 0) {
                if (validate_move(game, index, move)) {
                    apply_move(game, index, move);
                    *start_time = time(NULL); // Reiniciar timeout
                } else {
                    game->players[index].invalid_moves++;
                }
                game->players[index].blocked = block_player(game, index);
            }

            post(&sems->game_state_mutex);
        }
    }

    *start_rr = (*start_rr + 1) % num_players;
}

void wait_for_process(pid_t pid, const char *desc) {
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        printf("El proceso %s terminó con código de salida %d.\n", desc, WEXITSTATUS(status));
    } else {
        printf("El proceso %s no terminó correctamente.\n", desc);
    }
}

void wait_for_players_processes(Game_map *game, int num_players) {
    for (int i = 0; i < num_players; i++) {
        char desc[64];
        snprintf(desc, sizeof(desc), "del jugador %d (%s)", i, game->players[i].player_name);
        wait_for_process(game->players[i].pid, desc);
    }
}



void close_pipes(int player_pipes[MAX_PLAYERS][2], int num_players) {
    for (int i = 0; i < num_players; i++) {
        close(player_pipes[i][0]); // lectura
        close(player_pipes[i][1]); // Escritura
    }
}

void print_game_ending(Game_map *game, int num_players){
    // Imprimir resumen final del juego
    printf("\n=== Resumen del juego ===\n\n");

    unsigned int max_points = 0;
    int winner_index = -1;
    bool tie = false;

    // Imprimir resumen de cada jugador
    for (int i = 0; i < num_players; i++) {
        printf("Jugador %2d (%-9s): %4u puntos / %3u mov inválidos / %3u mov válidos\n", i, game->players[i].player_name, game->players[i].points, game->players[i].invalid_moves, game->players[i].valid_moves);
        
        if (game->players[i].points > max_points) {
            max_points = game->players[i].points;
            winner_index = i;
            tie = false;
        } else if (game->players[i].points == max_points) {
            int current_invalid = game->players[i].invalid_moves;
            int winner_invalid = game->players[winner_index].invalid_moves;
    
            if (current_invalid < winner_invalid) {
                winner_index = i;
                tie = false;
            } else if (current_invalid == winner_invalid) {
                int current_valid = game->players[i].valid_moves;
                int winner_valid = game->players[winner_index].valid_moves;
    
                if (current_valid < winner_valid) {
                    winner_index = i;
                    tie = false;
                } else if (current_valid == winner_valid) {
                    tie = true;
                }
            } else {
                tie = true;
            }
        }
    }

    if (tie && max_points > 0) {
        printf("\n¡Hay un empate entre jugadores con %u puntos y mismos criterios de desempate!\n", max_points);
    } else if (winner_index != -1) {
        printf("\n¡El ganador es el Jugador %d!!! (%s) con %u puntos / %u movimientos invalidos / %u movimientos validos\n",
               winner_index,
               game->players[winner_index].player_name,
               max_points,
               game->players[winner_index].invalid_moves,
               game->players[winner_index].valid_moves);
    } else {
        printf("\nNo hay ganador.\n");
    }
}