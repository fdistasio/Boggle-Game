#include "../include/server.h"



// Inizializzazione variabili globali

pthread_t clientHandlerThreads[MAX_CONNECTED_CLIENTS] = {0};                // Array relativo ai tid dei thread che gestiscono gli utenti
int clientSocketFileDescriptors[MAX_CONNECTED_CLIENTS] = {0};               // Array relativo ai file descriptor dei socket dei client connessi
char board[MATRIX_ROWS][MATRIX_COLUMNS][MAX_ELEM_LENGTH];                   // Matrice di gioco
char clientsUsernames[MAX_CONNECTED_CLIENTS][MAX_USERNAME_SIZE] = {""};     // Array relativo agli username dei client connessi
int numRegisteredClients = 0;                                               // Numero dei client connessi e registrati
char ranking[RANK_STRING] = {""};                                           // Stringa relativa alla classifica di gioco
int inGame = 0;                                                             // Indica se il gioco e' in corso o meno (1 in corso, 0 altrimenti)
int wroteData = 0;                                                          // Indica se i dati devono essere scritti o meno sulla coda condivisa (1 in caso affermativo, 0 altrimenti)

pthread_mutex_t mutexClientHandlerThreads = PTHREAD_MUTEX_INITIALIZER;      // Mutex relativo a "clientHandlerThreads" e "clientSocketFileDescriptors"
pthread_mutex_t mutexBoard = PTHREAD_MUTEX_INITIALIZER;                     // Mutex relativo a "board"
pthread_mutex_t mutexClientsUsernames = PTHREAD_MUTEX_INITIALIZER;          // Mutex relativo a "clientsUsernames"
pthread_mutex_t mutexNumRegisteredClients = PTHREAD_MUTEX_INITIALIZER;      // Mutex relativo a "numClientsRegistered"
pthread_mutex_t mutexScoresQueue = PTHREAD_MUTEX_INITIALIZER;               // Mutex relativo alla coda dei punteggi (allocata dinamicamente)
pthread_mutex_t mutexRanking = PTHREAD_MUTEX_INITIALIZER;                   // Mutex relativo a "ranking"
pthread_mutex_t mutexInGame = PTHREAD_MUTEX_INITIALIZER;                    // Muter relativo a "inGame"
pthread_mutex_t mutexWroteData = PTHREAD_MUTEX_INITIALIZER;                 // Mutex relativo a "wroteData"
pthread_mutex_t mutexAlarm = PTHREAD_MUTEX_INITIALIZER;                     // Mutex relativo alla "risorsa" alarm()

pthread_cond_t notReadyRanking = PTHREAD_COND_INITIALIZER;                  // Variabile di condizione relativa "ranking"
pthread_cond_t notFullQueue = PTHREAD_COND_INITIALIZER;                     // Variabile di condizione relativa alla coda dei punteggi (allocata dinamicamente)



// Thread: scorer che gestisce la classifica dei punteggi
void* thread_scorer(void* args) {

    queue *scoresQueue;             // Coda condivisa dei punteggi

    // Casting e assegnamento della coda dei punteggi "scoresQueue"
    scoresQueue = (queue*)args;

    while(1) {

        // Lock del mutex relativo alla coda condivisa dei punteggi
        SYST(pthread_mutex_lock(&mutexScoresQueue));

        while(is_empty_queue(scoresQueue)) {

            // Attesa se la coda e' vuota
            SYST(pthread_cond_wait(&notFullQueue, &mutexScoresQueue));

        }

        // Assegnamento di 0 a "wroteData" per fare in modo che i dati non vengano riscritti nella stessa pausa
        write_flag(1, 0);

        // Array di appoggio di tipo "player" relativo alla coda "scoresQueue"
        int queueLength = get_queue_length(scoresQueue);
        player players[queueLength];

        // Copia della coda nell'array
        copy_score_queue(scoresQueue, players);

        // Dealloca gli elementi della coda
        destroy_queue(&scoresQueue);

        // Unlock del mutex
        SYST(pthread_mutex_unlock(&mutexScoresQueue));

        // Stila la classifica e notifica i thread in attesa quando e' pronta
        make_ranking(players, queueLength);

    }

}



// Copia la coda dei punteggi in un array di tipo "player"
void copy_score_queue(queue* scoreQueue, player players[]) {
    
    queue_node* head = scoreQueue->head;    // Puntatore alla testa della coda
    int i = 0;                              // Indice per scorrere "players"

    // Finche' ci sono elementi in coda
    while(head != NULL) {

        // Copia dei dati della coda nell'array
        players[i].score = head->score;
        strcpy(players[i].username, head->username);
        
        head = head->next;
        i++;

    }

}



// Stila la classifica e notifica i thread in attesa quando e' pronta
void make_ranking(player players[], int playersLength) {

    // Elemento corrente da appendere alla stringa della classifica di gioco
    char currentElemStr[FIXED_STRING_SIZE] = {""};

    // Ordinamento di "players" in ordine decrescente di punteggi
    qsort(players, playersLength, sizeof(player), cmp_players);

    // Lock del mutex relativo alla stringa della classifica di gioco
    SYST(pthread_mutex_lock(&mutexRanking));

    // Per ogni elemento dell'array "players" viene formattato il risultato ed aggiunto a "ranking" in formato CSV
    for(int i = 0 ; i < playersLength ; i++) {

        // Se e' l'ultimo elemento non viene aggiunta l'ultima virgola
        if(i == (playersLength - 1)) {
            
            // Formattazione della stringa che rappresenta l'elemento corrente
            sprintf(currentElemStr, "%s, %d", players[i].username, players[i].score);
            
            // Concatenazione alla stringa della classifica di gioco
            strcat(ranking, currentElemStr);
            break;

        }

        // Formattazione della stringa che rappresenta l'elemento corrente
        sprintf(currentElemStr, "%s, %d, ", players[i].username, players[i].score);
        
        // Concatenazione alla stringa della classifica di gioco
        strcat(ranking, currentElemStr);
        
    }

    // Segnala tutti i thread che aspettano la classifica
    SYST(pthread_cond_broadcast(&notReadyRanking));

    // Unlock del mutex
    SYST(pthread_mutex_unlock(&mutexRanking));

}



// Confronta elementi di un array di tipo "player" (funzione utilizzata per qsort())
int cmp_players(const void* playerA, const void* playerB) {

    // Casting e assegnamento dei parametri
    player pA = *((player*) playerA);
    player pB = *((player*) playerB);

    // Confronto in base ai punteggi
    return pB.score - pA.score;

}



// Thread: arbitro che gestisce la generazione di matrici e le pause di gioco
void *thread_arb(void *args) {

    int returnValue;
    int generateFrom;                               // Indica come inizializzare la matrice. Se 0 random, altrimenti da file
    char matrixFileName[MAX_PARAM_SIZE];            // Nome del file delle matrici
    FILE *matrixFile;                               // File delle matrici
    int durata;                                     // Durata delle partite (espressa in secondi)
    sigset_t signalSet;                             // Set dei segnali
    int sig;                                        // Segnale bloccato da sigwait
    
    // Casting e assegnazione di "generateFrom"
    generateFrom = ((arb_args*)args)->generateFrom;

    // Casting e assegnazione di "matrixFileName"
    strcpy(matrixFileName, ((arb_args*)args)->matrixFileName);

    // Casting, assegnazione e conversione in secondi di "durata";
    durata = 60 * (((arb_args*)args)->durata);

    // Deallocazione dei parametri di funzione
    free(args);

    // Azzeramento del set (per sigwait()) e aggiunta di "SIGALRM"
    SYSC(returnValue, sigemptyset(&signalSet), "Error signal set");
    SYSC(returnValue, sigaddset(&signalSet, SIGALRM), "Error signal set");

    // Inizializzazione tramite file di matrici se "generateFrom" = 1, altrimenti in modo random
    if(generateFrom) {
        
        // Apertura file delle matrici in lettura
        matrixFile = fopen(matrixFileName, "r");
        FILE_CHECK(matrixFile);

        // Inizializza "board" da file gestendo i tempi di gioco tramite alarm() e sigwait()
        while(1) {
            
            // Lock del mutex relativo alla matrice di gioco
            SYST(pthread_mutex_lock(&mutexBoard));

            // Inizializzazione da file
            init_file_board_matrix(matrixFile, MATRIX_ROWS, MATRIX_COLUMNS, board);

            // Unlock del mutex
            SYST(pthread_mutex_unlock(&mutexBoard));

            // Gestione delle pause
            timer_scheduler(durata, &signalSet, &sig);

        }

    } else {

        // Inizializza "board" in modo random gestendo tempi di gioco tramite alarm() e sigwait()
        while(1) {

            // Lock del mutex relativo alla matrice di gioco
            SYST(pthread_mutex_lock(&mutexBoard));

            // Inizializzazione random
            init_rand_board_matrix(MATRIX_ROWS, MATRIX_COLUMNS, board);

            // Unlock del mutex
            SYST(pthread_mutex_unlock(&mutexBoard));

            // Gestione delle pause
            timer_scheduler(durata, &signalSet, &sig);

        }

    }

}



// Inizializza la struttura dei parametri del thread arbitro
void init_arb_args(arb_args* args, int generateFrom, char matrixFile[], int durata) {

    args->generateFrom = generateFrom;
    args->durata = durata;
    strcpy(args->matrixFileName, matrixFile);

}



// Programma i tempi di gioco e di pausa tramite alarm() e sigwait()
void timer_scheduler(int durata, sigset_t* signalSet, int* sig) {

    int returnValue;

    int pauseSeconds = 60;                          // Secondi di pausa
    char *pauseString = "Avvio della pausa\n";      // Stringa da stampare su stdout per notificare la pausa
    char *startString = "Avvio della partita\n";    // Stringa da stampare su stdout per notificare l'avvio di una partita

    // Scrittura su stdout dell'avvio della pausa
    SYSC(returnValue, write(STDOUT_FILENO, pauseString, sizeof(char) * strlen(pauseString)), "Error Write pause");

    // Lock del mutex relativo ad alarm
    SYST(pthread_mutex_lock(&mutexAlarm));

    // Avvio della pausa da "pauseSeconds" secondi
    alarm(pauseSeconds);

    // Unlock del mutex relativo ad alarm
    SYST(pthread_mutex_unlock(&mutexAlarm));

    // Attesa di "SIGALARM"
    SYSC(returnValue, sigwait(signalSet, sig), "Error sigwait");
    
    // Aggiornamento dello stato di gioco in corso (inGame = 1)
    write_flag(0, 1);

    // Aggiornamento di "wroteData" ad 1 per fare in modo che i dati vengano scritti nella pausa
    write_flag(1, 1);

    // Lock del mutex relativo alla stringa della classifica di gioco
    SYST(pthread_mutex_lock(&mutexRanking));

    // Reset della stringa della classifica di gioco
    strcpy(ranking, "");

    // Unlock del mutex
    SYST(pthread_mutex_unlock(&mutexRanking));
    
    // Scrittura su stdout dell'avvio della partita
    SYSC(returnValue, write(STDOUT_FILENO, startString, sizeof(char) * strlen(startString)), "Error Write start");

    // Lock del mutex relativo ad alarm
    SYST(pthread_mutex_lock(&mutexAlarm));

    // Avvio della partita da "durata" secondi
    alarm(durata);

    // Unlock del mutex relativo ad alarm
    SYST(pthread_mutex_unlock(&mutexAlarm));

    // Attesa di "SIGALARM"
    SYSC(returnValue, sigwait(signalSet, sig), "Error sigwait");

    // Aggiornamento dello stato di gioco in pausa (inGame = 0)
    write_flag(0, 0);

    // Segnalazione, a tutti i thread che gestiscono gli utenti, di scrivere nella coda condivisa
    signal_client_handlers();

}



// Thread: gestisce la connessione di un client
void* thread_handle_connection(void* args) {

    int returnValue;
    int clientSocketFD;                                             // File descriptor del socket relativo al client
    char dictionaryFileName[MAX_PARAM_SIZE];                        // Nome del file dizionario
    FILE * dictionaryFile;                                          // File dizionario
    message receive;                                                // Struttura per la ricezione del messaggio
    queue *scoresQueue;                                             // Coda condivisa dei punteggi
    linked_list_node *guessedWords;                                 // Linked list delle parole proposte

    char *connectPrint = "Client Connesso\n";                       // Stringa per la stampa di una connessione avvenuta
    char username[MAX_USERNAME_SIZE] = {""};                        // Username client
    int isRegistered = 0;                                           // Indica se l'utente e' registrato o meno (0 se non lo e')
    int clientScore = 0;                                            // Punteggio del client

    // Casting e assegnazione del Socket File Descriptor
    clientSocketFD = ((client_handler_args*)args)->clientSocketFD;

    // Casting e assegnazione del nome del file dizionario
    strcpy(dictionaryFileName, ((client_handler_args*)args)->dictionaryFileName);

    // Casting e assegnazione del puntatore alla coda dei punteggi
    scoresQueue = ((client_handler_args*)args)->scoresQueue;

    // Deallocazione dei parametri di funzione
    free(args);
    
    // Stampa della connessione avvenuta su stdout
    SYSC(returnValue, write(STDOUT_FILENO, connectPrint, sizeof(char) * strlen(connectPrint)), "Error Write Connect");

    // Apertura del file dizionario in lettura
    dictionaryFile = fopen(dictionaryFileName, "r");
    FILE_CHECK(dictionaryFile);

    // Allocazione della linked list delle parole proposte
    guessedWords = (linked_list_node*) (sizeof(linked_list_node));
    MALLOC_CHECK(guessedWords);
    guessedWords = NULL;

    // Finche' il client e' connesso
    while(1) {

        // Reset del valore di ritorno della "read_socket()"
        returnValue = 0;

        // Se il gioco non e' in corso (inGame == 0), il thread non ha scritto i dati sulla coda (wroteData == 1), ed e' registrato, procede per scrivere dati sulla coda condivisa
        if(!get_flag(0) && get_flag(1) && isRegistered)

            handle_game_end(clientSocketFD, &scoresQueue, &clientScore, username, &guessedWords, isRegistered);
        
        // Lettura dei dati presenti sul socket.
        returnValue = read_socket(clientSocketFD, &receive);

        // Gestione ed invio della risposta (solo se la lettura non e' stata interrotta)
        if(returnValue != -1)

              handle_client_request(receive, clientSocketFD, username, &isRegistered, &clientScore, dictionaryFile, &guessedWords);

        // Deallocazione campo data se presente
        free_message(&receive);

    }

}



// Inizializza la struttura dei parametri del thread che gestisce i client
void init_client_handler_args(client_handler_args* args, int socketFD, char dictionaryFile[], queue* scoresQueue) {

    args->scoresQueue = scoresQueue;
    args->clientSocketFD = socketFD;
    strcpy(args->dictionaryFileName, dictionaryFile);

}



// Gestisce la richiesta del client in base al tipo di essa
void handle_client_request(message receive, int socketFD, char username[], int* isRegistered, int* clientScore, FILE* dictionaryFile, linked_list_node** guessedWords) {

    // Tipo della risposta
    char receiveType = receive.type;

    // Gestione della richiesta in base al tipo di essa
    switch (receiveType) {
    
    case MSG_REGISTRA_UTENTE:
        handle_registration(receive, socketFD, username, isRegistered);
        break;

    case MSG_PAROLA:
        handle_guess(receive, socketFD, clientScore, dictionaryFile, guessedWords, isRegistered);
        break;

    case MSG_MATRICE:
        handle_matrix_and_time(socketFD, (*isRegistered));
        break;

    case MSG_PUNTI_FINALI:
        handle_final_score(socketFD, (*isRegistered));
        break;

    // Chiusura della connessione in caso di "MSG_FINE"
    case MSG_FINE:

        // Invio di "MSG_FINE" al client e chiusura della connessione
        handle_exit(socketFD, username, (*isRegistered), dictionaryFile, guessedWords);
        break;

    // In caso di errori effettua return non facendo niente
    default:
    
        return;

    }

}



// Gestisce richieste del client di tipo "MSG_REGISTRA_UTENTE"
void handle_registration(message receive, int socketFD, char username[], int* isRegistered) {

    message response;                                               // Struttura messaggio di risposta

    char *alreadyRegistered = "\nSei gia' registrato\n\n";          // Stringa per comunicare che l'utente e' già registrato
    char *lengthError = "\nUsername troppo lungo\n\n";              // Stringa per comunicare errore sulla lunghezza
    char *alreadUsed = "\nUsername gia' utilizzato\n\n";            // Stringa per comunicare username già utilizzato
    char *registered = "\nUtente registrato correttamente\n\n";     // Stringa per comunicare la registrazione avvenuta correttamente

    // Invio di "MSG_ERR" se l'utente e' già registrato
    if(*isRegistered) {
        
        // Inizializzazione ed invio del messaggio
        init_message(&response, strlen(alreadyRegistered), MSG_ERR, alreadyRegistered);
        write_socket(socketFD, response);

        // Deallocazione del campo data
        free_message(&response);

        return;

    }

    // Invio di "MSG_ERR" se lo username e' troppo lungo
    if(receive.length > MAX_USERNAME_SIZE) {

        // Inizializzazione ed invio del messaggio
        init_message(&response, strlen(lengthError), MSG_ERR, lengthError);
        write_socket(socketFD, response);

        // Deallocazione del campo data
        free_message(&response);

        return;

    }

    // Invio di "MSG_ERR" se lo username e' già utilizzato
    if(is_user_registered(receive.data)) {

        // Inizializzazione ed invio del messaggio
        init_message(&response, strlen(alreadUsed), MSG_ERR, alreadUsed);
        write_socket(socketFD, response);

        // Deallocazione del campo data
        free_message(&response);

        return;

    }

    // Registrazione del nome utente nell'array globale "clientsUsernames" e aumento del numero di client registrati "numRegisteredClients"
    register_user(receive.data);

    // Copia del nome utente contenuto in "receive.data" in "username"
    strcpy(username, receive.data);

    // Impostazione dell'utente come registrato
    *isRegistered = 1;

    //Invio di "MSG_OK"

    // Inizializzazione ed invio del messaggio
    init_message(&response, strlen(registered), MSG_OK, registered);
    write_socket(socketFD, response);

    // Deallocazione campo data
    free_message(&response);

    // Invio di "MSG_MATRICE" e "MSG_TEMPO_PARTITA" se il gioco e' in corso, altrimenti "MSG_TEMPO_ATTESA"
    handle_matrix_and_time(socketFD, (*isRegistered));

}



// Invia il tempo rimanente alla fine della partita oppure il tempo restante alla prossima partita
void send_time(int socketFD) {

    message response;                           // Struttura messaggio di risposta
    int timeToSend;                             // Tempo partita o attesa da comunicare
    char timeStr[SMALL_BUF_SIZE];               // Stringa contenente il tempo da inviare

    // Lock del mutex relativo ad alarm
    SYST(pthread_mutex_lock(&mutexAlarm));

    // Tempo rimanente/restante
    timeToSend = alarm(0);

    // Set della alarm con il tempo che gli rimaneva da far passare (dato che alarm(0) la annulla)
    alarm(timeToSend);

    // Unlock del mutex relativo ad alarm
    SYST(pthread_mutex_unlock(&mutexAlarm));

    // Invio di "MSG_TEMPO_PARTITA" se "inGame" = 1, altrimenti invio di "MSG_TEMPO_ATTESA"
    if(get_flag(0)) {

        // Stringa da inviare
        sprintf(timeStr, "\nLa partita terminera' tra %d secondi\n\n", timeToSend);

        // Inizializzazione ed invio del messaggio
        init_message(&response, strlen(timeStr), MSG_TEMPO_PARTITA, timeStr);
        write_socket(socketFD, response);

        // Deallocazione campo data
        free_message(&response);

    } else {

        // Stringa da inviare
        sprintf(timeStr, "\n\nLa partita iniziera' tra %d secondi\n\n", timeToSend);

        // Inizializzazione ed invio del messaggio
        init_message(&response, strlen(timeStr), MSG_TEMPO_ATTESA, timeStr);
        write_socket(socketFD, response);

        // Deallocazione campo data
        free_message(&response);

    }

}



// Controlla se un nome utente e' già registrato in "clientsUsernames", in caso lo fosse restituisce 1, altrimenti 0
int is_user_registered(char* username) {

    int found = 0;                  // Indica se lo username e' stato trovato

    // Lock del mutex relativo alla risorsa condivisa dei nomi utenti "clienstUsernames"
    SYST(pthread_mutex_lock(&mutexClientsUsernames));
    
    // Se almeno un campo di "clientsUsernames" e' uguale a "username", "found" e' impostato ad 1
    for(int i = 0 ; i < MAX_CONNECTED_CLIENTS ; i++)

        if(strcmp(username, clientsUsernames[i]) == 0)
            found = 1;

    // Unlock del mutex
    SYST(pthread_mutex_unlock(&mutexClientsUsernames));

    return found;

}



// Registra un utente inserendo lo username in "clientsUsernames" e aumentando il numero di client registrati "numRegisteredClients"
void register_user(char* username) {

    // Incremento del numero di client registrati
    registered_clients_number_update(0);

    // Lock del mutex relativo alla risorsa condivisa dei nomi utenti "clientsUsernames"
    SYST(pthread_mutex_lock(&mutexClientsUsernames));
    
    // Inserimento dello username nel primo slot vuoto di "clientsUsernames"
    for(int i = 0 ; i < MAX_CONNECTED_CLIENTS ; i++)

        if(strcmp("", clientsUsernames[i]) == 0) {

            strcpy(clientsUsernames[i], username);
            break;

        }

    // Unlock del mutex
    SYST(pthread_mutex_unlock(&mutexClientsUsernames));

}



// Cancella la registrazione di un client da "clientsUsernames"
void unregister_user(char* username) {

    // Decremento del numero di client registrati da "numRegisteredClients"
    registered_clients_number_update(1);

    // Lock del mutex relativo alla risorsa condivisa dei nomi utenti "clientsUsernames"
    SYST(pthread_mutex_lock(&mutexClientsUsernames));
    
    // Inserimento dello username nel primo slot vuoto di "clientsUsernames"
    for(int i = 0 ; i < MAX_CONNECTED_CLIENTS ; i++)

        if(strcmp(username, clientsUsernames[i]) == 0)
            strcpy(clientsUsernames[i], "");

    // Unlock del mutex
    SYST(pthread_mutex_unlock(&mutexClientsUsernames));

}



// Aggiorna il numero di client registrati, se operator e' 0 lo aumenta, altrimenti lo decrementa
void registered_clients_number_update(int operator) {

    // Lock del mutex relativo a "numRegisteredClients"
    SYST(pthread_mutex_lock(&mutexNumRegisteredClients));

    if(operator) 

        numRegisteredClients -= 1;

    else 
        numRegisteredClients += 1;	

    // Unlock del mutex
    SYST(pthread_mutex_unlock(&mutexNumRegisteredClients));

}



// Gestisce richieste del client di tipo "MSG_PAROLA"
void handle_guess(message receive, int socketFD, int* clientScore, FILE* dictionaryFile, linked_list_node** guessedWords, int* isRegistered) {

    message response;                                                               // Struttura del messaggio di risposta
    char pointsStr[FIXED_STRING_SIZE];                                              // Stringa rappresentante i punti 

    int inMatrix = 0;                                                               // Indica se la parola e' presente nella matrice
    int isCorrectGuess = 0;                                                         // Indica se la parola proposta e' corretta e possono essere assegnati i punti (1 se e' corretta, 0 altrimenti)
    int points = 0;                                                                 // Punti della parola corrente
    char *guess = receive.data;                                                     // Parola proposta
    char *notRegisteredStr = "\nDevi essere registrato per proporre parole.\n\n";   // Stringa per notificare che il client deve essere registrato per proporre parole
    char *notInGameStr = "\nIl gioco non e' in corso.\n\n";                         // Stringa per notificare che il gioco deve essere in corso per proporre parole

    // Invio di "MSG_ERR" se l'utente non e' registrato
    if(!(*isRegistered)) {
        
        // Inizializzazione ed invio del messaggio
        init_message(&response, strlen(notRegisteredStr), MSG_ERR, notRegisteredStr);
        write_socket(socketFD, response);

        // Deallocazione del campo data
        free_message(&response);

        return;

    }

    // Invio di "MSG_ERR" se il gioco non e' in corso (inGame == 0)
    if(!(get_flag(0))) {
        
        // Inizializzazione ed invio del messaggio
        init_message(&response, strlen(notInGameStr), MSG_ERR, notInGameStr);
        write_socket(socketFD, response);

        // Deallocazione del campo data
        free_message(&response);
        
        return;

    }

    // Lock del mutex relativo alla matrice di gioco
    SYST(pthread_mutex_lock(&mutexBoard));

    // Controllo della presenza della parola nella matrice
    inMatrix = word_checker(MATRIX_ROWS, MATRIX_COLUMNS, board, guess);

    // Unlock del mutex
    SYST(pthread_mutex_unlock(&mutexBoard));

    // Correttezza determinata da: presenza nella matrice, presenza nel dizionario, lunghezza minima
    isCorrectGuess = inMatrix && check_dictrionary(dictionaryFile, guess) && check_word_length(guess, MIN_WORD_LENGTH);

    // Se la parola e' corretta
    if(isCorrectGuess) {

        // Se e' stata gia' proposta dal client, vengono inviati 0 punti
        // Inoltre viene confrontata in lower case per evitare che parole uguali ma con caratteri in lower o upper case vengano considerate diverse
        if(check_linked_list(*guessedWords, to_lower_word(guess))) {
            
            // Formattazione della stringa dei punti
            sprintf(pointsStr, "\nPunti ottenuti: %d\n\n", points);

            // Inizializzazione ed invio del messaggio
            init_message(&response, strlen(pointsStr), MSG_PUNTI_PAROLA, pointsStr);
            write_socket(socketFD, response);

            // Deallocazione campo data
            free_message(&response);

        } else {

            // Altrimenti viene inserita (in lower case) nella linked list delle parole proposte e vengono inviati i punti
            insert_linked_list_node(guessedWords, to_lower_word(guess));

            // Assegnamento dei punti
            points = word_points(guess);
            *clientScore += points;

            // Formattazione della stringa dei punti
            sprintf(pointsStr, "\nPunti ottenuti: %d\n\n", points);

            // Inizializzazione ed invio del messaggio
            init_message(&response, strlen(pointsStr), MSG_PUNTI_PAROLA, pointsStr);
            write_socket(socketFD, response);

            // Deallocazione campo data
            free_message(&response);

        }

    } else {
        
        // Altrimenti se la parola non e' valida viene inviato un messaggio di errore
        char *invalidWordStr = "\nParola non valida.\n\n";

        // Inizializzazione ed invio del messaggio
        init_message(&response, strlen(invalidWordStr), MSG_ERR, invalidWordStr);
        write_socket(socketFD, response);

        // Deallocazione campo data
        free_message(&response);

    }

}



// Gestisce richieste del client di tipo "MSG_MATRICE" aggiungendo "MSG_TEMPO_PARTITA" o "MSG_TEMPO_ATTESA"
void handle_matrix_and_time(int socketFD, int isRegistered) {

    // Se l'utente non e' registrato invia un messaggio di errore
    if(!isRegistered) {

        message response;                                                                   // Struttura del messaggio di risposta
        char* notReg = "\nNon puoi richiedere la matrice se non sei registrato.\n\n";       // Stringa di risposta
        
        // Inizializzazione ed invio del messaggio
        init_message(&response, strlen(notReg), MSG_ERR, notReg);
        write_socket(socketFD, response);

        // Deallocazione del campo data
        free_message(&response);

        return;

    }

    // Invio della matrice se il gioco e' in corso (inGame == 1)
    if(get_flag(0)) {

        message response;                       // Struttura del messaggio di risposta
        char matrixBoard[BUF_SIZE] = {""};      // Stringa che immagazzina la matrice "board"

        // Conversione della matrice "board" in stringa in "matrixBoard"
        matrix_to_string(matrixBoard);

        // Inizializzazione ed invio del messaggio
        init_message(&response, strlen(matrixBoard), MSG_MATRICE, matrixBoard);
        write_socket(socketFD, response);

        // Deallocazione campo data
        free_message(&response);

    }

    // Invio del di "MSG_TEMPO_PARTITA" / "MSG_TEMPO_ATTESA"
    send_time(socketFD);

}



// Converte la matrice di gioco in una stringa unica "toString"
void matrix_to_string(char toString[]) {

    // Lock del mutex relativo alla matrice di gioco
    SYST(pthread_mutex_lock(&mutexBoard));

    // Per ogni elemento di "board" lo concatena a "toString" e aggiunge uno spazio
    for(int i = 0 ; i < MATRIX_ROWS ; i++) {
        for(int j = 0 ; j < MATRIX_COLUMNS ; j++) {
            
            // Concatenazione elemento corrente
            strcat(toString, board[i][j]);
            
            // Per l'ultimo elemento della matrice non viene aggiunto lo spazio
            if((i != (MATRIX_ROWS - 1)) || (j != (MATRIX_COLUMNS - 1)))
                
                strcat(toString, " ");
            
        }
    }

    // Unlock del mutex
    SYST(pthread_mutex_unlock(&mutexBoard));

}



// Gestisce richieste del client di tipo "MSG_PUNTI_FINALI"
void handle_final_score(int socketFD, int isRegitered) {

    message response;
    char rankingStr[RANK_STRING + FIXED_STRING_SIZE];   // Stringa destinazione per la formattazione di "ranking"

    // Se l'utente non e' registrato viene inviato un messaggio di tipo "MSG_ERR"
    if(!isRegitered) {

        char * notRegistered = "\nDevi essere registrato per richiedere i punteggi finali.\n\n";

        // Inizializzazione ed invio del messaggio
        init_message(&response, strlen(notRegistered), MSG_ERR, notRegistered);
        write_socket(socketFD, response);

        // Deallocazione campo data
        free_message(&response);

        return;

    }

    // Se il gioco non e' in corso (inGame == 0) viene inviato un messaggio di tipo "MSG_ERR"
    if(get_flag(0)) {

        char * notInGameStr = "\nIl gioco e' ancora in corso.\n\n";

        // Inizializzazione ed invio del messaggio
        init_message(&response, strlen(notInGameStr), MSG_ERR, notInGameStr);
        write_socket(socketFD, response);

        // Deallocazione campo data
        free_message(&response);

        return;

    }

    // Lock del mutex relativo alla stringa della classifica
    SYST(pthread_mutex_lock(&mutexRanking));

    // Formattazione della stringa ranking
    sprintf(rankingStr, "\nClassifica finale: %s\n\n", ranking);

    // Unlock del mutex
    SYST(pthread_mutex_unlock(&mutexRanking));

    // Inizializzazione ed invio del messaggio
    init_message(&response, strlen(rankingStr), MSG_PUNTI_FINALI, rankingStr);
    write_socket(socketFD, response);

}



// Gestisce richieste del client di tipo "MSG_FINE"
void handle_exit(int socketFD, char username[], int isRegistered, FILE* dictionaryFile, linked_list_node** guessedWords) {

    message response;
    int returnValue;
    char *disconnectPrint = "Client Disconnesso\n";     // Messaggio di disconnessione

    // Cancellazione dello username del client da "clientsUsernames" (se registrato) e decremento del numero di client registrati da "numRegisteredClients"
    if(isRegistered)	
        unregister_user(username);

    // Stampa della disconnessione su stdout
    SYSC(returnValue, write(STDOUT_FILENO, disconnectPrint, sizeof(char) * strlen(disconnectPrint)), "Error Write Connect");

    // Inizializzazione ed invio del messaggio
    init_message(&response, 0, MSG_FINE, "");
    write_socket(socketFD, response);

    // Chiusura del socket
    SYSC(returnValue, close(socketFD), "Close socket Error");

    // Chiusura del file dizionario
    fclose(dictionaryFile);

    // Deallocazione linked list delle parole proposte
    destroy_linked_list(guessedWords);

    // Cancellazione del tid dall'array dei thread che gestiscono i client
    delete_tid();

    // Terminazione thread
    pthread_exit(NULL);

}



// Gestisce la fine di una partita scrivendo sulla coda condivisa e inviando i punteggi ai thread tramite "MSG_PUNTI_FINALI"
void handle_game_end(int socketFD, queue** scoresQueue, int* score, char username[], linked_list_node** guessedWords, int isRegistered) {

    // Lock del mutex relativo alla coda condivisa dei punteggi
    SYST(pthread_mutex_lock(&mutexScoresQueue));

    // Inserimento dei risultati nella coda condivisa
    enqueue(scoresQueue, *score, username);

    // Lock del mutex relativo al numero di client registrati
    SYST(pthread_mutex_lock(&mutexNumRegisteredClients));

    // Se il numero di client registrati e' lo stesso degli elementi della coda (tutti hanno scritto sulla coda) viene notificato il thread scorer
    if(numRegisteredClients == get_queue_length(*scoresQueue))
        SYST(pthread_cond_signal(&notFullQueue));

    // Unlock del mutex relativo al numero di client registrati
    SYST(pthread_mutex_unlock(&mutexNumRegisteredClients));

    // Unlock del mutex relativo alla coda condivisa dei punteggi
    SYST(pthread_mutex_unlock(&mutexScoresQueue));

    // Azzeramento del punteggio
    *score = 0;

    // Ripristino della linked list delle parole indovinate
    destroy_linked_list(guessedWords);

    // Lock del mutex relativo alla classifica dei punteggi
    SYST(pthread_mutex_lock(&mutexRanking));

    // Se la classifica non e' pronta il thread si mette in attesa
    if(strlen(ranking) == 0)
        SYST(pthread_cond_wait(&notReadyRanking, &mutexRanking));

    // Unlock del mutex relativo alla classifica dei punteggi
    SYST(pthread_mutex_unlock(&mutexRanking));

    // Invio dei punteggi ai client
    handle_final_score(socketFD, isRegistered);

}



// Crea una nuova coda impostando "head" e "tail" a NULL
void create_queue (queue** newQueue) {

    (*newQueue)->head = NULL;
    (*newQueue)->tail = NULL;

}



// Restituisce "1" se la coda e' vuota, "0" altrimenti
int is_empty_queue(queue* isEmptyQueue) {

    return isEmptyQueue->head ? 0 : 1;

}



// Inserisce un nodo alla coda inizializzando i campi "score" e "username"
void enqueue(queue** toInitQueue, int score, char username[]) {

    // Allocazione del nuovo nodo
    queue_node* newQueueNode = (queue_node*) malloc(sizeof(queue_node));
    MALLOC_CHECK(newQueueNode);
    
    // Inizializzazione del nodo
    newQueueNode->score = score;
    strcpy(newQueueNode->username, username);
    newQueueNode->next = NULL;
    newQueueNode->prev = (*toInitQueue)->head;
    
    // Se la coda e' vuota, testa e fine della coda devono puntare allo stesso elemento
    if(is_empty_queue(*toInitQueue))

        (*toInitQueue)->head = newQueueNode;

    else

        // Altrimenti il puntatore al successivo della fine della coda deve puntare al nuovo nodo
        ((*toInitQueue)->tail)->next = newQueueNode;

    // Aggiornamento della fine della coda al nuovo nodo
    (*toInitQueue)->tail = newQueueNode;

}



// Dealloca il primo elemento della coda
void dequeue(queue** toDeqQueue) {

    if(is_empty_queue(*toDeqQueue))
        return;

    // Nodo di supporto che punta alla coda di "toDeqQueue"
    queue_node* tempNode = (*toDeqQueue)->head;

    // Aggiornamento della coda
    (*toDeqQueue)->head = tempNode->next;

    // Se la testa e' diventata vuota, anche la fine della coda vale NULL
    if(!(*toDeqQueue)->head)

        (*toDeqQueue)->tail = NULL;

    // Deallocazione nodo
    free(tempNode);

}



// Dealloca la coda
void destroy_queue(queue ** toDestroyQueue) {

    // Finche' la coda non e' vuota, dealloca il primo elemento
    while(!is_empty_queue(*toDestroyQueue)) 

        dequeue(toDestroyQueue);

}



// Conta gli elementi presenti nella coda
int get_queue_length(queue* toCountQueue) {

    queue_node *currentPtr = toCountQueue->head;        // Puntatore all'elemento corrente
    int elementsNum = 0;                                // Numero degli elementi in coda

    while(currentPtr != NULL) {

        elementsNum++;
        currentPtr = currentPtr->next;

    }

    return elementsNum;
    
}




// Inserisce un nodo in testa alla linked list
void insert_linked_list_node(linked_list_node** head, char data[]) {

    // Allocazione del nuovo nodo
    linked_list_node *newNode = (linked_list_node*) malloc(sizeof(linked_list_node));
    MALLOC_CHECK(newNode);

    // Inizializzazione del nodo
    strcpy(newNode->guess, data);
    newNode->next = NULL;

    newNode->next = *head;
    *head = newNode;

}



// Dealloca la Linked List
void destroy_linked_list(linked_list_node** head) {

    // Puntatori al nodo corrente e successivo
    linked_list_node *currentNode = *head;
    linked_list_node *nextNode = NULL;

    // Finche' la lista non e' vuota
    while(currentNode != NULL) {

        // Assegnamento, al nodo successivo, del nodo successivo al corrente e deallocazione del nodo corrente
        nextNode = currentNode->next;
        free(currentNode);

        // Aggiornamento del nodo corrente al nodo successivo
        currentNode = nextNode;

    }

    *head = NULL;

}



// Controlla se "guess" e' presente nella linked list. Se presente restituisce 1, 0 altrimenti.
int check_linked_list(linked_list_node* head, char guess[]) {

    linked_list_node* currentPtr = head;    // Puntatore all'elemento corrente della lista
    int result = 0;                         // Risultato che indica se "guess" e' presente o meno

    // Finche' ci sono elementi nella lista
    while(currentPtr != NULL) {

        // Se "guess" e' uguale al campo guess dell'elemento corrente, allora la parola e' gia' stata proposta
        if(strcmp(currentPtr->guess, guess) == 0) {
            result = 1;
            break;
        }

        currentPtr = currentPtr->next;

    }

    return result;

}



// Inizializza la struttura "params" con i parametri di "argv[]"
void init_params(int argc, char* argv[], params* serverParams) {

    int nonOptionalParamsNum = 2;       // Numero di parametri obbligatori
    int optionalParamsNum = 4;          // Numero di parametri opzionali
    int durata = 3;                     // Durata di default
    int minDurata = 1;                  // Durata minima
    
    // Opzioni dei parametri opzionali
    char* matrixParam = "--matrici";
    char* durataParam = "--durata";
    char* seedParam = "--seed";
    char* dictionaryParam = "--diz";

    // Inizializzazione parametri default
    serverParams->seed = 0;
    serverParams->durata = durata;
    strcpy(serverParams->matrixFileName,"");
    strcpy(serverParams->dictionaryFileName, "data/dictionary_ita.txt");

    // Inizializzazione del nome del server dal primo elemento
    param_copy(serverParams, argv[1], 's');

    // Inizializzazione della porta del server dal secondo elemento
    serverParams->serverPort = atoi(argv[2]);

    // Inizializzazione dei parametri opzionali 
    for(int i = (nonOptionalParamsNum + 1) ; i < argc ; i+= 2) {        // i viene incrementato di 2 dato che deve saltare il valore del parametro opzionale

        char *currentParam = argv[i];       // Parola chiave ("--parametro")
        char *nextParam = argv[i + 1];      // Valore che segue la parola chiave

        // Confronto del parametro corrente con i vari nomi di parametri opzionali e inizializzazione per ognuno di essi
        for(int j = 0 ; j < optionalParamsNum ; j ++) {

            // Se il parametro e' "--matrici"
            if(!strcmp(currentParam, matrixParam)) 
                param_copy(serverParams, nextParam, 'm');
                
            // Se il parametro e' "--durata"
            else if(!strcmp(currentParam, durataParam)) {
                
                // Solo se la durata passata come parametro e' maggiore o uguale di quella minima
                if(atoi(nextParam) >= minDurata) {
                    serverParams->durata = atoi(nextParam);
                }

            }

            // Se il parametro e' "--seed"
            else if(!strcmp(currentParam, seedParam))
                serverParams->seed = atoi(nextParam);

            // Se il parametro e' "--diz"
            else if(!strcmp(currentParam,dictionaryParam))
                param_copy(serverParams, nextParam, 'd');

            // Altrimenti restituisce un errore
            else {

                perror("Parameters format error");
                exit(errno);

            }

        }
    }
    
}



// Copia la stringa "toCopy" in params.serverName / params.matrixFile / params.dictionaryFile in base al carattere "where" ('s', 'm', 'd')
void param_copy(params* dest, char* toCopy, char where) {

    // Se where e' 's' inizializza il nome del server, se e' 'm' il nome del file delle matrici, altrimenti se e' 'd' il nome del file dizionario
    switch (where) {

    case 's':

        strcpy(dest->serverName, toCopy);
        break;
    
    case 'm':

        strcpy(dest->matrixFileName, toCopy);
        break;

    case 'd':

        strcpy(dest->dictionaryFileName, toCopy);
        break;
    
    default:

        perror("Parameters copy error");
        exit(errno);
        break;

    }
    
}



// Controlla che il numero minimo di parametri da linea di comando sia corretto
void check_params(int argc, int minParams) {
    
    // Se il numero di parametri e' minore del numero minimo restituisce un errore
    if(argc < minParams) {
        perror("Parameters Error");
        exit(errno);
    }

}



// Ricerca una posizione libera nell'array globale "clientHandlerThreads" (restituisce l'indice se lo trova, -1 altrimenti) ed assegna "clientSocketFD"
// alla posizione libera associata all'array globale "clientSocketFileDescriptors"
int search_free_index(int clientSocketFD) {

    int index = -1;

    // Lock del mutex relativo all'array dei client connessi
    SYST(pthread_mutex_lock(&mutexClientHandlerThreads));

    for(int i = 0 ; i < MAX_CONNECTED_CLIENTS ; i++) {

        if(clientHandlerThreads[i] == 0) {

            index = i;

            // Assegnamento del file descriptor associato al thread di tid di posizione "index"
            clientSocketFileDescriptors[index] = clientSocketFD;
            break;

        }
        
    }

    // Unlock del mutex
    SYST(pthread_mutex_unlock(&mutexClientHandlerThreads));

    return index;

}


// Cancella il proprio tid dall'array globale dei thread che gestiscono i clients (e il relativo file descriptor dall'array globale dei FD)
void delete_tid() {

    // Tid del thread corrente
    pthread_t myTid = pthread_self();

    // Lock del mutex relativo all'array dei client connessi
    SYST(pthread_mutex_lock(&mutexClientHandlerThreads));

    for(int i = 0 ; i < MAX_CONNECTED_CLIENTS ; i++) {

        if(clientHandlerThreads[i] == myTid) {
            
            // Cancellazione del tid e del file descriptor associato
            clientHandlerThreads[i] = 0;
            clientSocketFileDescriptors[i] = 0;
            break;

        }
        
    }

    // Unlock del mutex
    SYST(pthread_mutex_unlock(&mutexClientHandlerThreads));

}



// Segnala tramite pthread_kill() tutti i thread che gestiscono i client
void signal_client_handlers() {

    // Lock del mutex relativo all'array dei client connessi
    SYST(pthread_mutex_lock(&mutexClientHandlerThreads));

    for(int i = 0 ; i < MAX_CONNECTED_CLIENTS ; i++) {

        // Invia un segnale di tipo "SIGUSR1" ad ogni thread
        if(clientHandlerThreads[i] != 0)

            pthread_kill(clientHandlerThreads[i], SIGUSR1);

    }

    // Unlock del mutex
    SYST(pthread_mutex_unlock(&mutexClientHandlerThreads));
    
}



// Restituisce il contenuto della variabile globale "inGame" se choice e' uguale a 0, altrimenti restituisce quello di "wroteData"
int get_flag(int choice) {
    
    int result;         // Risultato da essere restituito

    if(choice == 0) {

        // Lock del mutex relativo alla variabile globale "inGame"
        SYST(pthread_mutex_lock(&mutexInGame));

        result = inGame;

        // Unlock del mutex
        SYST(pthread_mutex_unlock(&mutexInGame));

    } else {

        // Lock del mutex relativo alla variabile globale "wroteData"
        SYST(pthread_mutex_lock(&mutexWroteData));

        result = wroteData;

        // Unlock del mutex
        SYST(pthread_mutex_unlock(&mutexWroteData));

    }
    
    return result;

}



// Scrive il valore di "op" nella variabile globale "inGame" se choice e' uguale a 0, altrimenti lo scrive in "wroteData"
void write_flag(int choice, int op) {

    if(choice == 0) {

        // Lock del mutex relativo alla variabile globale "inGame"
        SYST(pthread_mutex_lock(&mutexInGame));

        inGame = op;

        // Unlock del mutex
        SYST(pthread_mutex_unlock(&mutexInGame));

    } else {

        // Lock del mutex relativo alla variabile globale "wroteData"
        SYST(pthread_mutex_lock(&mutexWroteData));

        wroteData = op;

        // Unlock del mutex
        SYST(pthread_mutex_unlock(&mutexWroteData));

    }

}



// Gestore di segnali
void signal_handler(int signo) {

    // Non fa niente in caso di "SIGUSR1"
    if(signo == SIGUSR1)

        return;

    // Termina in caso di "SIGINT"
    if(signo == SIGINT) {

        write(STDOUT_FILENO, "Chiusura del server...\n", 23);

    }

}