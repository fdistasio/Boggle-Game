#include "macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>



#define MAX_ELEM_LENGTH 3



// Funzioni per controllare la validità di una parola all'interno della matrice di gioco

int word_checker(int rows, int columns, char board[rows][columns][MAX_ELEM_LENGTH], char* word);

int word_rec_check(int rows, int columns, char board[rows][columns][MAX_ELEM_LENGTH], int** visitedMatrix, char* word, int i, int j, int currentWordCharIndex);

int check_dictrionary(FILE* dictionary, char* word);

int check_word_length(char* word, int minLength);

char* to_lower_word(char* word);

int is_valid_element(int i, int j, int rows, int columns, int** visitedMatrix);

int is_equal_char(char* boardStr, char* word, int wordCharIndex);

int is_word_rec_ended(char* word, int currentWordCharIndex);

int qu_checker(char* word);

char* to_upper_word(char* word);

int word_points(char* word);



// Funzioni per inizializzare la matrice di gioco

void init_rand_board_matrix(int rows, int columns, char board[rows][columns][MAX_ELEM_LENGTH]);

void del_endl(char* str, int maxLength);

char* to_upper_row(char* fileRow);

int check_file_row(char* fileRow, int rows, int columns);

void init_file_board_matrix(FILE* filePtr, int rows, int columns, char board[rows][columns][MAX_ELEM_LENGTH]);

char generate_rand_char();



// Funzioni per generare, resettare ed eliminare la matrice di supporto delle celle visitate

int** generate_visited_matrix(int rows, int columns);

void reset_visited_matrix(int** visitedMatrix, int rows, int columns);

void destroy_visited_matrix(int** visitedMatrix, int rows);