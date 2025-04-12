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
#include <poll.h>
#include "game_structs.h"


int next_movement(GameMap *game, Player *player, unsigned short width, unsigned short height) {
    unsigned char best_move = 0;
    int max_value = -1000000;

    for (unsigned char dir = 0; dir < 8; dir++) {
        int new_x = player->x + dx[dir];
        int new_y = player->y + dy[dir];

        if (new_x < 0 || new_x >= width || new_y < 0 || new_y >= height)
            continue;

        int value = game->board[new_y * width + new_x];

        // Evitar moverse a una celda bloqueada
        if (value > 0 && value < 10) {
            if (value > max_value) {
                max_value = value;
                best_move = dir;
            }
        }
    }
    return best_move;
}

int get_player_index(GameMap* game){
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
    srand(pid);                                         //Utilizamos el pid para inicializar la semilla

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
        return 1;
    }

    unsigned short width = atoi(argv[1]);
    unsigned short height = atoi(argv[2]);

    //Calcular el tamaño total de la memoria compartida
    size_t shm_size = sizeof(GameMap) + (width * height * sizeof(int));

    // Abrir la memoria compartida (sin O_CREAT porque ya esta creada)
    int shm_state= shm_handler(SHM_NAME_STATE, O_RDONLY, 0666, "shm_state", 0, NULL);
    int shm_sync= shm_handler(SHM_NAME_SYNC, O_RDWR, 0666, "shm_sync",0, NULL);

    // Mapear la memoria compartida
    GameMap *game = shm_map(shm_state, shm_size, PROT_READ, "shm_state");
    Semaphores *sems = shm_map(shm_sync, sizeof(Semaphores), PROT_READ | PROT_WRITE, "shm_sync");

    // Booleano para identificar si es el primer movimiento
    bool firstMovement = 1;

    // Booleano para ver si el jugador se movió
    bool hasMove = 0;

    // Para ver si no se movió debido a un movimiento invalido
    int invalid_moves=0;

    // Coordenadas para chequear si se movio
    int player_x = 0;
    int player_y = 0;

    bool error=false;

    struct pollfd pfd;
    pfd.fd = STDOUT_FILENO;
    pfd.events = POLLOUT; // Esperar a que el pipe esté listo para escribir

    // Determinar el índice del jugador basado en su PID
    int player_index=get_player_index(game);
    if (player_index== -1) {
        fprintf(stderr, "Error: No se encontró el jugador con PID %d en la lista de jugadores.\n", pid);
        error=true;
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
    
        // Obtener el jugador actual
        Player *player = &game->players[player_index];
    
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
        if (firstMovement){
            firstMovement=0;
            hasMove=1;
            player_x = player->x;
            player_y = player->y;
        }
        // El jugador se ha movido, actualizar las coordenadas
        if (player->x != player_x || player->y != player_y) {
            hasMove = 1;
            player_x = player->x;
            player_y = player->y;
        }

        int state_invalid_moves = player->invalid_moves;
        
        
        //Decidir el siguiente movimiento, se usa info del estado
        
        unsigned char movement = next_movement(game, player, width, height);

        
        wait_sem(&sems->game_player_mutex);
        if (sems->players_reading-- == 1){
            post(&sems->game_state_mutex);
        }
        post(&sems->game_player_mutex);


        //Enviar movimiento

        if ((hasMove == 1)) {
            if (poll(&pfd, 1, 0) > 0) {  // timeout 0 = no bloqueante
                if (pfd.revents & POLLOUT) {
                    if(write(STDOUT_FILENO, &movement, sizeof(movement)) == -1){
                        perror("Error al escribir en el pipe");
                        break;
                    }
                    hasMove = 0;
                }
            }else{
                perror("POLL FALLA\n");
            }
        }
        else if (state_invalid_moves > invalid_moves){
            if (poll(&pfd, 1, 0) > 0) {  // timeout 0 = no bloqueante
                if (pfd.revents & POLLOUT) {
                    if(write(STDOUT_FILENO, &movement, sizeof(movement)) == -1){
                        perror("Error al escribir en el pipe");
                        break;
                    }
                    invalid_moves = state_invalid_moves;
                }
            }else{
                perror("POLL FALLO\n");
            }
        }
    }
    
    // Liberar recursos
    shm_closer(game, shm_size, sems, shm_state, shm_sync, 0);

    return 0;
}