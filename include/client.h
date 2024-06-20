#ifndef CLIENT_HEADER

#define CLIENT_HEADER

#include "macros.h"
#include "costants.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>



// Funzioni del thread che invia dati al server

void* thread_write_server(void* socketFD);

void send_user_request(int socketFD, char fromUser[]);

char get_request_type(char* request, char* requestKeys[]);

void register_user(int socketFD, char* username);

void send_word(int socketFD, char* word);

void print_matrix(int socketFD);

void print_results(int socketFD);

void print_help(char* commands[]);

void exit_game(int socketFD);

void del_endl(char str[]);

void clean_buffer(int size, char buf[]);



// Funzioni per il controllo e inizializzazione dei parametri

void check_params(int argc, int minParams);

void init_params(int argc, char* argv[], char serverName[], int* serverPort);



// Funzioni del thread che legge dati dal server

void* thread_read_server(void* socketFD);

void handle_server_response(message response, int socketFD);

void handle_ok_err(message response);

void handle_matrix(message response);

void print_str_to_matrix(int rows, int columns, char toMatrix[]);

void print_separators(int columns);

void handle_game_time(message response);

void handle_final_score(message response);

void handle_word_points(message response);

void handle_exit(message response, int socketFD);



#endif