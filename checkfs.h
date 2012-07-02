void readDir(FILE *disk, uint32_t ptrStart, uint8_t *blockBuffer, uint32_t block);
void readInode(FILE *disk, uint32_t ptrStart, uint8_t *blockBuffer, uint32_t inodeNumber);