# Variables
CC = gcc
CFLAGS = -Wall -g -fsanitize=address
OBJ_MASTER = master.o
OBJ_PLAYER = player.o
OBJ_VIEW = view.o

# Regla principal
all: $(OBJ_MASTER) $(OBJ_PLAYER) $(OBJ_VIEW)
    
$(OBJ_MASTER): master.c
	$(CC) $(CFLAGS) master.c -o $(OBJ_MASTER) 

$(OBJ_PLAYER): player.c
	$(CC) $(CFLAGS) player.c -o $(OBJ_PLAYER) 

$(OBJ_VIEW): view.c
	$(CC) $(CFLAGS) view.c -o $(OBJ_VIEW) 

# Regla para ejecutar el programa con los parámetros adecuados
run: all
	./$(OBJ_MASTER) -v ./$(OBJ_VIEW) -p ./$(OBJ_PLAYER)

# Limpieza de archivos compilados
clean:
	rm -f *.o


