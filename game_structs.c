// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "game_structs.h"
#include <stdio.h>

// Desplazamientos para las 8 direcciones posibles
const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

int shm_handler(char *name, int flag,const char *desc, int auth_flag, const char *to_clean){
    int shm_segment = shm_open(name, flag, 0666);
    if (shm_segment == -1) {
        if (auth_flag){
            shm_unlink(to_clean);
        }
        fprintf(stderr, "%s open fail: ", desc);
        perror("");
        exit(EXIT_FAILURE);
    }
    return shm_segment;
}

void *shm_map(int fd, size_t size, int prot, const char *desc) {
    void *mapped = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        fprintf(stderr, "%s mmap failed:\n", desc);
        perror("");
        close(fd);
        exit(EXIT_FAILURE);
    }
    return mapped;
}

void shm_closer(Game_map *game, size_t game_size, Semaphores *sems, int shm_state, int shm_sync, int auth_flag) {
    if (game) munmap(game, game_size);
    if (sems) munmap(sems, sizeof(Semaphores));

    close(shm_state);
    close(shm_sync);

    if (auth_flag){
        shm_unlink(SHM_NAME_STATE);
        shm_unlink(SHM_NAME_SYNC);
    }
}

void wait_sem(sem_t* sem){
    sem_wait(sem);
}
void post(sem_t* sem){
    sem_post(sem);
}
    


