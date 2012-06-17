/*--- checkfs.c ---*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#define SECTOR_SIZE 512
#define DESCR_SIZE  20
#define NPE         (SECTOR_SIZE / sizeof(PartEntry))


typedef struct {
  uint32_t type;
  uint32_t start;
  uint32_t size;
  uint8_t descr[DESCR_SIZE];
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

unsigned long getNumber(uint8_t *p) {
  return (uint32_t) *(p + 0) << 24 |
         (uint32_t) *(p + 1) << 16 |
         (uint32_t) *(p + 2) <<  8 |
         (uint32_t) *(p + 3) <<  0;
}

void convertPartitionTable(PartEntry *e, uint32_t n) {
  uint32_t i;
  uint8_t *p;

  for (i = 0; i < n; i++) {
    p = (uint8_t *) &e[i];
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
  uint32_t i, j;

  /* only for print partition table */
  /*uint32_t partLast;
  uint8_t c;*/

  if (argc != 2) {
    error("Wrong number of arguments.\nUsage: fsc <disk-image>\n");
  }

  /* open disk image, only read and binary */
  diskName = (uint8_t *)argv[1];
  disk = fopen((char *)diskName, "rb");
  if(disk == NULL){
    error("cannot open disk image file '%s'", argv[1]);
  }
  
  /* read image size */
  fseek(disk, 0, SEEK_END);
  diskSize = ftell(disk);
  numSectors = diskSize / SECTOR_SIZE;
  fclose(disk);
  printf("Disk '%s' has %lu (0x%lX) sectors.\n",
          diskName, (unsigned long)numSectors, (unsigned long)numSectors);
  if(diskSize % SECTOR_SIZE != 0){
    printf("Warning: disk size is not a multiple of sector size!\n");
  }
    
  disk = fopen((char *)diskName, "rb");
  /* read partition table */
  fseek(disk, 1 * SECTOR_SIZE, SEEK_SET);
  if(fread(ptr, 1, SECTOR_SIZE, disk) != SECTOR_SIZE){
    error("cannot read partition table from disk image '%s'", diskName);
  }
  fclose(disk);

  convertPartitionTable(ptr, NPE);
  /* print partition table */
  /*printf("Partitions:\n");
  printf(" # b type       start      last       size       description\n");
  for (i = 0; i < NPE; i++) {
    if (ptr[i].type != 0) {
      partLast = ptr[i].start + ptr[i].size - 1;
    } else {
      partLast = 0;
    }
    printf("%2d %s 0x%08lX 0x%08lX 0x%08lX 0x%08lX ",
           i,
           (unsigned long)ptr[i].type & 0x80000000 ? "*" : " ",
           (unsigned long)ptr[i].type & 0x7FFFFFFF,
           (unsigned long)ptr[i].start,
           (unsigned long)partLast,
           (unsigned long)ptr[i].size);
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
  }*/

  /* read partition */
  j=0;
  for(i=0; i<NPE; i++){
    if(ptr[i].type != 0){
      j++;
      /* checks partion for EOS32 partition type 
       * 88 is decimal, EOS32 have 0x00000058 as partition type
       * 88 == 0x58 */
      if(ptr[i].type == 0){
        /* nothing to do, next partion */
      }else if(((unsigned long)ptr[i].type & 0x7FFFFFFF) == 89){
        printf("Warning: Partion %2d is a EOS32 swap partition, can't check this type.\n", i);
      }else if(((unsigned long)ptr[i].type & 0x7FFFFFFF) == 88){
        /* check partion */
        printf("Check partition %2d.\n", i);
      }else{
        printf("Warning: %2d unknown partition type.\n", i);
      }
    }
  }
  printf("cnt: %d\n", j);
  
  
  return 0;
}

