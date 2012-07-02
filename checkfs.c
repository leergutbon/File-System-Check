/*--- checkfs.c ---*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include "checkfs.h"

#define SECTOR_SIZE 512
#define BLOCK_SIZE  4096
#define DESCR_SIZE  20
#define SPB         (BLOCK_SIZE / SECTOR_SIZE)
#define DIRPB 64
#define DIRSIZ 60

#define INOPB 64 /* inodes per block BLOCK_SIZE / INOSI */
#define INOSI 64 /* size of one inode */

/* file info */
#define IFMT      070000  /* type of file */
#define IFREG     040000  /* regular file */
#define IFDIR     030000  /* directory */
#define IFCHR     020000  /* character special */
#define IFBLK     010000  /* block special */
#define IFFREE    000000  /* reserved (indicates free inode) */
#define ISUID     004000  /* set user id on execution */
#define ISGID     002000  /* set group id on execution */
#define ISVTX     001000  /* save swapped text even after use */
#define UREAD     000400  /* user's read permission */
#define UWRITE    000200  /* user's write permission */
#define UEXEC     000100  /* user's execute permission */
#define GREAD     000040  /* group's read permission */
#define GRWRITE   000020  /* group's write permission */
#define GREXEC    000010  /* group's execute permission */
#define OTREAD    000004  /* other's read permission */
#define OTWRITE   000002  /* other's write permission */
#define OTEXEC    000001  /* other's execute permission */

uint32_t numInodes;
uint32_t *allInodes;
uint32_t *nlnks;


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
  if(fread(blockBuffer, BLOCK_SIZE, 1, disk) != 1){
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


/* reads directories from the directory/files section */
void readDir(FILE *disk, uint32_t ptrStart, uint8_t *blockBuffer, uint32_t block){
  uint8_t *p;
  int i;
  uint32_t inode;

  readBlock(disk, ptrStart * SECTOR_SIZE + block * BLOCK_SIZE, blockBuffer);
  
  p = blockBuffer;
  /* loop through the content of the directory */
  for (i = 0; i < DIRPB; i++) {
   /* get the next 4 Bytes, which is the inode of
    * this sub-directory/file */
   inode = get4Byte(p);
   /* increment counter of current(i == 0) and parent
    * (i == 1) directory */
   if(i < 2){
    allInodes[inode] += 1;
   }
   if(inode != 0){
    /* if inode is not 0, current or parent directory
     * read this inode */
    if(i >= 2){
      readInode(disk, ptrStart, blockBuffer, inode);
      /* restore blockBuffer */
      readBlock(disk, ptrStart * SECTOR_SIZE + block * BLOCK_SIZE, blockBuffer);
    }
   }
   /* set pointer to the next sub-directory/file */
   p += 64;
  }
}

/* reads the content of a specific inode */
void readInode(FILE *disk, uint32_t ptrStart, uint8_t *blockBuffer, uint32_t inodeNumber){

  FILE *diskBackup;
  uint32_t type;
  uint32_t block;
  uint32_t targetBlock;
  uint32_t targetInode;
  int i;
  
  /* increment the linkcount of the given inode by 1 */
  allInodes[inodeNumber] += 1;
  /* translates the given inode in the exact inode in a block */
  targetBlock = inodeNumber / INOPB + 2;
  targetInode = inodeNumber % INOPB;

  diskBackup = disk;
  readBlock(disk, ptrStart * SECTOR_SIZE + targetBlock * BLOCK_SIZE + targetInode * INOSI, blockBuffer);
  
  /* reads the type of the inode */
  type = get4Byte(blockBuffer);

  if(type != 0){
    if ((type & IFMT) != IFREG && (type & IFMT) != IFDIR && (type & IFMT) != IFCHR && (type & IFMT) != IFBLK) {
        error(18, "illegal type");
    } else if ((type & IFMT) == IFDIR) {
      /* if this inode is a directory loop through all direct
       * blocks of this inode */
      for(i = 32; i <= 52; i += 4){
        /* TODO: Also check for indirect */
        block = get4Byte(blockBuffer + i);
        
        
        if(block != 0){
          /* Jump to the block which was read in the direct block */
          readDir(disk, ptrStart, blockBuffer, block);
          /* restore blockBuffer */
          readBlock(disk, ptrStart * SECTOR_SIZE + targetBlock * BLOCK_SIZE + targetInode * INOSI, blockBuffer);
        }
      }
    }
  }else{
    error(18, "illegal type");
  }
  disk = diskBackup;

}

/* go throug all inodes of the given block and read their link count */
void goThroughInodes(FILE *disk, uint32_t ptrStart, uint8_t *blockBuffer, uint32_t targetBlock){
  int i;
  readBlock(disk, ptrStart * SECTOR_SIZE + targetBlock * BLOCK_SIZE, blockBuffer);
  for(i = 0; i < INOPB; i++){
    nlnks[i + (targetBlock -2)*INOPB] = get4Byte(blockBuffer + 4);
    blockBuffer += 64;
  }
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
  uint32_t freeInodes;
  uint32_t freeBlocks;
  int i;

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
  
  
  /* check the directories */
  allInodes = malloc (sizeof (uint32_t) * (numInodes * INOPB));
  nlnks = malloc(sizeof(uint32_t) * (numInodes * INOPB));
  if(allInodes == NULL){
    error(6, "malloc didn't work.");
  }
  for(i = 0; i < numInodes * INOPB; i++){
    allInodes[i] = 0;
    nlnks[i] = 0;
  }
  readInode(disk, ptrStart, blockBuffer, 1);
  /* it is necessary to drop the count of the root dir by 1 */
  allInodes[1] -= 1;
  
  for(i = 2; i <  numInodes; i++){
    goThroughInodes(disk, ptrStart, blockBuffer, i);
  }


  for(i = 0; i < numInodes * INOPB; i++){
    if(allInodes[i] != nlnks[i]){
      if(nlnks[i] < allInodes[i] && nlnks[i] == 0){
        error(15, "inode with linkcount 0 is in a directory");
      }else if(allInodes[i] == 0){
        error(21, "directory cannot be reached from root");
      }else{
        error(17, "inode doesn't show up in exactly %d directories", nlnks[i]);
      }
    }
  }

  fclose(disk);
  free(allInodes);
  free(nlnks);
  return 0;
}







