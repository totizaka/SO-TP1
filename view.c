// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
    "\033[31m", // Rojo
    "\033[32m", // Verde
    "\033[33m", // Amarillo
    "\033[34m", // Azul
    "\033[35m", // Magenta
    "\033[36m", // Cian
    "\033[91m", // Rojo claro
    "\033[92m", // Verde claro
    "\033[93m"  // Amarillo claro
};

const char *player_colors[] = {
    "\033[41m", // Fondo rojo
    "\033[42m", // Fondo verde
    "\033[43m", // Fondo amarillo
    "\033[44m", // Fondo azul
    "\033[45m", // Fondo magenta
    "\033[46m", // Fondo cian
    "\033[101m", // Fondo rojo claro
    "\033[102m", // Fondo verde claro
    "\033[103m"  // Fondo amarillo claro
};

// Códigos ANSI para colores de fondo más oscuros (256-color)
const char *player_bg_colors[] = {
    "\033[48;5;52m",  // Rojo oscuro
    "\033[48;5;22m",  // Verde oscuro
    "\033[48;5;58m",  // Amarillo oscuro (mezcla ocre)
    "\033[48;5;17m",  // Azul oscuro
    "\033[48;5;53m",  // Magenta oscuro
    "\033[48;5;23m",  // Cian oscuro
    "\033[48;5;88m",  // Rojo vino / más profundo
    "\033[48;5;22m",  // Verde musgo
    "\033[48;5;94m"   // Amarillo tierra / dorado apagado
};

#define RESET_COLOR "\033[0m"


void print_game_board(GameMap *game, int width, int height, const char *player_colors[]) {
            
    printf("\nTablero:\n");
    for (int x = 0; x < width; x++) {
        printf("-----");
    }
    printf("\n");
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int cell = game->board[y * width + x];
            if (cell < 0) {
                // Celda capturada por un jugador
                int player_id = -cell;
                printf("| %s  %s ", player_bg_colors[player_id % 9], RESET_COLOR); // Fondo de color
            } else if (cell == 0) {
                printf("| %s  %s ", player_bg_colors[0], RESET_COLOR);
            } else if (cell == 11) {
                printf("| %s  %s ", player_colors[0], RESET_COLOR);
            } else if (cell % 10 == 0) {
                int player_id = cell / 10;
                printf("| %s  %s ", player_colors[player_id % 9], RESET_COLOR);
            } else {
                printf("| %2d ", cell);
            }
        }
        printf("|\n");
        for (int x = 0; x < width; x++) {
            printf("-----");
        }
        printf("\n");
        
    }
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
    size_t shm_size = sizeof(GameMap) + (width * height * sizeof(int));

    // Abrir la memoria compartida (sin O_CREAT porque ya esta creada)
    
    int shm_state= shm_handler("/game_state", O_RDONLY, 0666, "shm_state",0, NULL);
    int shm_sync= shm_handler("/game_sync", O_RDWR, 0666, "shm_sync",0, NULL);

    // Mapear la memoria compartida

    GameMap *game = shm_map(shm_state, shm_size, PROT_READ, "shm_state");
    Semaphores *sems = shm_map(shm_sync, sizeof(Semaphores), PROT_READ | PROT_WRITE, "shm_sync");

    while(!game->game_over){
        //Esperamos
        sem_wait(&sems->view_pending);

        //IMPRESION

        // Imprimir estado
        printf("Tablero de %dx%d\n", game->width, game->height);
        printf("Jugadores: %u\n", game->num_players);
        
        for (int i = 0; i < game->num_players; i++) {
            printf("%sJugador%s %d: %s - Puntos: %u - Posición: (%hu, %hu)\t", letters_colors[i % 9], RESET_COLOR,
                i, game->players[i].player_name, game->players[i].points,
                game->players[i].x, game->players[i].y);
            if(game->players[i].blocked) {
                printf("Jugador %d está bloqueado.\n", i);
            }
            else {
                printf("Jugador %d no está bloqueado.\n", i);
            }
        }

    
        // Imprimir Tablero
        print_game_board(game, width, height, player_colors);        
        //Seguimos
        sem_post(&sems->view_done);
        }

    shm_closer(game, shm_size, sems, shm_state, shm_sync,0);

    return 0;
}
