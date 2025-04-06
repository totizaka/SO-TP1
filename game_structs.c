#include "game_structs.h"
#include <stdio.h>



// Desplazamientos para las 8 direcciones posibles
const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};


    int shm_handler(char *name, int flag, mode_t mode,const char *desc, int auth_flag, const char *to_clean){
        int shm_segment = shm_open(name, flag, mode);
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


