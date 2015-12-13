#ifndef RGSSA1_H
#define RGSSA1_H

#include "util.h"

bool rgssa1_readFile(FILE* fp);
bool rgssa1_readInt(RgssaKey* key, uint32_t* value, FILE* fp);
bool rgssa1_readSz(RgssaKey* key, char* value, size_t size, FILE* fp);

bool rgssa1_writeFile(const FileList* fileList, FILE* out);
bool rgssa1_writeInt(RgssaKey* key, uint32_t value, FILE* fp);
bool rgssa1_writeSz(RgssaKey* key, const char* value, FILE* fp);

#endif
