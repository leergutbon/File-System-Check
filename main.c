/*--- main.c ---*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define SSIZE	512	/* disk sector size in bytes */

/* error function */
void error(char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  printf("Error: ");
  vprintf(fmt, ap);
  printf("\n");
  va_end(ap);
  exit(1);
}

/*----------------------------------------------------------------------------*/

int main(int argc, char *argv[]){
FILE *disk;
  unsigned long fsStart;	/* file system start sector */
  unsigned long fsSize;		/* file system size in sectors */
  int part;
  char *endptr;
  unsigned char partTable[SSIZE];
  unsigned char *ptptr;
  unsigned long partType;
  unsigned long offset;
  unsigned int value;
  unsigned char valBuf[4];
  int width;

  if (argc != 2) {
    error("Wrong amount of arguments.\nUsage: fsc <disk-image>\n");
    exit(1);
  }
  /* open disk image */
  disk = fopen(argv[1], "r+b");
  if (disk == NULL) {
    error("cannot open disk image file '%s'", argv[1]);
  }
 
  /* close file */
  fclose(disk);
  return 0;
}

