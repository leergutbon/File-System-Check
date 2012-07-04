/*--- checkfs.h ---*/
#define SECTOR_SIZE 512
#define BLOCK_SIZE  4096
#define DESCR_SIZE  20
#define FRELS_SIZE  500
#define SPB         (BLOCK_SIZE / SECTOR_SIZE)
#define DIRPB 64
#define DIRSIZ 60

#define INOPB 64 /* inodes per block BLOCK_SIZE / INOSI */
#define INORE  8 /* ref elements of inode */
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


/* cnt for refs from inodes */
typedef struct blockCnt{
  int file;
  int freelist;
}BlockCnt;

typedef struct inode{
  uint32_t nlnks;
  uint32_t size;
  uint32_t computedSize;
}Inode;

void readDir(uint8_t *blockBuffer, uint32_t block);
void readSingleInode(uint8_t *blockBuffer, uint32_t inodeNumber);
void error(int errorCode, char *fmt, ...);
unsigned long get4Byte(unsigned char *addr);
void readBlock(uint32_t offset, uint8_t *blockBuffer);
void readIndirect(int numRef,
                  BlockCnt *blocks,
                  uint32_t curBlock,
                  uint8_t * blockBuffer);
void readInodes(uint32_t numInodeBlocks,
                uint8_t *blockBuffer);
void goThroughInodes(uint8_t *blockBuffer, uint32_t targetBlock);
