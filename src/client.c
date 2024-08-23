#include "../include/client.h"



// Thread: invia i dati letti dall'utente verso il server
void* thread_write_server(void* socketFD) {

    int returnValue;
    char fromUser[BUF_SIZE];                            // Buffer di lettura da stdin
    char* prompt = "[PROMPT PAROLIERE]--> ";            // Prompt di avvio

    // Casting e assegnamento del Socket File Descriptor
    int clientSocketFD = *((int*) socketFD); 

    // Deallocazione del parametro "socketFD"
    free(socketFD);

    // Pulizia del buffer di input 
    clean_buffer(BUF_SIZE, fromUser);

    while(1) {

        // Scrittura del prompt su stdout
        SYSC(returnValue, write(STDOUT_FILENO, prompt, sizeof(char) * strlen(prompt)), "Write user prompt Error");
        
        // Lettura da stdin della richiesta dell'utente
        SYSC(returnValue, read(STDIN_FILENO, fromUser, BUF_SIZE), "Read user Error");

        // Invio della richiesta dell'utente al server
        send_user_request(clientSocketFD, fromUser);

        // Pulizia del buffer di input
        clean_buffer(BUF_SIZE, fromUser);

        // Attende per sincronizzare la visualizzazione del prompt e la risposta dal server sullo standard output
        sleep(1);

    }
    
}



// Invia il messaggio proposto dall'utente verso il server
void send_user_request(int socketFD, char fromUser[]) {

    char *request;                                                                              // Parola chiave della richiesta
    char *requestData;                                                                          // Dati che seguono la parola chiave
    char requestType;                                                                           // Tipo della richiesta
    char *reqKeys[] = {"registra_utente", "p", "matrice", "punti_finali", "aiuto", "fine"};     // Parole chiave

    // Elimina '\n' da "fromUser"
    del_endl(fromUser);

    // Memorizza in "request" la parola chiave della richiesta (es. "matrice")
    request = strtok(fromUser, " ");

    // Termina se "request" e' null
    if(!request)
        return;

    // Memorizza in "requestType" il tipo della richiesta
    requestType = get_request_type(request, reqKeys);

    // In base alla richiesta legge o meno il campo data ed invia il messaggio al server
    switch(requestType) {
        
        // Legge data e registra l'utente
        case MSG_REGISTRA_UTENTE:
            
            // Assegnazione a requestData dello username proposto
            requestData = strtok(NULL, " ");
            
            // Se il secondo campo e' definito il comando e' valido, altrimenti stampa la lista dei comandi validi
            if(requestData)
                
                register_user(socketFD, requestData);

            else 
                print_help(reqKeys);

            break;

        // Legge data ed invia la parola
        case MSG_PAROLA:
            
            // Assegnazione a requestData della parola proposta
            requestData = strtok(NULL, " ");
                                    
            // Se il secondo campo e' definito il comando e' valido, altrimenti stampa la lista dei comandi validi
            if(requestData)
                
                send_word(socketFD, requestData);
            
            else 
                print_help(reqKeys);

            break;

        // Richiede la matrice
        case MSG_MATRICE:

            print_matrix(socketFD);

            break;

        // Richiede i punti finali
        case MSG_PUNTI_FINALI:

            print_results(socketFD);
        
            break;

        // Richiede la lista dei comandi (non invia un messaggio al server)
        case MSG_AIUTO:

            print_help(reqKeys);
        
            break;

        // Esce dal gioco (non invia un messaggio al server)
        case MSG_FINE:
          
            exit_game(socketFD);
          
            break;

        // In caso di comando non valido stampa la lista dei comandi (non invia un messaggio al server)
        case MSG_ERR:
           
            print_help(reqKeys);
           
            break;

    }

}



// Restituisce il tipo di richiesa in base alla richiesta
char get_request_type(char* request, char* requestKeys[]) {

    int keyNum = 6;     // Numero di parole chiave

    // Confronto della richiesta "request" con ogni key di richiesta possibile
    for(int i = 0 ; i < keyNum ; i++) {

        // Se request e' uguale alla chiave i-esima, in base alla posizione della chiave, restituisce il tipo di richiesta
        if(!strcmp(request, requestKeys[i])) {

            switch(i) {

                case 0:
                    return MSG_REGISTRA_UTENTE;
                case 1:
                    return MSG_PAROLA;
                case 2:
                    return MSG_MATRICE;
                case 3:
                    return MSG_PUNTI_FINALI;
                case 4:
                    return MSG_AIUTO;
                case 5:
                    return MSG_FINE;

            }

        }
    }

    // Altrimenti restituisce il tipo errore
    return MSG_ERR;

}



// Invia al server la richiesta di registrazione
void register_user(int socketFD, char* username) {

    int returnValue;
    message send;                                                                          // Struttura per il messaggio da inviare
    char *allowedChars = "ABCDEFGHIJKLMNOPQRSTUVZabcdefghijklmnopqrstuvxyz0123456789";     // Caratteri permessi nello username
    int allowedCharsLength = strlen(allowedChars);                                         // Lunghezza della stringa dei caratteri permessi
    int usernameLength = strlen(username);                                                 // Lunghezza dello username proposto
    int equal = 0;                                                                         // Valore che indica la validità di un carattere
                        
    // Ogni lettera dello username viene confrontata con ogni lettera dell'alfabeto
    for(int i = 0 ; i < usernameLength ; i++) {

        equal = 0;

        for(int j = 0 ; j < allowedCharsLength ; j++) {

            if(username[i] == allowedChars[j]) 
                equal ++;

        }
        
        // Se almeno un carattere non e' presente nell'alfabeto, lo username non e' valido e non invia il messaggio al server
        if(!equal) {

            // Invio di un messaggio su stdout
            char * errorStr = "\nCaratteri username non validi\n\n";
            SYSC(returnValue, write(STDOUT_FILENO, errorStr, sizeof(char) * strlen(errorStr)), "Write username Error");
            return;

        }
        
    }

    // A questo punto lo username e' valido, altrimenti la funzione sarebbe terminata

    // Inizializzazione ed invio del messaggio
    init_message(&send, usernameLength, MSG_REGISTRA_UTENTE, username);
    write_socket(socketFD, send);

    // Deallocazione del campo data
    free_message(&send);

}



// Invia al server la parola proposta
void send_word(int socketFD, char* word) {

    int wordLength = strlen(word);              // Lunghezza della parola proposta
    message send;                               // Struttura per il messaggio da inviare

    // Inizializzazione ed invio del messaggio
    init_message(&send, wordLength, MSG_PAROLA, word);
    write_socket(socketFD, send);

    // Deallocazione del campo data
    free_message(&send);

}



// Invia al server la richiesta di visualizzazione della matrice
void print_matrix(int socketFD) {

    message send;               // Struttura per il messaggio da inviare
    
    // Inizializzazione ed invio del messaggio
    init_message(&send, 0, MSG_MATRICE, "");
    write_socket(socketFD, send);

}



// Invia al server la richiesta di visualizzazione dei risultati
void print_results(int socketFD) {

    message send;               // Struttura per il messaggio da inviare
    
    // Inizializzazione ed invio del messaggio
    init_message(&send, 0, MSG_PUNTI_FINALI, "");
    write_socket(socketFD, send);

}



// Scrive sullo stdout la lista dei comandi disponibili
void print_help(char* commands[]) {

    int returnValue;
    int commandsNum = 6;                                            // Numero dei comandi disponibili
    char toPrint[BUF_SIZE] = "\nI comandi disponibili sono:\n";     // Stringa da scrivere su stdout
    
    // Creazione della stringa dei comandi disponibili
    for(int i = 0 ; i < commandsNum ; i++) {

        // Concatenazione di "toPrint" con ogni comando disponibile. I comandi sono separati da virgola.
        strcat(toPrint, commands[i]);

        // Omissione della virgola nell'ultimo elemento
        if(i != (commandsNum - 1)) strcat(toPrint, ", ");

    }

    strcat(toPrint, "\n\n");

    // Scrittura su stdout
    SYSC(returnValue, write(STDOUT_FILENO, toPrint, sizeof(char) * strlen(toPrint)), "Write help Error");

}



// Termina la partita inviando al server un messaggio "MSG_FINE" e terminando il thread (il socket verra' chiuso dal thread che legge dal server)
void exit_game(int socketFD) {

    message send;               // Struttura per il messaggio da inviare
    
    // Inizializzazione ed invio del messaggio
    init_message(&send, 0, MSG_FINE, "");
    write_socket(socketFD, send);

    // Terminazione thread
    pthread_exit(NULL);

}



// Elimina '\n' sostituendolo con '\0'
void del_endl(char str[]) {

    int length = strlen(str);   // Lunghezza di "str"
    str[length - 1] = '\0';     // Rimozione '\n' tramite '\0'

}



// Pulisce il buffer settando gli elementi a '\0'
void clean_buffer(int size, char buf[]) {

    // Ogni elemento del buffer e' sostituito con '\0'
    for(int i = 0 ; i < size ; i++)
        buf[i] = '\0';

}



// Controlla che il numero di parametri da linea di comando sia corretto
void check_params(int argc, int paramsNum) {
    
    // Se "argc" e' diverso dal numero richiesto di parametri "paramsNum" lancia un errore
    if(argc != paramsNum) {
        perror("Parameters Error");
        exit(errno);
    }

}



// Inizializza i parametri tramite "argv[]"
void init_params(int argc, char* argv[], char serverName[], int* serverPort) {
    
    // Inizializzazione nome del server
    strcpy(serverName, argv[1]);

    // Inizializzazione porta del server
    *serverPort = atoi(argv[2]);

}



// Thread: legge dati dal server
void* thread_read_server(void* socketFD) {

    message receive;                            // Struttura per il messaggio da riceverer
    int clientSocketFD = *((int*) socketFD);    // Casting del Socket File Descriptor

    // Deallocazione del parametro "socketFD"
    free(socketFD);

    // Finche' il client e' connesso
    while(1) {
        
        // Lettura dei dati presenti sul socket
        read_socket(clientSocketFD, &receive);

        // Gestione della risposta del server
        handle_server_response(receive, clientSocketFD);

        // Deallocazione campo data se presente
        free_message(&receive);

    }

}



// Gestisce la risposta inviata dal server
void handle_server_response(message response, int socketFD) {

    char responseType = response.type;      // Tipo della risposta

    // Gestione della risposta: in base al tipo di essa viene chiamata una funzione associata
    switch (responseType) {
    
    case MSG_OK:
        handle_ok_err(response);
        break;
    
    case MSG_ERR:
        handle_ok_err(response);
        break;

    case MSG_MATRICE:
        handle_matrix(response);
        break;

    case MSG_TEMPO_PARTITA:
        handle_game_time(response);
        break;

    case MSG_TEMPO_ATTESA:
        handle_game_time(response);
        break;

    case MSG_PUNTI_FINALI:
        handle_final_score(response);
        break;

    case MSG_PUNTI_PAROLA:
        handle_word_points(response);
        break;

    // Chiusura della connessione in caso di messaggio non valido oppure "MSG_FINE"
    default:
        handle_exit(response, socketFD);
        break;

    }

}



// Gestisce risposte del server di tipo "MSG_OK" e "MSG_ERR"
void handle_ok_err(message response) {

    int returnValue;
    char responseType = response.type;                          // Tipo della risposta
    char *okResponse = "\n\nRichiesta andata a buon fine.\n";   // Stringa standard da scrivere su stdout per "MSG_OK"
    char *errResponse = "\n\nRichiesta fallita.\n";             // Stringa standard da scrivere su stdout per "MSG_ERR"

    // Se il tipo della risposta e' "MSG_OK"
    if(responseType == MSG_OK) {
        
        // Scrittura su stdout della risposta base per "MSG_OK"
        SYSC(returnValue, write(STDOUT_FILENO, okResponse, sizeof(char) * strlen(okResponse)), "Error write response");
       
    } else {
        
        // Altrimenti se il tipo della risposta e' "MSG_ERR"

        // Scrittura su stdout della risposta base per "MSG_ERR"
        SYSC(returnValue, write(STDOUT_FILENO, errResponse, sizeof(char) * strlen(errResponse)), "Error write response");

    }

    // Scrittura su stdout del campo data, contenente in dettaglio il motivo dell'esito, se definito
    if(response.length)

        SYSC(returnValue, write(STDOUT_FILENO, response.data, sizeof(char) * response.length), "Error write response");

}



// Gestisce risposte del server di tipo "MSG_MATRICE"
void handle_matrix(message response) {
    
    // Stampa la matrice effettuando fflush del buffer stdout (alternativa a "write()" dato le varie printf utilizzate)
    print_str_to_matrix(MATRIX_ROWS, MATRIX_COLUMNS, response.data);
    fflush(stdout);

}



// Stampa sullo standard output la stringa "toMatrix" come se fosse una matrice
void print_str_to_matrix(int rows, int columns, char toMatrix[]) {

    char *token;                       // Elemento corrente di "toMatrix"

    // Stampa dei separatori iniziali
    printf("\n\n");
    print_separators(columns);
    printf("\n");

    // Stampa del primo elemento
    token = strtok(toMatrix, " ");

    // Gestione della stringa "Qu", se l'elemento corrente e' Qu non viene stampato uno spazio così da non disallineare le celle
    if(strcmp(token, "Qu") == 0) {

        printf("| %s| ", token);

    } else {   

        // Altrimenti l'elemento e' preceduto e seguito da spazio
        printf("| %s | ", token);

    }
    
    // Stampa di ogni elemento seguito da una riga di separatori
    for(int i = 0 ; i < rows ; i++) {

        for(int j = 0 ; j < columns ; j++) {
            
            // Salta il primo elemento
            if(i == 0 && j == columns - 1) break;
            
            // Elemento corrente
            token = strtok(NULL, " ");

            // Gestione della stringa "Qu", se l'elemento corrente e' Qu non viene stampato uno spazio così da non disallineare le celle
            if(strcmp(token, "Qu") == 0) {

                printf("| %s| ", token);

            } else {   
                
                // Altrimenti l'elemento e' preceduto e seguito da spazio
                printf("| %s | ", token);

            }
    
        }

        printf("\n");
        print_separators(columns);
        printf("\n");

    }

    printf("\n");

}



// Stampa separatori
void print_separators(int columns) {
    
    // Per ogni colonna stampa un separatore
    for(int j = 0 ; j < columns ; j++) 
        printf("+---+ ");

}



// Gestisce risposte del server di tipo "MSG_TEMPO_PARTITA" "MSG_TEMPO_ATTESA"
void handle_game_time(message response) {
    
    int returnValue;

    // Scrittura su stdout della risposta del server
    SYSC(returnValue, write(STDOUT_FILENO, response.data, sizeof(char) * strlen(response.data)), "Error write response");

}



// Gestisce risposte del server di tipo "MSG_PUNTI_FINALI"
void handle_final_score(message response) {
    
    int returnValue;
    
    // Scrittura su stdout della risposta del server
    SYSC(returnValue, write(STDOUT_FILENO, response.data, sizeof(char) * strlen(response.data)), "Error write response");

}



// Gestisce risposte del server di tipo "MSG_PUNTI_PAROLA"
void handle_word_points(message response) {
    
    int returnValue;

    // Scrittura su stdout della risposta del server
    SYSC(returnValue, write(STDOUT_FILENO, response.data, sizeof(char) * strlen(response.data)), "Error write response");
    
}



// Gestisce risposte del server di tipo "MSG_FINE"
void handle_exit(message response, int socketFD) {

    int returnValue;
    char *disconnectPrint = "Disconnessione...\n";		// Messaggio di disconnessione

    // Stampa della disconnessione su stdout
    SYSC(returnValue, write(STDOUT_FILENO, disconnectPrint, sizeof(char) * strlen(disconnectPrint)), "Error Write Connect");

    // Chiusura del socket
    SYSC(returnValue, close(socketFD), "Close socket Error");

    // Terminazione thread
    pthread_exit(NULL);

}