# Variables
CC = gcc
CFLAGS = -Wall -g -fsanitize=address

OBJ_GAME = game_structs.o

# Archivos objeto intermedios
OBJ_MASTER = master_tmp.o
OBJ_PLAYER = player_tmp.o
OBJ_VIEW = view_tmp.o

# Ejecutables (con .o como extensión, si así lo querés)
EXE_MASTER = master.o
EXE_PLAYER = player.o
EXE_VIEW = view.o

.PHONY: all clean run

all: $(EXE_MASTER) $(EXE_PLAYER) $(EXE_VIEW)

# Compilar game_structs.c
$(OBJ_GAME): game_structs.c game_structs.h
	$(CC) $(CFLAGS) -c game_structs.c -o $@

# Compilar master.c
$(OBJ_MASTER): master.c game_structs.h
	$(CC) $(CFLAGS) -c master.c -o $@

# Compilar player.c
$(OBJ_PLAYER): player.c game_structs.h
	$(CC) $(CFLAGS) -c player.c -o $@

# Compilar view.c
$(OBJ_VIEW): view.c game_structs.h
	$(CC) $(CFLAGS) -c view.c -o $@

# Enlazar ejecutables
$(EXE_MASTER): $(OBJ_MASTER) $(OBJ_GAME)
	$(CC) $(CFLAGS) $(OBJ_MASTER) $(OBJ_GAME) -o $@

$(EXE_PLAYER): $(OBJ_PLAYER) $(OBJ_GAME)
	$(CC) $(CFLAGS) $(OBJ_PLAYER) $(OBJ_GAME) -o $@

$(EXE_VIEW): $(OBJ_VIEW) $(OBJ_GAME)
	$(CC) $(CFLAGS) $(OBJ_VIEW) $(OBJ_GAME) -o $@

run: all
	@if [ -z "$(PLAYER_PATH)" ]; then \
		echo "Por favor, especifica el path de los jugadores con 'make run PLAYER_PATH=\"path1 path2 ...\"'"; \
		exit 1; \
	fi
	./$(EXE_MASTER) -v ./$(EXE_VIEW) -p $(PLAYER_PATH)

clean:
	rm -f *.o *_tmp.o


