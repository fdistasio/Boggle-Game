# Regole Phony
.PHONY: clean all


# Definizione dei file relativi al server
SERVER_OBJECTS = src/paroliere_srv.o src/server.o src/paroliere.o src/protocol.o


# Definizione dei file relativi al client
CLIENT_OBJECTS = src/paroliere_cl.o src/client.o src/protocol.o


# Compilatore
CC = gcc


# Flags di compilazione
CFLAGS = -Wall -pedantic -ggdb3 -pthread


# Regola di compilazione per gli eseguibili finali
all: paroliere_srv paroliere_cl


# Regola di compilazione del protocollo
src/protocol.o: src/protocol.c include/protocol.h include/macros.h include/costants.h
	$(CC) $(CFLAGS) -c $< -o $@


# Regole di compilazione del server

paroliere_srv: $(SERVER_OBJECTS)
	$(CC) $^ -o $@

src/server.o: src/server.c include/server.h include/macros.h include/protocol.h include/costants.h include/paroliere.h
	$(CC) $(CFLAGS) -c $< -o $@

src/paroliere.o: src/paroliere.c include/paroliere.h include/macros.h
	$(CC) $(CFLAGS) -c $< -o $@
 

# Regole di compilazione del client

paroliere_cl: $(CLIENT_OBJECTS)
	$(CC) $^ -o $@

src/client.o: src/client.c include/client.h include/macros.h include/protocol.h include/costants.h
	$(CC) $(CFLAGS) -c $< -o $@


# Pulizia dei file oggetto e dell'eseguibile

clean:
	rm src/*.o
	rm paroliere_srv
	rm paroliere_cl


# Avvio client e server con parametri di default

srv:
	./paroliere_srv 127.0.0.1 8000

cl:
	./paroliere_cl 127.0.0.1 8000

# Test con parametri personalizzati

srv_all_params: 
	./paroliere_srv 127.0.0.1 8000 --matrici data/matrix.txt --durata 1 --seed 22

srv_durata0:
	./paroliere_srv 127.0.0.1 8000 --matrici data/matrix.txt --durata 0 --seed 22

srv_empty_diz:
	./paroliere_srv 127.0.0.1 8000 --matrici data/matrix.txt --durata 1 --seed 22 --diz data/dictionary_custom.txt

srv_empty:
	./paroliere_srv

cl_empty:
	./paroliere_cl