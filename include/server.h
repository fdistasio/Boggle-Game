#ifndef SERVER_HEADER

#define SERVER_HEADER

#include "macros.h"
#include "costants.h"
#include "protocol.h"
#include "paroliere.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>



// Variabili globali

extern pthread_t clientHandlerThreads[MAX_CONNECTED_CLIENTS];               // Array relativo ai tid dei thread che gestiscono gli utenti
extern int clientSocketFileDescriptors[MAX_CONNECTED_CLIENTS];              // Array relativo ai file descriptor dei socket dei client
extern char board[MATRIX_ROWS][MATRIX_COLUMNS][MAX_ELEM_LENGTH];            // Matrice di gioco
extern char clientsUsernames[MAX_CONNECTED_CLIENTS][MAX_USERNAME_SIZE];	    // Username dei client connessi
extern int numClients;                                                      // Numero dei client connessi
extern int numRegisteredClients;                                            // Numero dei client connessi e registrati
extern char ranking[RANK_STRING];                                           // Stringa relativa alla classifica di gioco
extern int inGame;                                                          // Indica se il gioco e' in corso o meno (1 in corso, 0 altrimenti)
extern int wroteData;                                                       // Indica se i dati devono essere scritti o meno sulla coda condivisa (1 in caso affermativo, 0 altrimenti)

extern pthread_mutex_t mutexClientHandlerThreads;                           // Mutex relativo a "clientHandlerThreads"
extern pthread_mutex_t mutexRanking;                                        // Mutex relativo a "ranking"
extern pthread_mutex_t mutexBoard;                                          // Mutex relativo a "board"
extern pthread_mutex_t mutexClientsUsernames;                               // Mutex relativo a "clientUsernames"
extern pthread_mutex_t mutexNumClients;                                     // Mutex relativo a "numClients"
extern pthread_mutex_t mutexNumRegisteredClients;                           // Mutex relativo a "numClientsRegistered"
extern pthread_mutex_t mutexScoresQueue;                                    // Mutex relativo alla coda dei punteggi (allocata dinamicamente)
extern pthread_mutex_t mutexInGame;                                         // Muter relativo a "inGame"
extern pthread_mutex_t mutexWroteData;                                      // Mutex relativo a "wroteData"
extern pthread_mutex_t mutexAlarm;                                          // Mutex relativo alla "risorsa" alarm()

extern pthread_cond_t notReadyRanking;                                      // Variabile di condizione relativa a "ranking"
extern pthread_cond_t notFullQueue;                                         // Variabile di condizione relativa alla coda dei punteggi (allocata dinamicamente)



// Struttura per i parametri da linea di comando

typedef struct {

    char serverName[MAX_PARAM_SIZE];                // Nome del server
    int serverPort;                                 // Porta del server 
    char matrixFileName[MAX_PARAM_SIZE];            // Nome del file delle matrici
    int durata;                                     // Durata delle partite (espressa in minuti)
    int seed;                                       // Seed per srand()
    char dictionaryFileName[MAX_PARAM_SIZE];        // Nome del file dizionario

} params;



// Struttura per la coda condivisa dei punteggi

typedef struct qn {

    int score;                              // Punteggio
    char username[MAX_USERNAME_SIZE];       // Username
     struct qn * next;                      // Puntatore all'elemento successivo della coda
    struct qn * prev;                       // Puntatore all'elemento precedente della coda

} queue_node;

typedef struct {

        queue_node* head;   // Puntatore alla testa della coda
        queue_node* tail;   // Puntatore alla fine della coda

} queue;



// Struttura relativa ai nodi della linked list delle parole proposte

typedef struct ln {

    char guess[MAX_WORD_LENGTH];        // Parola proposta
    struct ln * next;                   // Puntatore all'elemento successivo della lista

} linked_list_node;



// Struttura per i parametri del thread arbitro che gestisce generazione di matrici e pause di gioco

typedef struct {

    char matrixFileName[MAX_PARAM_SIZE];            // Nome del file delle matrici
    int generateFrom;                               // Indica come inizializzare la matrice, 0 random, altrimenti da file
    int durata;                                     // Durata delle partite (espressa in minuti)

} arb_args;



// Struttura per i parametri dei thread che gestiscono le connessioni

typedef struct {

    int clientSocketFD;                             // File descriptor del socket
    char dictionaryFileName[MAX_PARAM_SIZE];        // Nome del file dizionario
    queue* scoresQueue;                             // Coda condivisa dei punteggi

} client_handler_args;



// Struttura per l'array di appoggio per la classifica del thread scorer

typedef struct {

    int score;                                      // Punteggio
    char username[MAX_USERNAME_SIZE];               // Username

} player;



// Funzioni del thread che gestisce la generazione di matrici

void* thread_scorer(void* args);

void copy_score_queue(queue* scoreQueue, player players[]);

void make_ranking(player players[], int playersLength);

int cmp_players(const void* playerA, const void* playerB);



// Funzioni del thread che gestisce la generazione di matrici

void* thread_arb(void* args);

void init_arb_args(arb_args* args, int generateFrom, char matrixFile[], int durata);

void timer_scheduler(int durata, sigset_t* signalSet, int* sig);



// Funzioni dei thread che gestiscono le connessioni

void* thread_handle_connection(void* args);

void clients_number_update(int operator);

void init_client_handler_args(client_handler_args* args, int socketFD, char dictionaryFile[], queue* scoresQueue);

void handle_client_request(message receive, int socketFD, char username[], int* isRegistered, int* clientScore, FILE* dictionaryFile, linked_list_node** guessedWords);

void handle_registration(message receive, int socketFD, char username[], int* isRegistered);

void send_time(int socketFD);

int is_user_registered(char* username);

void register_user(char* username);

void unregister_user(char* username);

void registered_clients_number_update(int operator);

void handle_guess(message receive, int socketFD, int* clientScore, FILE* dictionaryFile, linked_list_node** guessedWords, int* isRegistered);

void handle_matrix_and_time(int socketFD, int isRegistered);

void matrix_to_string(char toString[]);

void handle_final_score(int socketFD, int isRegitered);

void handle_exit(int socketFD, char username[], int isRegistered, FILE* dictionaryFile, linked_list_node** guessedWords);

void handle_game_end(int socketFD, queue** scoresQueue, int* score, char username[], linked_list_node** guessedWords, int isRegistered);



// Funzioni per gestire la coda condivisa dei punteggi

void create_queue (queue** newQueue);

int is_empty_queue(queue* isEmptyQueue);

void enqueue(queue** toInitQueue, int score, char username[]);

void dequeue(queue** toDeqQueue);

void destroy_queue(queue ** toDestroyQueue);

int get_queue_length(queue* toCountQueue);



// Funzioni per gestire Linked lists relativa alle parole proposte

void insert_linked_list_node(linked_list_node** head, char guess[]);

void destroy_linked_list(linked_list_node** head);

int check_linked_list(linked_list_node* head, char guess[]);



// Funzioni per il controllo e inizializzazione dei parametri

void init_params(int argc, char* argv[], params* serverParams);

void param_copy(params* dest, char* toCopy, char where);

void check_params(int argc, int minParams);



// Funzioni per la gestione dell'array globale "clientHandlerThreads"

int search_free_index(int clientSocketFD);

void delete_tid();

void signal_client_handlers();



// Funzioni relative a flag globali

int get_flag(int choice);

void write_flag(int choice, int op);



// Gestore di segnali

void signal_handler(int signo);



#endif