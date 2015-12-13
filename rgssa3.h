#ifndef RGSSA3_H
#define RGSSA3_H

#include "util.h"

bool rgssa3_readFile(FILE* fp);
bool rgssa3_readInt(RgssaKey key, uint32_t* value, FILE* fp);
bool rgssa3_readSz(RgssaKey key, char* value, size_t size, FILE* fp);

bool rgssa3_writeFile(FileList* fileList, FILE* out);
bool rgssa3_writeInt(RgssaKey key, uint32_t value, FILE* fp);
bool rgssa3_writeSz(RgssaKey key, const char* value, FILE* fp);

#endif
