#include <stdlib.h>
#include <fcntl.h>

#include <netdb.h>
#include <stdio.h>

#include "defs.h"

/* open a file to read - file */
void file_read(char* file, int *fd) {

      /* open the file for reading */
      *fd = open(file, O_RDONLY);
      if (*fd == -1) {
           fprintf(stderr, "Could not open file [%s] for reading! (%s)\n", file, strerror(errno));
      }
}


/* create a file to write - target */
void file_write(char* file, int *fd) {
     /* open up a handle to our target file to receive the contents */
     /* from the server */
     *fd = open(file, O_RDWR | O_CREAT, 0666);

     if (*fd == -1) {
         fprintf(stderr, "Could not open target file [%s], using stdout. (%s)\n", file, strerror(errno));
         *fd = 1; //fd=1 is stdout?!?!
     }
}
