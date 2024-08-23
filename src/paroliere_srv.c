#include "../include/server.h"



int main(int argc, char* argv[]) {

    int returnValue;													// Valore di ritorno per la gestione degli errori delle chiamate di sistema
    FILE * matrixFile;													// File delle matrici
    params serverParams;												// Struttura contenente i parametri di avvio del server
    int serverSocketFD, clientSocketFD;									// File Descriptor dei socket client e server
    struct sockaddr_in serverAddr, clientAddr;							// Strutture sockaddr
    socklen_t clientAddrLength;											// Lunghezza indirizzo client
    sigset_t signalSet;													// Set di segnali
    struct sigaction sig;												// Struct per sigaction
    message response;													// Struttura per il messaggio "MSG_FINE" alla terminazione del server

    int generateFrom = 0;												// Indica come inizializzare la matrice, 0 random, altrimenti da file


    // Maschera per "SIGALRM"
    SYSC(returnValue, sigemptyset(&signalSet), "Error signal set");
    SYSC(returnValue, sigaddset(&signalSet, SIGALRM), "Error signal set");
    SYST(pthread_sigmask(SIG_SETMASK, &signalSet, NULL));

    // Gestore per i segnali "SIGINT" e "SIGUSR1"		
    memset(&sig, 0, sizeof(sig));
    sig.sa_handler = signal_handler;
    sigaction(SIGUSR1, &sig, NULL);
    sigaction(SIGINT, &sig, NULL);

    // Controllo della correttezza del numero minimo dei parametri
    check_params(argc, MIN_SERVER_PARAMS);

    // Inizializzazione dei parametri
    init_params(argc, argv, &serverParams);

    // Se il file di matrici e' definito la matrice sarà generata tramite esso
    if(strlen(serverParams.matrixFileName)) {

        // Apertura del file matrici
        matrixFile = fopen(serverParams.matrixFileName, "r");
        FILE_CHECK(matrixFile);

        // Aggiornamento di "generateFrom" che indica di inizializzare la matrice tramite file
        generateFrom = 1;

    }
        
    // Se la matrice non e' inizializzata da file, verrà inizializzata in modo random, se il seed non è inizializzato viene generato tramite time(NULL)
    (!serverParams.seed) ? srand(serverParams.seed) : srand(time(NULL));

    // Allocazione di una coda condivisa tra thread per la classifica dei punteggi
    queue *scoresQueue = malloc(sizeof(queue));
    MALLOC_CHECK(scoresQueue);
    create_queue(&scoresQueue);

    // Creazione di un thread scorer che gestisce la classifica dei punteggi
    pthread_t scorerThread;
    SYST(pthread_create(&scorerThread, NULL, &thread_scorer, scoresQueue));

    // Detatch del thread scorer
    SYST(pthread_detach(scorerThread));

    // Allocazione, inizializzazione dei parametri e creazione di un thread arbitro che gestisce la generazione di matrici e le pause della partita
    pthread_t arbThread;

    arb_args *arbThreadArgs = malloc(sizeof(arb_args));
    MALLOC_CHECK(arbThreadArgs);

    init_arb_args(arbThreadArgs, generateFrom, serverParams.matrixFileName, serverParams.durata);

    SYST(pthread_create(&arbThread, NULL, &thread_arb, arbThreadArgs));

    // Detatch del thread arbitro
    SYST(pthread_detach(arbThread));

    // Creazione del socket
    SYSC(serverSocketFD, socket(AF_INET, SOCK_STREAM, 0), "Socket Error");

    // Riutilizzo dell'indirizzo
    SYSC(returnValue, setsockopt(serverSocketFD, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)), "Setsockopt Error");

    // Inizializzazione della struttura "serverAddr"
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverParams.serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverParams.serverName);

    // Binding
    SYSC(returnValue, bind(serverSocketFD, (struct sockaddr *) &serverAddr, sizeof(serverAddr)), "Bind Error");

    // Listen
    SYSC(returnValue, listen(serverSocketFD, MAX_CONNECTED_CLIENTS), "Listen Error");

    // Accept e Gestione delle connessioni
    while(1) {
        
        // Indice di posizione libera nell'array "clientHandlerThreads"
        int freeIndex = -1;

        // Accetta la connessione
        clientAddrLength = sizeof(clientAddr);
        clientSocketFD = accept(serverSocketFD, (struct sockaddr *) &clientAddr, &clientAddrLength);

        // Gestione errori ed interruzione dovuta a "SIGINT"
        if(clientSocketFD == -1) {

            // Se accept() viene interrotta (da SIGINT), esce dal ciclo, altrimenti restituisce un errore
            if (errno == EINTR)

                break;

            else {

                perror("Accept Error"); 
                exit(errno); 
            
            }

        }

        // Ricerca di un indice libero nell'array globale "clientHandlerThreads"
        freeIndex = search_free_index(clientSocketFD);

        // Se non vi e' una posizione libera la richiesta di connessione viene chiusa e ignorata
        if(freeIndex == -1) {

            SYSC(returnValue, close(clientSocketFD), "Close Error");
            continue;

        }

        // Allocazione, inizializzazione dei parametri e creazione di un thread dedicato al client
        
        client_handler_args *clientHandlerArgs = malloc(sizeof(client_handler_args));
        MALLOC_CHECK(clientHandlerArgs);

        init_client_handler_args(clientHandlerArgs, clientSocketFD, serverParams.dictionaryFileName, scoresQueue);

        SYST(pthread_create(&clientHandlerThreads[freeIndex], NULL, &thread_handle_connection, clientHandlerArgs));

        // Detatch del thread corrente
        SYST(pthread_detach(clientHandlerThreads[freeIndex]));

    }

    // Terminazione server
    
    // Lock del mutex relativo all'array dei client connessi
    SYST(pthread_mutex_lock(&mutexClientHandlerThreads));

    // Inizializzazione del messaggio
    init_message(&response, 0, MSG_FINE, "");

    // Invio del messaggio "MSG_FINE" su tutti i file descriptor dei client connessi
    for(int i = 0 ; i < MAX_CONNECTED_CLIENTS ; i++) {

        if(clientSocketFileDescriptors[i] != 0)
            write_socket(clientSocketFileDescriptors[i], response);
        
    }

    // Unlock del mutex
    SYST(pthread_mutex_unlock(&mutexClientHandlerThreads));
    
}