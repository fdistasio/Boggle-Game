#include "../include/paroliere.h"



//  Controlla la validità della parola "word" all'interno della matrice "board".
//  Si avvale di una matrice di supporto per tenere traccia delle caselle visitate e utilizza
//  la funzione ricorsiva "word_rec_check" per controllare la validità della parola in modo Depth First Search.
int word_checker(int rows, int columns, char board[rows][columns][MAX_ELEM_LENGTH], char* word) {

    int found = 0;
    int **visitedMatrix = generate_visited_matrix(rows, columns);   // Matrice degli elementi visitati
    char *toUpperWord = to_upper_word(word);                        // Parola "word" in upper case

    for(int i = 0 ; i < rows ; i ++) {
        for(int j = 0 ; j < columns ; j ++) {

            // Per ogni elemento della matrice che corrisponde alla prima lettera della parola,
            // la matrice "visitedMatrix" viene ripristinata e viene effettuata una ricerca ricorsiva della parola
            
            if(is_equal_char(board[i][j], toUpperWord, 0)) {

                reset_visited_matrix(visitedMatrix, rows, columns);
                found = word_rec_check(rows, columns, board, visitedMatrix, toUpperWord, i, j, 0);

            }
            
            if(found) return found;
        }
    }

    // Deallocazione della matrice di supporto
    destroy_visited_matrix(visitedMatrix,rows);

    // Deallocazione stringa di supporto in upper case
    free(toUpperWord);
    
    return found;

}



//  Ricerca la parola "word" nella matrice "board" in modo ricorsivo.
//  Vengono controllati ricorsivamente tutti gli elementi adiacenti e la chiamata ricorsiva e' effettuata solo dopo
//  aver controllato che il carattere sia uguale e valido, quindi dal momento in cui il carattere e' l'ultimo la stringa e' valida
int word_rec_check(int rows, int columns, char board[rows][columns][MAX_ELEM_LENGTH], int** visitedMatrix, char* word, int i, int j, int currentWordCharIndex) {

    // Indici per elementi adiacenti
    int leftJ = j - 1;
    int rightJ = j + 1;
    int upI = i - 1;
    int downI = i + 1;

    // Risultati delle chiamate ricorsive
    int res = 0, upRes = 0, downRes = 0, leftRes = 0, rightRes = 0;

    // Elemento corrente marcato come visitato
    visitedMatrix[i][j] = 1;

    // Se la parola e' finita, essa e' valida
    if(is_word_rec_ended(word,currentWordCharIndex + 1))
        return 1;

    // Gestione di "Qu"
    // Se la lettera corrente della parola "word" e' Q significa che e' seguita da una "u" (altrimenti la funzione non sarebbe stata eseguita)
    // Se dopo la u la stringa termina, la funzione restituisce 1, altrimenti continua
    if(word[currentWordCharIndex] == 'Q') {
        
        if(is_word_rec_ended(word,currentWordCharIndex + 2))
            return 1;

        currentWordCharIndex += 2;

    } else {

        currentWordCharIndex ++;

    }
    
    // Chiamate ricorsive sui caratteri adiacenti controllando prima la validità e l'uguaglianza al carattere successivo della parola. 

    // Elemento [i][j+1] (destra)
    if(is_valid_element(i, rightJ, rows, columns, visitedMatrix) && is_equal_char(board[i][rightJ], word, currentWordCharIndex)) {

        rightRes = word_rec_check(rows, columns, board, visitedMatrix, word, i, rightJ, currentWordCharIndex);
        
        // Ripristino della cella per gestire vicoli ciechi
        visitedMatrix[i][rightJ]= 0;

    }

    // elemento [i][j-1] (sinistra)
    if(is_valid_element(i, leftJ, rows, columns, visitedMatrix)  && is_equal_char(board[i][leftJ], word, currentWordCharIndex))	{

        leftRes = word_rec_check(rows, columns, board, visitedMatrix, word, i, leftJ, currentWordCharIndex);
        
        // Ripristino della cella per gestire vicoli ciechi
        visitedMatrix[i][leftJ] = 0;

    }
    
    // Elemento [i-1][j] (sotto)
    if(is_valid_element(upI, j, rows, columns, visitedMatrix)  && is_equal_char(board[upI][j], word, currentWordCharIndex))	{

        upRes = word_rec_check(rows, columns, board, visitedMatrix, word, upI, j, currentWordCharIndex);
        
        // Ripristino della cella per gestire vicoli ciechi
        visitedMatrix[upI][j] = 0;

    }
    
    // Elemento [i+1][j] (sopra)
    if(is_valid_element(downI, j, rows, columns, visitedMatrix) && is_equal_char(board[downI][j], word, currentWordCharIndex)) {
        
        downRes = word_rec_check(rows, columns, board, visitedMatrix, word, downI, j, currentWordCharIndex);
        
        // Ripristino della cella per gestire vicoli ciechi
        visitedMatrix[downI][j] = 0;

    }
    
    // Se almeno una chiamata e' valida allora la parola e' presente
    res =  upRes || downRes || leftRes || rightRes;
    return res;

}



// Controlla la presenza della parola "word" nel file di testo dizionario
int check_dictrionary(FILE* dictionaryFile, char* word) {

    int res = 0;								// Risultato: 0 se "word" non e' presente, 1 altrimenti
    char* toLowerWord = to_lower_word(word);	// Parola "word" in lower case
    int maxWordLength = 25;						// Lunghezza massima di una parola
    char currentWord[maxWordLength];			// Parola corrente del dizionario

    while(fgets(currentWord, maxWordLength, dictionaryFile) != NULL) {

        int currentWordLength = strlen(currentWord);
        currentWord[currentWordLength - 2] = '\0';	// Rimozione di '\n'

        // Se la parola corrente del dizionario e' uguale alla parola "toLowerWord", quest'ultima e' presente e il ciclo e' interrotto
        if(!strcmp(currentWord, toLowerWord)) {
            res = 1;
            break;
        }

    }

    // Deallocazione della stringa copia in lower case
    free(toLowerWord);

    // Riprisitino del file pointer ad inizio file
    if(fseek(dictionaryFile, 0, SEEK_SET) != 0) {

        perror("Fseek error");
        exit(errno);

    }

    return res;

}



// Controlla che la parola "word" sia lunga almeno "minLength" caratteri
int check_word_length(char* word, int minLength) {

    return strlen(word) < minLength ? 0 : 1;

}



char* to_lower_word(char* word) {

    // Allocazione di una stringa della dimensione di "word" + 1 (terminatore stringa)
    int toMalloc = strlen(word);
    char* toLowerWord = (char*) malloc((toMalloc + 1) * sizeof(char));
    MALLOC_CHECK(toLowerWord);

    for(int i = 0 ; i < toMalloc ; i++)
        
        // L'elemento corrente di "toLowerWord" e' dato dal carattere corrente di "word" in lower case
        toLowerWord[i] = tolower(word[i]);

    toLowerWord[toMalloc] = '\0';
    return toLowerWord;

}



// Controlla che un elemento sia compreso nella matrice, rispetto alla sua dimensione, e che non sia stato già visitato
int is_valid_element(int i, int j, int rows, int columns, int** visitedMatrix) {

    // Controllo rispetto alla dimensione delle righe
    if(i < 0 || i >= rows)
        return 0;

    // Controllo rispetto alla dimensione delle colonne
    if(j < 0 || j >= columns)
        return 0;

    // Controllo rispetto all'elemento corrispondente nella matrice "visitedMatrix", se 1 e' già stato visitato, altrimenti no
    if(visitedMatrix[i][j])
        return 0;
    
    return 1;

}



// Controlla che il carattere "boardStr[0]" (o la stringa "boardStr in caso di "Qu") della matrice di gioco sia uguale al carattere della parola "word"
int is_equal_char(char* boardStr, char* word, int wordCharIndex) {
    
    // Gestione della stringa "Qu": se "boardStr" e' uguale a "Qu" e il carattere della parola "word" e' 'Q',
    // allora se esso e' seguito da una 'U', le stringhe sono uguali
    if(qu_checker(boardStr) && word[wordCharIndex] == 'Q')

        return word[wordCharIndex + 1] == 'U'? 1 : 0;

    else 
        // Controllo del carattere della parola "word" con il carattere "boardStr[0]" della matrice
        return word[wordCharIndex] == boardStr[0];

}



// Controlla se la stringa "word" e' terminata in base al terminatore di stringa '\0'
int is_word_rec_ended(char* word, int currentWordCharIndex) {
    
    return word[currentWordCharIndex] == '\0' ?  1 :  0;

}



// Controlla se la  stringa "word" e' uguale a "Qu"
int qu_checker(char* word) {

    return strcmp(word,"Qu") == 0 ? 1 : 0;

}



// Restituisce la stringa "word" in upper case
char* to_upper_word(char* word) {

    // Allocazione di una stringa della dimensione di "word" + 1 (terminatore stringa)
    int toMalloc = strlen(word);
    char* toUpperWord = (char*) malloc((toMalloc + 1) * sizeof(char));
    MALLOC_CHECK(toUpperWord);

    for(int i = 0 ; i < toMalloc ; i++)

        // L'elemento corrente di "toUpperWord" e' dato dal carattere corrente di "word" in upper case
        toUpperWord[i] = toupper(word[i]);

    toUpperWord[toMalloc] = '\0';
    return toUpperWord;

}



// Restituisce i punti relativi ad una parola corretta
int word_points(char* word) {

    int wordLength = strlen(word);		// Lunghezza della parola
    int points = 0;

    // Per ogni 'Q' presente viene contato un punto in meno, questa
    // condizione e' sufficiente dato che si assume che la parola data sia corretta
    for(int i = 0 ; i < wordLength ; i++)

        if(toupper(word[i]) != 'Q')
            
            points++;

    return points;

}



// Inizializza una matrice di gioco in modo random
void init_rand_board_matrix(int rows, int columns, char board[rows][columns][MAX_ELEM_LENGTH]) {

    char randomChar;

    for(int i = 0 ; i < rows ; i++) {
        for(int j = 0 ; j < columns ; j++) {

            // Generazione di un carattere random
            randomChar = generate_rand_char();

            // Gestione carattere 'Q'
            if(randomChar == 'Q') {

                // Inizializzazione della cella della matrice con aggiunta del carattere 'u'
                strcpy(board[i][j], "Qu");
                board[i][j][2] = '\0';


            } else {

                // Inizializzazione della cella della matrice
                board[i][j][0] = randomChar;
                board[i][j][1] = '\0';

            }

        }

    }

}



// Genera un carattere random
char generate_rand_char() {
    
    char *alphabet = "ABCDEFGHILMNOPQRSTUVZ"; 
    int alphabetLength = 21;
    int randNum; 
    char randChar;

    // Genera un indice casuale per selezionare in modo random un carattere dalla stringa alphabet
    randNum = rand() % alphabetLength;
    randChar = alphabet[randNum];

    return randChar;

}



// Inizializza una matrice da file di testo contenente su ogni riga gli elementi della matrice separati da spazio
void init_file_board_matrix(FILE* filePtr, int rows, int columns, char board[rows][columns][MAX_ELEM_LENGTH]) {

    // Definizione di un buffer di lettura. La lunghezza massima e' data dal caso in cui ogni elemento sia "Qu" (contando spazi e \n)
    int maxFileRowLength = (rows * columns * 2) + ((rows * columns) + 1);
    char fileRow[maxFileRowLength];

    char *token;
    int tokenLength;

    // Lettura di una riga del file.
    // Se si verificano errori o il file e' finito (fgets == NULL), oppure una riga del file non e' valida, inizializza la matrice in modo random
    if(fgets(fileRow, maxFileRowLength, filePtr) == NULL || check_file_row(fileRow, rows, columns) == -1) {

        init_rand_board_matrix(rows, columns, board);
        printf("Matrice non valida, inizializzazione random...\n");
        return;

    }

    // Conversione della riga in upper case, in caso non lo fosse già
    char *toUppeFileRow = to_upper_row(fileRow);

    // Elimina '\n' dalla riga letta
    del_endl(toUppeFileRow, maxFileRowLength);

    // Primo elemento di strtok()
    token = strtok(toUppeFileRow, " ");

    tokenLength = strlen(token);

    // Inizializzazione della prima cella
    strcpy(board[0][0], token);
    board[0][0][tokenLength] = '\0';

    // Elementi successivi
    for(int i = 0 ; i < rows ; i++) {
        for(int j = 0 ; j < columns ; j++) {

            // Salta il primo elemento già inizializzato
            if(i == 0 && j == 0) continue;

            // Elemento corrente di strtok()
            token = strtok(NULL, " ");
            
            tokenLength = strlen(token);

            // Inizializzazione della cella corrente della matrice
            strcpy(board[i][j], token);
            board[i][j][tokenLength] = '\0';

        }
    }

    // Deallocazione stringa della riga in upper case
    free(toUppeFileRow);

}



// Elimina '\n' sostituendolo con '\0'
void del_endl(char* str, int maxLength) {

    for(int i = 0 ; i < maxLength ; i++)
        if(str[i] == '\n') str[i] = '\0';

}



char* to_upper_row(char* fileRow) {

        // Allocazione di una stringa della dimensione di "fileRow" + 1 (terminatore stringa)
        int toMalloc = strlen(fileRow);
        char* toUpperWord = (char*) malloc((toMalloc + 1) * sizeof(char));
        MALLOC_CHECK(toUpperWord);

        for(int i = 0 ; i < toMalloc ; i++)
            
            // L'elemento corrente di "fileRow" e' convertito in uppercase se e' diverso da 'u' o dal carattere spazio,
            // in caso fosse 'u', viene convertito in uppercase solo se l'elemento precedente e' diverso dal carattere spazio
            if((fileRow[i] != 'u' && fileRow[i] != ' ' ) || (fileRow[i] == 'u' && toUpperWord[i-1] == ' '))

                toUpperWord[i] = toupper(fileRow[i]);
                
            else
                toUpperWord[i] = fileRow[i];

        toUpperWord[toMalloc] = '\0';
        return toUpperWord;

}



// Controlla la validità della riga letta dal file delle matrici
int check_file_row(char* fileRow, int rows, int columns) {

    int fileRowLength = strlen(fileRow);                            // Lunghezza della riga del file (con '\n' già rimosso da del_endl())
    int minLength = (rows * columns * 2) - 1;                       // Lunghezza minima contando spazi e considerando solo caratteri singoli

    // Se la lunghezza e' valida restituisce 0, altrimenti -1
    if(fileRowLength < minLength)
        return -1;

    return 0;

}



// Genera una matrice di interi di "rows" righe e "columns" colonne
int** generate_visited_matrix(int rows, int columns) {

    // Allocazione della matrice
    int **visitedMatrix = (int**) malloc(rows * sizeof(int*));
    MALLOC_CHECK(visitedMatrix);

    // Allocazione delle righe di lunghezza "columns"
    for(int i = 0 ; i < rows ; i++) {
        visitedMatrix[i] = (int*) malloc(columns * sizeof(int));
        MALLOC_CHECK(visitedMatrix);
    }

    // Inizializza / resetta la matrice
    reset_visited_matrix(visitedMatrix, rows, columns);
    return visitedMatrix;

}



// Ripristina gli elementi della matrice "visitedMatrix", inizializzandoli a 0
void reset_visited_matrix(int** visitedMatrix, int rows, int columns) {

    for(int i = 0 ; i < rows ; i ++) 
        for(int j = 0 ; j < columns ; j ++) 
            visitedMatrix[i][j] = 0;

}



// Dealloca la matrice "visitedMatrix"
void destroy_visited_matrix(int** visitedMatrix, int rows) {

    for(int i = 0 ; i < rows ; i++)

        // Deallocazione di ogni riga
        free(visitedMatrix[i]);		

    // Deallocazione della matrice
    free(visitedMatrix);

}