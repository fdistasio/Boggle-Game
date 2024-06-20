#ifndef PROTOCOL_HEADER

#define PROTOCOL_HEADER

#include "macros.h"
#include "costants.h"

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>



// Costanti relative al campo type del protocollo

#define MSG_OK 'K'

#define MSG_ERR 'E'

#define MSG_REGISTRA_UTENTE 'R'

#define MSG_MATRICE 'M'

#define MSG_TEMPO_PARTITA 'T'

#define MSG_TEMPO_ATTESA 'A'

#define MSG_PAROLA 'W'

#define MSG_PUNTI_FINALI 'F'

#define MSG_PUNTI_PAROLA 'P'

#define MSG_AIUTO 'H'

#define MSG_FINE 'Z'



// Struttura per il protocollo

typedef struct {

    unsigned int length;        // Lunghezza del campo data
    char type;                  // Tipologia del messaggio
    char* data;                 // Contenuto del messaggio

} message;



// Funzioni per la ricezione ed invio dei messaggi

int read_socket(int socketFD, message* receive);

void write_socket(int socketFD, message send);

void init_message(message* toInit, unsigned int length, char type, char* data);

void free_message(message* toFree);



#endif