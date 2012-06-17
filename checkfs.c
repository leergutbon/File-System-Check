/*--- checkfs.c ---*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define SECTOR_SIZE 512
#define DESCR_SIZE  20
#define NPE         (SECTOR_SIZE / sizeof(PartEntry))


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

unsigned long getNumber(unsigned char *p) {
  return (unsigned long) *(p + 0) << 24 |
         (unsigned long) *(p + 1) << 16 |
         (unsigned long) *(p + 2) <<  8 |
         (unsigned long) *(p + 3) <<  0;
}


void convertPartitionTable(PartEntry *e, int n) {
  int i;
  unsigned char *p;

  for (i = 0; i < n; i++) {
    p = (unsigned char *) &e[i];
    e[i].type  = getNumber(p + 0);
    e[i].start = getNumber(p + 4);
    e[i].size  = getNumber(p + 8);
  }
}

/*----------------------------------------------------------------------------*/

int main(int argc, char *argv[]){
  FILE *disk;
  uint8_t *diskName;
  uint32_t diskSize;
  uint32_t numSectors;
  uint32_t partLast;
  uint32_t i, j;
  uint8_t c;

  if (argc != 2) {
    error("Wrong number of arguments.\nUsage: fsc <disk-image>\n");
  }

  /* open disk image, only read and binary */
  diskName = argv[1];
  disk = fopen(diskName, "rb");
  if(disk == NULL){
    error("cannot open disk image file '%s'", argv[1]);
  }
  
  /* read image size */
  fseek(disk, 0, SEEK_END);
  diskSize = ftell(disk);
  numSectors = diskSize / SECTOR_SIZE;
  fclose(disk);
  printf("Disk '%s' has %lu (0x%lX) sectors.\n",
          diskName, numSectors, numSectors);
  if(diskSize % SECTOR_SIZE != 0){
    printf("Warning: disk size is not a multiple of sector size!\n");
  }
    
  disk = fopen(diskName, "rb");
  /* read partition table */
  fseek(disk, 1 * SECTOR_SIZE, SEEK_SET);
  if(fread(ptr, 1, SECTOR_SIZE, disk) != SECTOR_SIZE){
    error("cannot read partition table from disk image '%s'", diskName);
  }
  fclose(disk);
printf("size: %d\n", sizeof(ptr));

  convertPartitionTable(ptr, NPE);
  /* show partition table */
  printf("Partitions:\n");
  printf(" # b type       start      last       size       description\n");
  




  for (i = 0; i < NPE; i++) {
    if (ptr[i].type != 0) {
      partLast = ptr[i].start + ptr[i].size - 1;
    } else {
      partLast = 0;
    }
    printf("%2d %s 0x%08lX 0x%08lX 0x%08lX 0x%08lX ",
           i,
           ptr[i].type & 0x80000000 ? "*" : " ",
           ptr[i].type & 0x7FFFFFFF,
           ptr[i].start,
           partLast,
           ptr[i].size);
    for (j = 0; j < DESCR_SIZE; j++) {
      c = ptr[i].descr[j];
      if (c == '\0') {
        break;
      }
      if (c >= 0x20 && c < 0x7F) {
        printf("%c", c);
      } else {
        printf(".");
      }
    }
    printf("\n");
  }
  
  return 0;
}
