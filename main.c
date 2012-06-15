/*--- main.c ---*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define SECTOR_SIZE	512
#define NPE		(SECTOR_SIZE / sizeof(PartEntry))
#define DESCR_SIZE	20


typedef struct {
  unsigned long type;
  unsigned long start;
  unsigned long size;
  char descr[DESCR_SIZE];
} PartEntry;

PartEntry ptr[NPE];

/*----------------------------------------------------------------------------*/

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

unsigned long getNumber(unsigned char *p) {
  return (unsigned long) *(p + 0) << 24 |
         (unsigned long) *(p + 1) << 16 |
         (unsigned long) *(p + 2) <<  8 |
         (unsigned long) *(p + 3) <<  0;
}

/*----------------------------------------------------------------------------*/

void convertPartitionTable(PartEntry *e, int n) {
  int i;
  unsigned char *p;

  for (i = 0; i < n; i++) {
    p = (unsigned char *) &e[i];
    e[i].type = getNumber(p + 0);
    e[i].start = getNumber(p + 4);
    e[i].size = getNumber(p + 8);
  }
}

/*----------------------------------------------------------------------------*/

int main(int argc, char *argv[]){
  FILE *disk;
  char *diskName;
  unsigned long diskSize;
  unsigned long numSectors;
  unsigned long partLast;
  int i, j;
  char c;
  

  if (argc != 2) {
    error("Wrong amount of arguments.\nUsage: fsc <disk-image>\n");
    exit(1);
  }
  /* open disk image, only read and binary */
  disk = fopen(argv[1], "r+b");
  if (disk == NULL) {
    error("cannot open disk image file '%s'", argv[1]);
  }
  
  /* read image size */
  diskName = argv[1];
  fseek(disk, 0, SEEK_END);
  diskSize = ftell(disk);
  numSectors = diskSize / SECTOR_SIZE;
  printf("Disk '%s' has %lu (0x%lX) sectors.\n", diskName, numSectors, numSectors);
  if (diskSize % SECTOR_SIZE != 0){
    printf("Warning: disk size is not a multiple of sector size!\n");
  }
  fseek(disk, 1 * SECTOR_SIZE, SEEK_SET);
  if (fread(ptr, 1, SECTOR_SIZE, disk) != SECTOR_SIZE) {
    error("cannot read partition table from disk image '%s'", diskName);
  }
  
  /* read partion tables */
  fseek(disk, 1 * SECTOR_SIZE, SEEK_SET);
  if (fread(ptr, 1, SECTOR_SIZE, disk) != SECTOR_SIZE) {
    error("cannot read partition table from disk image '%s'", diskName);
  }

  /* close file */
  fclose(disk);
  
  return 0;
}

