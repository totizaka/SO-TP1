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

    // Calcular el tamaño total de la memoria compartida
    size_t shm_size = sizeof(Game_map) + (width * height * sizeof(int));

    // Abrir la memoria compartida (sin O_CREAT porque ya esta creada)
 
    int shm_state= shm_handler(SHM_NAME_STATE, O_RDONLY, "shm_state", 0, NULL);
    int shm_sync= shm_handler(SHM_NAME_SYNC, O_RDWR, "shm_sync",0, NULL);

    // Mapear la memoria compartida

    Game_map *game = shm_map(shm_state, shm_size, PROT_READ, "shm_state");
    Semaphores *sems = shm_map(shm_sync, sizeof(Semaphores), PROT_READ | PROT_WRITE, "shm_sync");

    int valid_moves = 0;
    int invalid_moves = 0;
    int sendMovement = 0;

    // Para el write del jugador
    struct pollfd pfd;
    pfd.fd = STDOUT_FILENO;
    pfd.events = POLLOUT; // Esperar a que el pipe esté listo para escribir

    // Determinar el índice del jugador basado en su PID
    int player_index = -1;
    bool error=false;
    for (int i = 0; i < game->num_players; i++) {
        if (game->players[i].pid == pid) {
            player_index = i;
            break;
        }
    }
    if (player_index == -1) {
        fprintf(stderr, "Error: No se encontró el jugador con PID %d en la lista de jugadores.\n", pid);

        wait_sem(&sems->game_player_mutex);
        if (sems->players_reading == 1){
            post(&sems->game_state_mutex);
        }
        sems->players_reading-=1;
        post(&sems->game_player_mutex);
        error=true;
    }
    Player *player;
    if(!error){
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
            if (sems->players_reading == 1){
                post(&sems->game_state_mutex);
            }
            sems->players_reading-=1;
            post(&sems->game_player_mutex);
            break;
        }
    
        
    
        // Verificar si el jugador está bloqueado
        if (player->blocked){
            wait_sem(&sems->game_player_mutex);
            if (sems->players_reading == 1){
                post(&sems->game_state_mutex);
            }
            sems->players_reading-=1;
            post(&sems->game_player_mutex);
            break;
        }

        // Verificar si el movimiento fue enviado
        if (player->invalid_moves > invalid_moves){
            invalid_moves = player->invalid_moves;
            sendMovement = 1;
        }
        if (player->valid_moves > valid_moves){
            valid_moves = player->valid_moves;
            sendMovement = 1;
        }
        //Primer movimiento
        if (valid_moves == 0 && invalid_moves == 0){
            sendMovement = 1;
        }
        
        
        //Decidir el siguiente movimiento
        unsigned char movement = rand() % 8;


        wait_sem(&sems->game_player_mutex);
        if (sems->players_reading == 1){
            post(&sems->game_state_mutex);
        }
        sems->players_reading-=1;
        post(&sems->game_player_mutex);

        
        // Enviar el movimiento al máster

        if (sendMovement == 1) {
            if (poll(&pfd, 1, 0) > 0) {  // timeout 0 = no bloqueante
                if (pfd.revents & POLLOUT) {
                    if(write(STDOUT_FILENO, &movement, sizeof(movement)) == -1){
                        perror("Error al escribir en el pipe");
                        break;
                    }
                    sendMovement=0;
                }
            }
        }
    }
    
    // Liberar recursos

    shm_closer(game, shm_size, sems, shm_state, shm_sync,0);
    return 0;
}



