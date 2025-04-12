// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <time.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include "game_structs.h"


// Códigos ANSI para colores

const char *letters_colors[] = {
    "\033[38;5;196m", // Rojo flúo
    "\033[38;5;46m",  // Verde brillante
    "\033[38;5;27m",  // Azul eléctrico
    "\033[38;5;226m", // Amarillo flúo
    "\033[38;5;165m", // Violeta fuerte
    "\033[38;5;208m", // Naranja brillante
    "\033[38;5;213m", // Rosa vibrante
    "\033[38;5;15m",  // Blanco puro
    "\033[38;5;51m"   // Celeste eléctrico
};

const char *player_head_colors[] = {
    "\033[48;5;196m",  // Rojo flúo
    "\033[48;5;46m",   // Verde brillante
    "\033[48;5;27m",   // Azul eléctrico
    "\033[48;5;226m",  // Amarillo flúo
    "\033[48;5;165m",  // Violeta fuerte (más violeta que 201)
    "\033[48;5;214m",  // Naranja brillante
    "\033[48;5;213m",  // Rosa vibrante
    "\033[48;5;15m",   // Blanco puro
    "\033[48;5;51m"    // Celeste eléctrico
};

const char *player_tail_colors[] = {
    "\033[48;5;52m",   // Rojo oscuro
    "\033[48;5;22m",   // Verde musgo
    "\033[48;5;18m",   // Azul muy oscuro
    "\033[48;5;100m",  // Amarillo más oscuro pero no naranja
    "\033[48;5;54m",   // Violeta oscuro
    "\033[48;5;130m",  // Naranja tierra
    "\033[48;5;125m",  // Rosa viejo
    "\033[48;5;240m",  // Gris medio oscuro
    "\033[48;5;25m"    // Celeste oscuro
};

#define RESET_COLOR "\033[0m"


void print_player_state(Game_map *game) {
    printf("\033[3J\033[H\033[2J");  // Limpia pantalla y scrollback
    fflush(stdout);

    // Imprimir estado
    printf("\nTablero de %dx%d\n", game->width, game->height);
    printf("Jugadores: %u\n", game->num_players);

    for (int i = 0; i < game->num_players; i++) {
        printf("%sJugador %2d:%s %-15s - Puntos: %3u - Posición: (%2hu, %2hu)\t", 
            letters_colors[i], i, RESET_COLOR,
            game->players[i].player_name,
            game->players[i].points,
            game->players[i].x, game->players[i].y);

        printf("\tJugador %d %s\n", i, game->players[i].blocked ? "está bloqueado." : "no está bloqueado.");
    }
    printf("\n");
}


void print_game_board(Game_map *game, int width, int height, const char *player_colors[], const Player *players, int num_players) {

    printf("Tablero:\n");
    for (int x = 0; x < width; x++) {
        printf("---");
    }
    printf("\n");
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int cell = game->board[y * width + x];
    
            // Veo si es la cabeza
            int is_head = 0;
            for (int p = 0; p < num_players; p++) {
                if (players[p].x == x && players[p].y == y) {
                    printf("|%s  %s", player_colors[p], RESET_COLOR);
                    is_head = 1;
                    break;
                }
            }
    
            if (is_head) continue;
    
            // Celda capturada 
            else if (cell <= 0) {
                int player_id = -cell;
                printf("|%s  %s", player_tail_colors[player_id], RESET_COLOR);
            }
            // Puntaje común
            else {
                printf("|%2d", cell);
            }
        }
    
        printf("|\n");
        for (int x = 0; x < width; x++) {
            printf("---");
        }
        printf("\n");
    }
}


void clear_terminal() {
    printf("\033[3J\033[H\033[2J");
    fflush(stdout);
}



int main(int argc, char const *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <width> <height>\n", argv[0]);
        return 1;
    }

    unsigned short width = atoi(argv[1]);
    unsigned short height = atoi(argv[2]);

    // Calcular el tamaño total de la memoria compartida
    size_t shm_size = sizeof(Game_map) + (width * height * sizeof(int));

    // Abrir la memoria compartida (sin O_CREAT porque ya esta creada)
    
    int shm_state= shm_handler("/game_state", O_RDONLY, 0666, "shm_state",0, NULL);
    int shm_sync= shm_handler("/game_sync", O_RDWR, 0666, "shm_sync",0, NULL);

    // Mapear la memoria compartida

    Game_map *game = shm_map(shm_state, shm_size, PROT_READ, "shm_state");
    Semaphores *sems = shm_map(shm_sync, sizeof(Semaphores), PROT_READ | PROT_WRITE, "shm_sync");

    while(!game->game_over){
        //Esperamos
        wait_sem(&sems->view_pending);

        clear_terminal();
        
        // Imprimir estado de los jugadores
        print_player_state(game);

        Player* players = game->players;
    
        // Imprimir Tablero
        print_game_board(game, width, height, player_head_colors, players, game->num_players);        
        //Seguimos
        post(&sems->view_done);
        }

    shm_closer(game, shm_size, sems, shm_state, shm_sync,0);

    return 0;
}
