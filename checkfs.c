/*--- checkfs.c ---*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#define SECTOR_SIZE 512
#define BLOCK_SIZE  4096
#define DESCR_SIZE  20
#define SPB         (BLOCK_SIZE / SECTOR_SIZE)

#define INOPB 64 /* inodes per block BLOCK_SIZE / INOSI */
#define INOSI 64 /* size of one inode */

/* file info */
#define IFMT		  070000	/* type of file */
#define IFREG		  040000	/* regular file */
#define IFDIR		  030000	/* directory */
#define IFCHR		  020000	/* character special */
#define IFBLK		  010000	/* block special */
#define IFFREE	  000000	/* reserved (indicates free inode) */
#define ISUID		  004000	/* set user id on execution */
#define ISGID		  002000	/* set group id on execution */
#define ISVTX		  001000	/* save swapped text even after use */
#define UREAD     000400	/* user's read permission */
#define UWRITE		000200	/* user's write permission */
#define UEXEC     000100	/* user's execute permission */
#define GREAD     000040	/* group's read permission */
#define GRWRITE		000020	/* group's write permission */
#define GREXEC		000010	/* group's execute permission */
#define OTREAD		000004	/* other's read permission */
#define OTWRITE		000002	/* other's write permission */
#define OTEXEC		000001	/* other's execute permission */


/*--- error function ---------------------------------------------------------*/
void error(int errorCode, char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  printf("Error: ");
  vprintf(fmt, ap);
  printf("\n");
  va_end(ap);
  exit(errorCode);
}


/*--- get4Byte -----------------------------------------------------------------
 * returns 4 bytes of disk buffer */
unsigned long get4Byte(unsigned char *addr) {
  return (unsigned long) addr[0] << 24 |
         (unsigned long) addr[1] << 16 |
         (unsigned long) addr[2] <<  8 |
         (unsigned long) addr[3] <<  0;
}


/*--- readblock-----------------------------------------------------------------
 * sets pointer of buffer to x */
void readBlock(FILE *disk, uint32_t offset, uint8_t *blockBuffer){
  fseek(disk, offset, SEEK_SET);
  if(fread(blockBuffer, 1, SECTOR_SIZE, disk) != SECTOR_SIZE){
    error(3, "I/O error");
  }
}


/*--- readInodes ---------------------------------------------------------------
 * reads inodes from inode tabelle */
void readInodes(FILE *disk, uint32_t ptrStart, uint8_t *blockBuffer){
  uint32_t numInodes;
  
  /* set pointer to inode-tablle
   * block number 2 == inode tabelle */
  readBlock(disk, ptrStart * SECTOR_SIZE + 1 * BLOCK_SIZE, blockBuffer);
  /*--- test output ----------------------------------------------------------*/
  numInodes = get4Byte(blockBuffer);
  printf("%lu (0x%lX)\n", (unsigned long)numInodes, (unsigned long)numInodes);
  /*--------------------------------------------------------------------------*/
  numInodes = get4Byte(blockBuffer + 20);
  printf("%lu (0x%lX)\n", (unsigned long)numInodes, (unsigned long)numInodes);
}


/*--- main -------------------------------------------------------------------*/
int main(int argc, char *argv[]){
  /* disk */
  FILE *disk;
  uint8_t *diskName;
  uint8_t  ptrNumber;
  uint32_t diskSize;
  uint32_t numSectors;
  /* partition */
  uint8_t  ptrTable[SECTOR_SIZE];
  uint8_t  *ptrEntry;
  uint32_t ptrType;
  uint32_t ptrStart;
  uint32_t ptrSize;
  uint32_t currentBlock;
  uint8_t  blockBuffer[BLOCK_SIZE];
  uint32_t numInodes;
  uint32_t freeInodes;
  uint32_t freeBlocks;

  if (argc != 3) {
    error(1, "Wrong number of arguments.\nUsage: fsc <disk-image>\n");
  }

  /* open disk image, only read and binary mode */
  diskName = (uint8_t *) argv[1];
  ptrNumber = (uint8_t) atoi(argv[2]);
  if(ptrNumber < 0 || ptrNumber > 15){
    error(4, "Illegal partition number %d", ptrNumber);
  }
  disk = fopen((char *)diskName, "rb");
  if(disk == NULL){
    error(2, "cannot open disk image file '%s'", argv[1]);
  }
  
  /* read disk size */
  fseek(disk, 0, SEEK_END);
  diskSize = ftell(disk);
  numSectors = diskSize / SECTOR_SIZE;
  fclose(disk);
  printf("Disk '%s' has %lu (0x%lX) sectors and %d bytes.\n",
          diskName, (unsigned long)numSectors,
          (unsigned long)numSectors, diskSize);
  if(diskSize % SECTOR_SIZE != 0){
    printf("Warning: disk size is not a multiple of sector size!\n");
  }
  
  /* read partition table */
  disk = fopen((char *)diskName, "rb");
  fseek(disk, 1 * SECTOR_SIZE, SEEK_SET);
  if(fread(ptrTable, 1, SECTOR_SIZE, disk) != SECTOR_SIZE){
    error(3, "cannot read partition table from disk image '%s'", diskName);
  }
  
  /* check partition entry */
  ptrEntry = ptrTable + ptrNumber * 32;
  ptrType = get4Byte(ptrEntry + 0);
  if ((ptrType & 0x7FFFFFFF) != 0x00000058) {
    error(5, "Partition %d of disk '%s' does not contain an EOS32 file system",
          ptrNumber, diskName);
  }
  
  /* set partition start, end and print partition size */
  ptrStart = get4Byte(ptrEntry + 4);
  ptrSize = get4Byte(ptrEntry + 8);
  printf("File system has size %lu (0x%lX) sectors of %d bytes each,\n",
         (unsigned long)ptrSize, (unsigned long)ptrSize, SECTOR_SIZE);
  if (ptrSize % SPB != 0) {
    printf("File system size is not a multiple of block size.\n");
  }
  currentBlock = ptrSize / SPB;
  printf("and %lu (0x%lX) blocks of %d bytes each.\n",
         (unsigned long)currentBlock, (unsigned long)currentBlock, BLOCK_SIZE);
  if (currentBlock < 2) {
    error(9, "file system has less than 2 blocks");
  }

  /* partition start and size */
  printf("Partition start: %lu (0x%lX) size: %lu (0x%lX).\n",
         (unsigned long)ptrStart, (unsigned long)ptrStart,
         (unsigned long)ptrSize, (unsigned long)ptrSize);
  /* set pointer to super block 
   * block number 1 == super block */
  fseek(disk, ptrStart * SECTOR_SIZE + 1 * BLOCK_SIZE, SEEK_SET);
  if (fread(blockBuffer, BLOCK_SIZE, 1, disk) != 1) {
    error(9, "cannot read block %lu (0x%lX)",
          (unsigned long)blockBuffer, (unsigned long)blockBuffer);
  }
  numInodes  = get4Byte(blockBuffer + 8);
  freeBlocks = get4Byte(blockBuffer + 12);
  freeInodes = get4Byte(blockBuffer + 16);
  printf("Inode list size %lu (0x%lX) blocks\nfree blocks %lu (0x%lX)\nfree inodes %lu (0x%lX)\n",
         (unsigned long)numInodes, (unsigned long)numInodes,
         (unsigned long)freeBlocks, (unsigned long)freeBlocks,
         (unsigned long)freeInodes, (unsigned long)freeInodes);
  
  /* read inode tablle and interpretation */
  readInodes(disk, ptrStart, blockBuffer);
  
  fclose(disk);
  return 0;
}

