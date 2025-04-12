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

int best_adjacent_move(GameMap *game, Player *player, unsigned short width, unsigned short height) {
    unsigned char best_move = 0;
    int max_value = -1000000;

    for (unsigned char dir = 0; dir < 8; dir++) {
        int new_x = player->x + dx[dir];
        int new_y = player->y + dy[dir];

        if (new_x < 0 || new_x >= width || new_y < 0 || new_y >= height)
            continue;

        int value = game->board[new_y * width + new_x];

        if (value>=6){ //Idea : para mayr rapidez si el valor es alto , que lo tome 
            max_value = value;
            best_move = dir;
            return best_move;

        }

        if (value >= 1 && value <= 9 && value > max_value) {
            max_value = value;
            best_move = dir;
        }
    }

    return best_move;
}

int get_player_index(GameMap* game){
    pid_t pid = getpid();
    for (int i = 0; i < game->num_players; i++) {
        if (game->players[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char const *argv[])
{
    pid_t pid = getpid();
    srand(pid);

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
        return 1;
    }

    unsigned short width = atoi(argv[1]);
    unsigned short height = atoi(argv[2]);

    size_t shm_size = sizeof(GameMap) + (width * height * sizeof(int));

    int shm_state = shm_handler(SHM_NAME_STATE, O_RDONLY, 0666, "shm_state", 0, NULL);
    int shm_sync = shm_handler(SHM_NAME_SYNC, O_RDWR, 0666, "shm_sync", 0, NULL);

    GameMap *game = shm_map(shm_state, shm_size, PROT_READ, "shm_state");
    Semaphores *sems = shm_map(shm_sync, sizeof(Semaphores), PROT_READ | PROT_WRITE, "shm_sync");

    int sendMovements = 0;
    int invalid_moves = 0;
    int player_x = 0, player_y = 0;
    int hasMove = 0;

    struct pollfd pfd;
    pfd.fd = STDOUT_FILENO;
    pfd.events = POLLOUT;

    while (1) {
        sem_wait(&sems->master_mutex);
        sem_post(&sems->master_mutex);
        sem_wait(&sems->game_player_mutex);
        if (sems->players_reading == 0){
            sem_wait(&sems->game_state_mutex);
        }
        sems->players_reading += 1;
        sem_post(&sems->game_player_mutex);

        if (game->game_over) {
            sem_wait(&sems->game_player_mutex);
            if (sems->players_reading == 1){
                sem_post(&sems->game_state_mutex);
            }
            sems->players_reading -= 1;
            sem_post(&sems->game_player_mutex);
            break;
        }

        int player_index = get_player_index(game);
        if (player_index == -1) {
            fprintf(stderr, "Error: No se encontró el jugador con PID %d en la lista de jugadores.\n", pid);
            sem_wait(&sems->game_player_mutex);
            if (sems->players_reading == 1){
                sem_post(&sems->game_state_mutex);
            }
            sems->players_reading -= 1;
            sem_post(&sems->game_player_mutex);
            break;
        }

        Player *player = &game->players[player_index];

        if (player->blocked){
            sem_wait(&sems->game_player_mutex);
            if (sems->players_reading == 1){
                sem_post(&sems->game_state_mutex);
            }
            sems->players_reading -= 1;
            sem_post(&sems->game_player_mutex);
            break;
        }

        if (sendMovements == 0){
            player_x = player->x;
            player_y = player->y;
        }

        if (player->x != player_x || player->y != player_y) {
            hasMove = 1;
            player_x = player->x;
            player_y = player->y;
        }

        int state_invalid_moves = player->invalid_moves;

        unsigned char movement = best_adjacent_move(game, player, width, height);

        sem_wait(&sems->game_player_mutex);
        if (sems->players_reading == 1){
            sem_post(&sems->game_state_mutex);
        }
        sems->players_reading -= 1;
        sem_post(&sems->game_player_mutex);

        if ((hasMove == 0 && sendMovements == 0) || (hasMove == 1)) {
            if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLOUT)) {
                if (write(STDOUT_FILENO, &movement, sizeof(movement)) == -1) {
                    perror("Error al escribir en el pipe");
                    break;
                }
                sendMovements++;
                hasMove = 0;
            }
        }
        else if (hasMove == 0 && state_invalid_moves > invalid_moves) {
            if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLOUT)) {
                if (write(STDOUT_FILENO, &movement, sizeof(movement)) == -1) {
                    perror("Error al escribir en el pipe");
                    break;
                }
                sendMovements++;
                invalid_moves = state_invalid_moves;
                hasMove = 0;
            }
        }
    }

    shm_closer(game, shm_size, sems, shm_state, shm_sync, 0);

    return 0;
}
