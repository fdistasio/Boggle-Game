#ifndef MACROS_HEADER

#define MACROS_HEADER



// Gestione errori System Call che restituiscono -1
#define SYSC(value, command, message) if((value = command) == -1) { perror(message); exit(errno); }



// Gestione errori System Call che restituiscono NULL
#define SYSCN(value, command, message) if((value = command) == NULL) { perror(message); exit(errno); }



// Gestione errori Thread
#define SYST(command) if(command == -1) { perror("Thread Error"); exit(errno); }



// Gestione errori malloc()
#define MALLOC_CHECK(value) if(value == NULL) { perror("Malloc Error"); exit(errno); }



// Gestione errori FILE
#define FILE_CHECK(value) if(value == NULL) { perror("File Error"); exit(errno); }



#endif