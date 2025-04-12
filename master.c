// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <stdio.h>
#include "master_functions.h"

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

    // Inicializar y distribuir jugadores en el tablero                         
    initialize_players(game, player_paths, num_players, width, height);

    pid_t viewPid;
    if(view_path != NULL){
        // Crear proceso para la vista
        launch_view_process(view_path, width, height, &viewPid);
    }

    // Crear pipes y el proceso para cada jugador
    int player_pipes[MAX_PLAYERS][2];
    launch_player_processes(game, sems, shm_state, shm_sync, player_paths, num_players, width, height, player_pipes);

    
    time_t start_time = time(NULL);
    fd_set read_fds;

    // Para el uso de round robin
    int index = 0;
    int start = 0;
    
    // Bucle principal del máster
    while (!game->game_over) {

        // Verificar timeout
        if (check_timeout(start_time, timeout)) {
            game->game_over = true;
            //Para que la vista termine
            post(&sems->view_pending);
            break;
        }

        // Verificar si estan todos los jugadores bloqueados
        if (players_all_blocked(game, num_players)){
            game->game_over = true;
            //Para que la vista termine
            post(&sems->view_pending);
            break;
        }
        
        if(view_path != NULL){
            // Notificar a la vista que hay cambios
            post(&sems->view_pending);

            // Esperar a que la vista termine de imprimir
            wait_sem(&sems->view_done);
        }
        

        // Configurar los pipes para lectura 
        FD_ZERO(&read_fds);
        int max_fd = -1;
        set_reading_pipes(&read_fds, &max_fd, game, player_pipes, num_players);


        // Esperar solicitudes de movimientos
        struct timeval tv = {timeout, 0};
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ready == -1) {
            perror("Error en select");
            break;
        } else if (ready == 0) {
            // Timeout sin movimientos válidos
            game->game_over = true;
            post(&sems->view_pending);
            break;
        }
        
        // Procesar movimientos
        for (int i = 0; i < num_players; i++) {

            index = (start+i) % num_players;

            if (FD_ISSET(player_pipes[index][0], &read_fds)) {
                unsigned char move;
                int bytes_read;
                bytes_read = read(player_pipes[index][0], &move, sizeof(move));

                wait_sem(&sems->master_mutex); 
                wait_sem(&sems->game_state_mutex);
                post(&sems->master_mutex);

                if (bytes_read == 0) {                          //preguntar, creo q no es necesario!!
                    // Jugador bloqueado (EOF)
                    game->players[i].blocked = true; 
                } else if (bytes_read > 0) {
                    if (validate_move(game, index, move)) {
                        apply_move(game, index, move);
                        start_time = time(NULL); // Reiniciar timeout
                    } else {
                        game->players[index].invalid_moves++;
                    }
                    // Bloquear al jugador si no hay movimientos válidos
                    game->players[index].blocked = block_player(game,index);
                }
                post(&sems->game_state_mutex);
            }
        }
        
        start = ((start+1) % num_players);

        // Respetar el delay configurado si hay vista
        if(view_path!=NULL){
            usleep(delay * 1000);
        }
    }

    // Esperar a que la view termine
    if(view_path!=NULL){
        wait_for_process(viewPid, "de vista");
    }

    // Esperar a que los procesos hijo terminen
    for (int i = 0; i < num_players; i++) {
        char desc[64];
        snprintf(desc, sizeof(desc), "del jugador %d (%s)", i, game->players[i].player_name);
        wait_for_process(game->players[i].pid, desc);
    }

    print_game_ending(game, num_players);
    
    // Liberar recursos

    // Cerrar pipes de los jugadores
    for (int i = 0; i < num_players; i++) {
        close(player_pipes[i][0]); // Cerrar extremo de lectura del pipe
        close(player_pipes[i][1]); // Cerrar extremo de escritura del pipe
    }
    // Cerrar memoria compartida
    shm_closer(game,  sizeof(GameMap) + ( game->width * game->height * sizeof(int) ),sems,shm_state,shm_sync,1);


    return 0;
}