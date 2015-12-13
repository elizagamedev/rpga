#ifndef WOLF_H
#define WOLF_H

#include "util.h"

#define WOLF_KEY_SIZE 12
extern const unsigned char WOLF_KEY[WOLF_KEY_SIZE];

#define WOLF_HEADER_SIZE 4
extern const unsigned char WOLF_HEADER[WOLF_HEADER_SIZE];

bool wolf_readFile(FILE* fp, size_t sizeFile);
bool wolf_writeFile(FileList* fileList, FILE* out);

#endif
