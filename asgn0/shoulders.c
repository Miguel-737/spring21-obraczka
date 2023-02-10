// Author:  Miguel Avalos
// Cruzid:  1704078
// Date:    4.11.2021
// File:    shoulders.c
// Purpose: Print head of source up to
//          a provided length

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 100

// will print up to the required size characters
int headprint(int source, int size){
    char buffer[BUFFER_SIZE];
    int characters = BUFFER_SIZE;

    //while the there is characters to be written
    //or characters being read
    while (size>0 && characters != 0){

        //reading to buffer, check for error
        characters = read(source, buffer, BUFFER_SIZE);
        if (characters < 0) {
            //if standard input is the source
            if (source == 0)
                fprintf(stderr,"shoulders: Error reading from stdin\n");
            return 2;
        }

        //assign characters to be wrriten:
        //all in buffer or partial
        int wrtsize = characters;
        if (characters>=size) wrtsize = size;

        //writting to stdout, check for error
        if (write(1, buffer, wrtsize) != wrtsize) {
            fprintf(stderr, 
            "shoulders: Error writing to stdout\n");
            return 3;
        }

        //reduce needed characters to print
        size -= characters;
    }

    //no errors
    return 0;
}

//handle file options
//specific return values are for dubugging
int filesource(char *filename, int size){   

    //open file
    int filedesc = open(filename, O_RDONLY, S_IRWXU);
    
    //if the file does not exist
    if(filedesc < 0){
        fprintf(stderr, 
        "shoulders: cannot open file '%s'",
        filename);
        fprintf(stderr, ": No such file or directory\n");
        return 1;
    }

    // reading error messege
    if (headprint(filedesc, size) == 2) {
        fprintf(stderr,
        "shoulders: Error reading from '%s'\n",
        filename);
        close(filedesc);
        return 2;
    }

    //error closing file
    if (close(filedesc)<0) return 4;

    //no errors
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc<2) {
        fprintf(stderr, 
        "shoulders: requires an argument for bytes to be printed\n");
        exit(EXIT_FAILURE);
    }

    //assigning name to reduce complexity 
    char *num_bytes = argv[1];
    //if num_bytes contains characters
    //then it contains an invalid number
    //of bytes entry
    for(int i = 0; i<strlen(num_bytes); ++i){
        if(!isdigit(num_bytes[i])){
            fprintf(stderr, 
            "shoulders: invalid number of bytes: ‘%s’\n",
            num_bytes);
            exit(EXIT_FAILURE);
        }
    }

    //number of max bytes printed
    int size = atoi(argv[1]);
    char in[] = "-\0";

    //if files or stdin are provided as
    //arguments prints head of them
    for (int i = 2;i<argc;i++){
        if (!strcmp(in, argv[i])){
            headprint(0, size);
        }
        else {
            filesource(argv[i], size);
        }
    }

    //if no arguments are entered after number
    //read from standard input, check for error
    if (argc<3) headprint(0, size);

    exit(EXIT_SUCCESS);
}