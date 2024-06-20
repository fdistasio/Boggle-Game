#include "../include/protocol.h"



// Legge dati dal socket e li memorizza nella struttura message "from". Restituisce -1 se una read() e' stata interrotta, 0 altrimenti
int read_socket(int socketFD, message* receive) {

    // Lettura della lunghezza e del tipo del messaggio
    if(read(socketFD, &receive->length, sizeof(unsigned int)) == -1) {
        
        // Se la read fallisce ma e' stata interrotta da un segnale (comportamento previsto lato server), restituisce -1, altrimenti restituisce errore
        if (errno == EINTR)

            return -1;

        perror("Read length Error"); 
        exit(errno);

    }

    if(read(socketFD, &receive->type, sizeof(char)) == -1) {

        // Se la read fallisce ma e' stata interrotta da un segnale (comportamento previsto lato server), restituisce -1, altrimenti restituisce errore
        if (errno == EINTR)

            return -1;

        perror("Read length Error"); 
        exit(errno);
         
    }

    // Se la lunghezza e' definita viene letto il campo data
    if(receive->length) {

        // Allocazione di data in base a length
        receive->data = (char*) malloc(sizeof(char) * ((receive->length) + 1));
        MALLOC_CHECK(receive->data);

        if(read(socketFD, receive->data, sizeof(char) * receive->length) == -1) {

            // Se la read fallisce ma e' stata interrotta da un segnale (comportamento previsto lato server), restituisce -1, altrimenti restituisce errore
            if (errno == EINTR) {

                // Deallocazione del campo data
                free(receive->data);
                return -1;

            }

            perror("Read length Error"); 
            exit(errno);
         
        }

        receive->data[receive->length] = '\0';

    }

    return 0;

}



// Scrive dati della struttura message "send" e li invia al socket
void write_socket(int socketFD, message send) {

    int returnValue;

    // Scrittura della lunghezza e del tipo del messaggio
    SYSC(returnValue, write(socketFD, &send.length, sizeof(unsigned int)), "Write length Error");
    SYSC(returnValue, write(socketFD, &send.type, sizeof(char)), "Write type Error");

    // Se la lunghezza e' definita allora viene scritto il campo data
    if(send.length)

        SYSC(returnValue, write(socketFD, send.data, sizeof(char) * send.length), "Write data Error");

}



// Inizializzazione della struttura message "toInit" tramite "length", "type" e "data"
void init_message(message* toInit, unsigned int length, char type, char* data) {

    toInit->length = length;        // Assegnamento della lunghezza alla struttura
    toInit->type = type;            // Assegnamento del tipo alla struttura
    
    // Se la lunghezza e' definita il campo data viene allocato
    if(length) {

        // Allocazione di data in base a length
        toInit->data = (char*) malloc(sizeof(char) * (length + 1));
        MALLOC_CHECK(toInit->data);

        // Assegnamento di data alla struttura
        strcpy(toInit->data, data);
        toInit->data[length] = '\0';

    } else
        
        // Altrimenti data e' inizializzato a NULL
        toInit->data = NULL;

}



// Dealloca il campo data della struttura message (se presente)
void free_message(message* toFree) {

    if(toFree->data != NULL) {

        free(toFree->data);
        toFree->data = NULL;
        
    }

}