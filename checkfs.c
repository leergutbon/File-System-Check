/*--- checkfs.c ---*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include "checkfs.h"


Inode *allInodes;
uint32_t *nlnks;
FILE *disk;
uint32_t ptrStart;
uint32_t numBlocks;


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
void readBlock(uint32_t offset, uint8_t *blockBuffer){
  fseek(disk, offset, SEEK_SET);
  if(fread(blockBuffer, 1, BLOCK_SIZE, disk) != BLOCK_SIZE){
    error(3, "I/O error");
  }
}


/*--- readIndirect -------------------------------------------------------------
 * read the two indirect blocks of inode */
void readIndirect(int numRef,
                  BlockCnt *blocks,
                  uint32_t curBlock,
                  uint8_t * blockBuffer){
  int cnt;
  unsigned long offset;
  uint32_t val4Byte;
  
  offset = 0;
  /* second to last single indirect */
  if(numRef == INORE-2){
    /* 4 byte for every entry in indirect block */
    for(cnt=0; cnt < BLOCK_SIZE/sizeof(uint32_t); cnt++){
      val4Byte = get4Byte(blockBuffer + offset);
      if(val4Byte > 0 && val4Byte < numBlocks){
        blocks[val4Byte].file += 1;
      }
      offset += 4;
    }
  }else if(numRef == INORE-1){ /* last double indirect */
    for(cnt=0; cnt < BLOCK_SIZE/sizeof(uint32_t); cnt++){
      val4Byte = get4Byte(blockBuffer + offset);
      blocks[val4Byte].file += 1;
      /* second indirect block */
      readBlock(ptrStart * SECTOR_SIZE + val4Byte * BLOCK_SIZE, blockBuffer);
      readIndirect(numRef-1, blocks, curBlock, blockBuffer);
      /* first indirect block */
      readBlock(ptrStart * SECTOR_SIZE + curBlock * BLOCK_SIZE, blockBuffer);
      offset += 4;
    }
  }
}


/* follow indirect blocks */
void followIndirect(uint32_t block, uint32_t numBlocks, uint8_t *blockBuffer, uint32_t inodeNumber, uint32_t type){
  uint32_t val4Byte;
  int cnt, offset;
  if(block != 0 && block < numBlocks){
    /* set blockBuffer to the target block */
    offset = 0;
    for(cnt=0; cnt < BLOCK_SIZE/sizeof(uint32_t); cnt++){
      readBlock(ptrStart * SECTOR_SIZE + block * BLOCK_SIZE, blockBuffer);
      val4Byte = get4Byte(blockBuffer + offset);
      if(val4Byte > 0 && val4Byte < numBlocks){
        allInodes[inodeNumber].computedSize += BLOCK_SIZE;
        if((type & IFMT) == IFDIR){
          readDir(blockBuffer, val4Byte);
        }
      }else if(block >= numBlocks){
        error(99, "Block %lu is out of range", (unsigned long)val4Byte);
      }
      offset += 4;
    }
    
  }else if(block >= numBlocks){
    error(99, "Block %lu is out of range", (unsigned long)block);
  }
}

/*--- readLinkBlock ------------------------------------------------------------
 * reads link blocks of free list */
void readLinkBlock(BlockCnt *blocks,
                   uint8_t *blockBuffer,
                   int curBlock,
                   int offset){
  int i;
  uint32_t val4Byte;
  uint32_t sizeFreeList;
  
  /* first 4 bytes size of link block */
  sizeFreeList = get4Byte(blockBuffer + offset);
  
  /* first entry of list is link block */
  offset += 4;
  val4Byte = get4Byte(blockBuffer + offset);
  
  /* first element of free list is link block */
  if(val4Byte != 0 && val4Byte < numBlocks){
    blocks[val4Byte].freelist += 1;
    readBlock(ptrStart * SECTOR_SIZE + val4Byte * BLOCK_SIZE, blockBuffer);
    readLinkBlock(blocks, blockBuffer, val4Byte, 0);
    readBlock(ptrStart * SECTOR_SIZE + curBlock * BLOCK_SIZE, blockBuffer);
  }
  
  offset += 4;
  /* go through free list */
  for(i=1; i<sizeFreeList; i++){
    val4Byte = get4Byte(blockBuffer + offset);
    /*printf("%lu\n", get4Byte(blockBuffer+offset));*/
    if(val4Byte != 0){
      if(val4Byte > 0 && val4Byte < numBlocks){
        blocks[val4Byte].freelist += 1;
      }else if(val4Byte >= numBlocks){
        error(99, "Block %lu is out of range", (unsigned long)val4Byte);
      }
    }
    offset += 4;
  }
}
 
 
/*--- readInodes ---------------------------------------------------------------
 * reads inodes from inode tabelle */
void readInodes(uint32_t numInodeBlocks,
                uint8_t *blockBuffer){
  /* see description BlockCnt */
  BlockCnt *blocks;
  int i, j, z, offset, curBlock;
  uint32_t val4Byte;
  uint32_t type;

  /* cnt for ref in inode and free list */
  blocks = malloc(sizeof(BlockCnt) * numBlocks);
  if(blocks == NULL) error(6, "Out of memory.");

  /* set pointer to inode-tablle
   * block number 2 begining of inode tabelle */
  curBlock = 2;
  for(i=0; i<numInodeBlocks; i++){
    readBlock(ptrStart * SECTOR_SIZE + curBlock * BLOCK_SIZE, blockBuffer);
    offset = 0;/*32;*/
    /* go through all inodes in block */
    for(j=0; j<INOPB; j++){
      type = get4Byte(blockBuffer + offset);
      offset += 32;
      if((type & IFMT) != IFCHR && (type & IFMT) != IFBLK){
        /* go through all block refs in inode */
        for(z=0; z<INORE; z++){
          val4Byte = get4Byte(blockBuffer + offset);
          if(z < INORE-2){ /* direct refs */
            if(val4Byte >= 0 && val4Byte < numBlocks){
              if(val4Byte != 0) blocks[val4Byte].file += 1;
            }else{
              error(99, "Block %lu is out of range", (unsigned long)val4Byte);
            }
          }else if(z >= INORE-2 && z < INORE){ /* indirect refs */
            if(val4Byte != 0){
              blocks[val4Byte].file += 1;
              /* go to indirect block */
              readBlock(ptrStart * SECTOR_SIZE + val4Byte * BLOCK_SIZE, blockBuffer);
              readIndirect(z, blocks, val4Byte, blockBuffer);
              /* go back to inode block */
              readBlock(ptrStart * SECTOR_SIZE + curBlock * BLOCK_SIZE, blockBuffer);
            }
          }
          offset += 4;
        }
      }else{
        offset += 32;
      }
    }
    /* next block */
    curBlock++;
  }
  
  curBlock = 1;
  /* check free list, position in super block */
  readBlock(ptrStart * SECTOR_SIZE + curBlock * BLOCK_SIZE, blockBuffer);
  /* start of free block */
  offset = 24 + get4Byte(blockBuffer+20)*4;
  readLinkBlock(blocks, blockBuffer, curBlock, offset);
  
  /* prints boot, super block and inode tabelle */
  /*for(i=0; i<numInodeBlocks+2; i++){
    printf("%lu %lu\n", (unsigned long)blocks[i].file, (unsigned long)blocks[i].freelist);
  }*/

  for(i=numInodeBlocks+2; i<numBlocks; i++){
    if(blocks[i].file == 0 && blocks[i].freelist == 0){
      error(10, "Block %lu is not in file and free list.", (unsigned long)i);
    }
    if(blocks[i].file == 1 && blocks[i].freelist == 1){
      error(11, "Block %lu is in file and free list.", (unsigned long)i);
    }
    if(blocks[i].freelist > 1){
      error(12, "Block %lu is more than one in free list.", (unsigned long)i);
    }
    if(blocks[i].file > 1){
      error(13, "Block %lu is more than one in files", (unsigned long)i);
    }
  }
}


/* reads directories from the directory/files section */
void readDir(uint8_t *blockBuffer, uint32_t block){
  uint8_t *p;
  int i;
  uint32_t inode, name;

  readBlock(ptrStart * SECTOR_SIZE + block * BLOCK_SIZE, blockBuffer);
  
  p = blockBuffer;
  /* loop through the content of the directory */
  for (i = 0; i < DIRPB; i++) {
   /* get the next 4 Bytes, which is the inode of
    * this sub-directory/file */
   inode = get4Byte(p);
   name = get4Byte(p + 4);

   /* increment counter of current(i == 0) and parent
    * (i == 1) directory */
   if(i == 0 && 771751936 == name){
    allInodes[inode].nlnks += 1;
   }
   if(i == 1 && 774766592 == name){
    allInodes[inode].nlnks += 1;
   }
   if(inode != 0){
    /* if inode is not 0, current or parent directory
     * read this inode */
    if(i >= 2 || (name != 771751936 && name != 774766592 && inode != 0)){
      readSingleInode(blockBuffer, inode);
      /* restore blockBuffer */
      readBlock(ptrStart * SECTOR_SIZE + block * BLOCK_SIZE, blockBuffer);
    }
   }
   /* set pointer to the next sub-directory/file */
   p += 64;
  }
}


/* reads the content of a specific inode */
void readSingleInode(uint8_t *blockBuffer, uint32_t inodeNumber){

  FILE *diskBackup;
  uint32_t type;
  uint32_t block, val4Byte;
  uint32_t targetBlock;
  uint32_t targetInode;
  int i, offset;
  
  offset = 0;
  /* increment the linkcount of the given inode by 1 */
  allInodes[inodeNumber].nlnks += 1;
  /* translates the given inode in the exact inode in a block */
  targetBlock = inodeNumber / INOPB + 2;
  targetInode = inodeNumber % INOPB;

  diskBackup = disk;
  readBlock(ptrStart * SECTOR_SIZE + targetBlock * BLOCK_SIZE + targetInode * INOSI, blockBuffer);
  
  /* reads the type of the inode */
  type = get4Byte(blockBuffer);
  /* check if this is the root directory and if so check if it is a directory */
  if(inodeNumber == 1 && (type & IFMT) != IFDIR){
    error(20, "root inode is not a directory");
  }
  /* reads the size of this inode */
  allInodes[inodeNumber].size = get4Byte(blockBuffer + 28);
  if(type != 0){
    if ((type & IFMT) != IFREG && (type & IFMT) != IFDIR && (type & IFMT) != IFCHR && (type & IFMT) != IFBLK) {
        error(18, "illegal type");
    } else if ((type & IFMT) == IFDIR || (type & IFMT) == IFREG) {
      /* if this inode is a directory or reg file loop through all direct
       * blocks of this inode */
      for(i = 32; i <= 52; i += 4){
        block = get4Byte(blockBuffer + i);
        
        if(block != 0){
          /* increment the computed Size */
          allInodes[inodeNumber].computedSize += BLOCK_SIZE;
          /* Jump to the block which was read in the direct block */
          if((type & IFMT) == IFDIR){
            if(block > 0 && block < numBlocks){
              readDir(blockBuffer, block);
            }else if(block >= numBlocks){
              error(99, "Block %lu is out of range", (unsigned long)block);
            }
          }
          /* restore blockBuffer */
          readBlock(ptrStart * SECTOR_SIZE + targetBlock * BLOCK_SIZE + targetInode * INOSI, blockBuffer);
        }
      }

      /* check single indirect */
      block = get4Byte(blockBuffer + 56);
      if(block != 0 && block < numBlocks){
        followIndirect(block, numBlocks, blockBuffer, inodeNumber, type);
      }else if(block > numBlocks){
        error(99, "Block %lu is out of range", (unsigned long)block);
      }
      /* restore blockBuffer */
      readBlock(ptrStart * SECTOR_SIZE + targetBlock * BLOCK_SIZE + targetInode * INOSI, blockBuffer);
      

      /* check double indirect */
      block = get4Byte(blockBuffer + 60);
      if(block != 0 && block < numBlocks){
        /* set blockBuffer to the target block */
        readBlock(ptrStart * SECTOR_SIZE + block * BLOCK_SIZE, blockBuffer);
        for(i=0; i < BLOCK_SIZE/sizeof(uint32_t); i++){
          val4Byte = get4Byte(blockBuffer + offset);
          followIndirect(val4Byte, numBlocks, blockBuffer, inodeNumber, type);
          /* restore blockBuffer */
          readBlock(ptrStart * SECTOR_SIZE + block * BLOCK_SIZE, blockBuffer);
          offset += 4;
        }
      }else if(block > numBlocks){
        error(99, "Block %lu is out of range", (unsigned long)block);
      }
      /* restore blockBuffer */
      readBlock(ptrStart * SECTOR_SIZE + targetBlock * BLOCK_SIZE + targetInode * INOSI, blockBuffer);
    }else if((type & IFMT) == IFCHR ||  (type & IFMT) == IFBLK){

    }else{
      error(18, "illegal type");
    }

  }else{
    error(19, "inode is in a directory but not free");
  }
  disk = diskBackup;

}


/* go throug all inodes of the given block and read their link count */
void goThroughInodes(uint8_t *blockBuffer, uint32_t targetBlock){
  int i;
  readBlock(ptrStart * SECTOR_SIZE + targetBlock * BLOCK_SIZE, blockBuffer);
  for(i = 0; i < INOPB; i++){
    nlnks[i + (targetBlock -2)*INOPB] = get4Byte(blockBuffer + 4);
    blockBuffer += 64;
  }
}


/*--- main -------------------------------------------------------------------*/
int main(int argc, char *argv[]){
  /* disk */
  uint8_t *diskName;
  uint8_t  ptrNumber;
  uint32_t diskSize;
  uint32_t numSectors;
  /* partition */
  uint8_t  ptrTable[SECTOR_SIZE];
  uint8_t  *ptrEntry;
  uint32_t ptrType;
  uint32_t ptrSize;
  uint8_t  blockBuffer[BLOCK_SIZE];
  uint32_t numInodeBlocks;
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
    error(3, "Cannot read partition table from disk image '%s'", diskName);
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
  numBlocks = ptrSize / SPB;
  printf("and %lu (0x%lX) blocks of %d bytes each.\n",
         (unsigned long)numBlocks, (unsigned long)numBlocks, BLOCK_SIZE);
  if (numBlocks < 2) {
    error(9, "File system has less than 2 blocks.");
  }

  /* partition start and size */
  printf("Partition start: %lu (0x%lX) size: %lu (0x%lX).\n",
         (unsigned long)ptrStart, (unsigned long)ptrStart,
         (unsigned long)ptrSize, (unsigned long)ptrSize);
  /* set pointer to super block 
   * block number 1 == super block */
  fseek(disk, ptrStart * SECTOR_SIZE + 1 * BLOCK_SIZE, SEEK_SET);
  if (fread(blockBuffer, BLOCK_SIZE, 1, disk) != 1) {
    error(9, "Cannot read block %lu (0x%lX)",
          (unsigned long)blockBuffer, (unsigned long)blockBuffer);
  }
  numInodeBlocks = get4Byte(blockBuffer + 8);
  freeBlocks = get4Byte(blockBuffer + 12);
  freeInodes = get4Byte(blockBuffer + 16);
  printf("Inode list size %lu (0x%lX) blocks\nFree blocks %lu (0x%lX)\nFree inodes %lu (0x%lX)\n",
         (unsigned long)numInodeBlocks, (unsigned long)numInodeBlocks,
         (unsigned long)freeBlocks, (unsigned long)freeBlocks,
         (unsigned long)freeInodes, (unsigned long)freeInodes);
  
  /* read inode tablle and interpretation */
  readInodes(numInodeBlocks, blockBuffer);
  
  
  /* check the directories */
  allInodes = malloc (sizeof (Inode) * (numInodeBlocks * INOPB));
  nlnks = malloc(sizeof(uint32_t) * (numInodeBlocks * INOPB));
  if(allInodes == NULL){
    error(6, "Malloc didn't work, maybe memory is full.");
  }
  for(i = 0; i < numInodeBlocks * INOPB; i++){
    allInodes[i].nlnks = 0;
    allInodes[i].size = 0;
    nlnks[i] = 0;
  }
  /* start reading from root inode */
  readSingleInode(blockBuffer, 1);
  /* it is necessary to drop the count of the root dir by 1 */
  allInodes[1].nlnks -= 1;
  
  for(i = 2; i <  numInodeBlocks; i++){
    goThroughInodes(blockBuffer, i);
  }


  for(i = 0; i < numInodeBlocks * INOPB; i++){

    if(allInodes[i].nlnks != nlnks[i]){
      if(nlnks[i] < allInodes[i].nlnks && nlnks[i] == 0){
        error(15, "Inode with linkcount 0 is in a directory");
      }else if(allInodes[i].nlnks == 0){
        error(21, "Directory cannot be reached from root");
      }else{
        error(17, "Inode doesn't show up in exactly %d directories", nlnks[i]);
      }
    }
  }

  for(i = 0; i < numInodeBlocks * INOPB; i++){
    if(allInodes[i].size != allInodes[i].computedSize){
      if(!(allInodes[i].size < allInodes[i].computedSize && allInodes[i].size > (allInodes[i].computedSize - BLOCK_SIZE))){
        error(14, "Size of this inode is not right");
      }
      
    }
    if(allInodes[i].nlnks == 0 && allInodes[i].computedSize > 0){
        error(16, "Inode with linkcount 0 is not free");
    }
  }

  fclose(disk);
  free(allInodes);
  free(nlnks);
  return 0;
}
