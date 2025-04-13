# Compilador y flags
CC = gcc
CFLAGS = -Wall -g

# Archivos objeto comunes
OBJ_GAME = game_structs.o
OBJ_MASTER_FUNCS = master_functions.o

# Archivos fuente
SRC_MASTER = master.c
SRC_MASTER_FUNCS = master_functions.c
SRC_PLAYER2 = player2.c
SRC_VIEW = view.c

# Archivos objeto
OBJ_MASTER = master.o
OBJ_PLAYER = player.o
OBJ_PLAYER2 = player2.o
OBJ_PLAYER3 = player3.o
OBJ_PLAYER4 = player4.o

OBJ_VIEW = view.o

# Ejecutables
EXE_MASTER = master
EXE_PLAYER2 = player2
EXE_VIEW = view

# Targets principales
.PHONY: all clean run

all: $(EXE_MASTER) $(EXE_PLAYER) $(EXE_PLAYER2) $(EXE_PLAYER3) $(EXE_PLAYER4) $(EXE_VIEW)

# Compilar archivos comunes
$(OBJ_GAME): game_structs.c game_structs.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_MASTER_FUNCS): $(SRC_MASTER_FUNCS) master_functions.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compilar y generar objetos
$(OBJ_MASTER): $(SRC_MASTER) game_structs.h master_functions.h
	$(CC) $(CFLAGS) -c $< -o $@


$(OBJ_PLAYER2): $(SRC_PLAYER2) game_structs.h
	$(CC) $(CFLAGS) -c $< -o $@


$(OBJ_VIEW): $(SRC_VIEW) game_structs.h
	$(CC) $(CFLAGS) -c $< -o $@

# Enlazar ejecutables
$(EXE_MASTER): $(OBJ_MASTER) $(OBJ_MASTER_FUNCS) $(OBJ_GAME)
	$(CC) $(CFLAGS) $^ -o $@ -lm


$(EXE_PLAYER2): $(OBJ_PLAYER2) $(OBJ_GAME)
	$(CC) $(CFLAGS) $^ -o $@ -lm



$(EXE_VIEW): $(OBJ_VIEW) $(OBJ_GAME)
	$(CC) $(CFLAGS) $^ -o $@ -lm

# Correr el master con los jugadores
run: all
	@if [ -z "$(PLAYER_PATH)" ]; then \
		echo "Por favor, especifica el path de los jugadores con 'make run PLAYER_PATH=\"path1 path2 ...\"'"; \
		exit 1; \
	fi
	./$(EXE_MASTER) -v ./$(EXE_VIEW) -p $(PLAYER_PATH)

# Limpiar todo
clean:
	rm -f *.o $(EXE_MASTER) $(EXE_PLAYER) $(EXE_PLAYER2) $(EXE_PLAYER3) $(EXE_PLAYER4) $(EXE_VIEW)