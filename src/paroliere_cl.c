#include "../include/client.h"



int main(int argc, char* argv[]) {

	int returnValue;						        // Valore di ritorno per la gestione degli errori delle chiamate di sistema
    char serverName[MAX_PARAM_SIZE];                // Nome del server (argv[1])
    int serverPort;                                 // Porta del server (argv[2])
    int clientSocketFD;                             // File descriptor del socket
    struct sockaddr_in serverAddr;                  // Struttura sockaddr
    struct sigaction sig;                           // Struttura per sigaction

    // Gestore per i segnali "SIGINT" (ignorato)	
	memset(&sig, 0, sizeof(sig));
	sig.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sig, NULL);

	// Controllo della correttezza del numero dei parametri
	check_params(argc, CLIENT_PARAMS);

    // Inizializzazione dei parametri
    init_params(argc, argv, serverName, &serverPort);

    // Creazione del socket
    SYSC(clientSocketFD, socket(AF_INET, SOCK_STREAM, 0), "Socket Error");

    // Inizializzazione della struttura server_addr
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverName);

    // Connessione al server
    SYSC(returnValue, connect(clientSocketFD, (struct sockaddr *) &serverAddr, sizeof(serverAddr)), "Connect Error");

    // Thread ricevitore di messaggi dal server
    pthread_t readThread;

    // Allocazione del parametro per il thread (descrittore socket)
    int * serverReaderArgs = malloc(sizeof(int));
	MALLOC_CHECK(serverReaderArgs);
	*serverReaderArgs = clientSocketFD;

    // Creazione thread ricevitore di messaggi dal server
	SYST(pthread_create(&readThread, NULL, &thread_read_server, serverReaderArgs));

    // Thread emittente di messaggi verso il server
    pthread_t writeThread;

    // Allocazione del parametro per il thread (descrittore socket)
    int * serverWriterArgs = malloc(sizeof(int));
	MALLOC_CHECK(serverWriterArgs);
	*serverWriterArgs = clientSocketFD;

    // Creazione thread emittente di messaggi verso il server
	SYST(pthread_create(&writeThread, NULL, &thread_write_server, serverWriterArgs));

    // Detatch del thread "writeThread"
	SYST(pthread_detach(writeThread));

    // Attesa terminazione "readThread" dato che dal momento in cui esso termina significa che il server e' offline oppure il client si vuole disconnettere
    SYST(pthread_join(readThread, NULL));
    
}