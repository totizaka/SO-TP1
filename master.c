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
    Game_map *game;
    Semaphores *sems;
    int shm_state, shm_sync;
    setup_game( width, height, seed, &shm_state, &shm_sync, &game, &sems, player_paths, num_players);

    pid_t view_pid;
    if(view_path != NULL){
        // Crear proceso para la vista
        launch_view_process(view_path, width, height, &view_pid);
    }

    // Crear pipes y el proceso para cada jugador
    int player_pipes[MAX_PLAYERS][2];
    launch_player_processes(game, sems, shm_state, shm_sync, player_paths, num_players, width, height, player_pipes);

    time_t start_time = time(NULL);
    fd_set read_fds;

    // Para el uso de round robin
    int start_rr = 0;
    
    // Bucle principal del máster
    while (!game->game_over) {

        if(end_game(game, sems, start_time, timeout, num_players)) break;
        
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
        movement_handler(game, sems, &read_fds, player_pipes, num_players, &start_rr, &start_time);

        // Respetar el delay configurado si hay vista
        if(view_path!=NULL){
            usleep(delay * 1000);
        }
    }

    // Esperar a que la view termine
    if(view_path!=NULL){
        wait_for_process(view_pid, "de vista");
    }

    // Esperar a que los jugadores terminen
    wait_for_players_processes(game , num_players);

    // Printar el resumen final del juego
    print_game_ending(game, num_players);
    
    // Liberar recursos

    // Cerrar pipes de los jugadores
    close_pipes(player_pipes, num_players);

    // Cerrar memoria compartida
    shm_closer(game,  sizeof(Game_map) + ( game->width * game->height * sizeof(int) ),sems,shm_state,shm_sync,1);

    return 0;
}