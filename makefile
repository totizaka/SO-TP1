# Variables
CC = gcc
CFLAGS = -Wall -g -fsanitize=address
OBJ_MASTER = master.o
OBJ_PLAYER = player.o
OBJ_VIEW = view.o
OBJ_GAME = game_structs.o


# Regla principal
all: $(OBJ_GAME) $(OBJ_MASTER) $(OBJ_PLAYER) $(OBJ_VIEW)
    

# Compilar game_structs.c
$(OBJ_GAME): game_structs.c game_structs.h
	$(CC) $(CFLAGS) -c game_structs.c -o game_structs.o


# Compilar master.c
$(OBJ_MASTER): master.c game_structs.h $(OBJ_GAME)
	$(CC) $(CFLAGS) -c master.c -o master.o 

# Compilar player.c
$(OBJ_PLAYER): player.c game_structs.h $(OBJ_GAME)
	$(CC) $(CFLAGS) -c player.c -o player.o 

# Compilar view.c
$(OBJ_VIEW): view.c game_structs.h $(OBJ_GAME)
	$(CC) $(CFLAGS) -c view.c -o view.o 

# Enlazar los ejecutables
master: $(OBJ_MASTER) $(OBJ_GAME)
	$(CC) $(CFLAGS) master.o game_structs.o -o master

player: $(OBJ_PLAYER) $(OBJ_GAME)
	$(CC) $(CFLAGS) player.o game_structs.o -o player

view: $(OBJ_VIEW) $(OBJ_GAME)
	$(CC) $(CFLAGS) view.o game_structs.o -o view

# Ejecutar el programa
run: master player view
	./master -v ./view -p ./player

# Limpieza de archivos compilados
clean:
	rm -f *.o master player view


