// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
#include "game_structs.h"


int next_movement(Game_map *game, Player *player, unsigned short width, unsigned short height) {
    unsigned char best_move = 0;
    int max_value = -1000000;

    for (unsigned char dir = 0; dir < 8; dir++) {
        int new_x = player->x + dx[dir];
        int new_y = player->y + dy[dir];

        if (new_x < 0 || new_x >= width || new_y < 0 || new_y >= height)
            continue;

        int value = game->board[new_y * width + new_x];

        // Evitar moverse a una celda bloqueada
        if (value > 0) {
            if (value > max_value) {
                max_value = value;
                best_move = dir;
            }
        } 
        if (max_value==9) {
            break;
        }
        
    }
    return best_move;
}

int get_player_index(Game_map* game){
    int player_index = -1;
        pid_t pid = getpid();
        for (int i = 0; i < game->num_players; i++) {
            if (game->players[i].pid == pid) {
                return i;
            }
        }
        return player_index;
}


int main(int argc, char const *argv[])
{
    pid_t pid = getpid();

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
        return 1;
    }

    unsigned short width = atoi(argv[1]);
    unsigned short height = atoi(argv[2]);

    //Calcular el tamaño total de la memoria compartida
    size_t shm_size = sizeof(Game_map) + (width * height * sizeof(int));

    // Abrir la memoria compartida (sin O_CREAT porque ya esta creada)
    int shm_state= shm_handler(SHM_NAME_STATE, O_RDONLY, "shm_state", 0, NULL);
    int shm_sync= shm_handler(SHM_NAME_SYNC, O_RDWR, "shm_sync",0, NULL);

    // Mapear la memoria compartida
    Game_map *game = shm_map(shm_state, shm_size, PROT_READ, "shm_state");
    Semaphores *sems = shm_map(shm_sync, sizeof(Semaphores), PROT_READ | PROT_WRITE, "shm_sync");

    // Booleano para identificar si es el primer movimiento
    bool first_movement = true;

    // Booleano para ver si el jugador se movió
    bool has_move = false;

    // Para ver si no se movió debido a un movimiento invalido
    int invalid_moves=false;

    // Coordenadas para chequear si se movio
    int player_x = 0;
    int player_y = 0;

    bool error=false;


    // Determinar el índice del jugador basado en su PID
    int player_index=get_player_index(game);
    Player *player;
    if (player_index== -1) {
        fprintf(stderr, "Error: No se encontró el jugador con PID %d en la lista de jugadores.\n", pid);
        error=true;
    }else{
        // Obtener el jugador actual
        player = &game->players[player_index];
    }


     // Bucle principal del jugador
     while (!error) {

        wait_sem(&sems->master_mutex);
        post(&sems->master_mutex);
        wait_sem(&sems->game_player_mutex);

        if (sems->players_reading == 0){
            wait_sem(&sems->game_state_mutex);
        }
        sems->players_reading+=1;
        post(&sems->game_player_mutex);

        //Consultar estado
        // Verificar si el juego terminó
        if (game->game_over) {
            
            wait_sem(&sems->game_player_mutex);
            if (sems->players_reading-- == 1){
                post(&sems->game_state_mutex);
            }
            post(&sems->game_player_mutex);
            break;
        }
    
    
        // Verificar si el jugador está bloqueado
        if (player->blocked){
            wait_sem(&sems->game_player_mutex);
            if (sems->players_reading-- == 1){
                post(&sems->game_state_mutex);
            }
            post(&sems->game_player_mutex);
            break;
        }

        // Guardar la posición inicial del jugador y marcar que se ha movido por ser la primer posicion
        if (first_movement){
            first_movement=false;
            has_move=true;
            player_x = player->x;
            player_y = player->y;
        }
        // El jugador se ha movido, actualizar las coordenadas
        if (player->x != player_x || player->y != player_y) {
            has_move = true;
            player_x = player->x;
            player_y = player->y;
        }

        // Guardamos los movimientos invalidos cuando leemos el estado
        
        int aux_invalid_moves = player->invalid_moves;
        
        //Decidir el siguiente movimiento, se usa info del estado
        unsigned char movement = next_movement(game, player, width, height);

        wait_sem(&sems->game_player_mutex);
        if (sems->players_reading-- == 1){
            post(&sems->game_state_mutex);
        }
        post(&sems->game_player_mutex);

        //Enviar movimiento

        if (has_move) {  
            if(write(STDOUT_FILENO, &movement, sizeof(movement)) == -1){
                perror("Error al escribir en el pipe");
                break;
            }
            has_move = false;
            
        }
        else if (aux_invalid_moves > invalid_moves){
            if(write(STDOUT_FILENO, &movement, sizeof(movement)) == -1){
                perror("Error al escribir en el pipe");
                break;
            }
            invalid_moves = aux_invalid_moves;
            
        }
    }
    
    // Liberar recursos
    shm_closer(game, shm_size, sems, shm_state, shm_sync, 0);

    return 0;
}