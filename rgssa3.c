#include "rgssa3.h"

/*
 * READ
 */

bool rgssa3_readInt(RgssaKey key, uint32_t* value, FILE* fp) {
    uint32_t rvalue;
    if(fread(&rvalue, 4, 1, fp) != 1) {
        return true;
    }
    *value = rvalue ^ key.i;

    return false;
}

bool rgssa3_readSz(RgssaKey key, char* value, size_t size, FILE* fp) {
    for(unsigned int i = 0; i < size; i++) {
        char c;
        if(fread(&c, 1, 1, fp) != 1) {
            return true;
        }
        value[i] = c ^ key.c[i % 4];
        if(value[i] == '\\') {
            value[i] = '/';
        }
    }
    value[size] = 0;
    return false;
}

bool rgssa3_readFile(FILE* fp) {
    RgssaKey key;
    if(fread(&key, 4, 1, fp) != 1) {
        return true;
    }
    
    key.i *= 9;
    key.i += 3;

    for(;;) {
        //Read misc data
        uint32_t offset, size, sizeName;
        RgssaKey keyFile;
        if(rgssa3_readInt(key, &offset, fp) ||
           rgssa3_readInt(key, &size, fp) ||
           rgssa3_readInt(key, &keyFile.i, fp) ||
           rgssa3_readInt(key, &sizeName, fp)) {
            return true;
        }
        if(!offset) {
            break;
        }

        //Read name
        char* szName = (char*)malloc(sizeName + 1);
        if(rgssa3_readSz(key, szName, sizeName, fp)) {
            free(szName);
            return true;
        }

        //Extract the file
        size_t cursor = ftell(fp);
        fseek(fp, offset, SEEK_SET);
        if(rgssa_extract(keyFile, szName, size, fp)) {
            free(szName);
            return true;
        }
        free(szName);

        fseek(fp, cursor, SEEK_SET);
    }
    return false;
}

/*
 * WRITE
 */

bool rgssa3_writeInt(RgssaKey key, uint32_t value, FILE* fp) {
    value ^= key.i;
    if(fwrite(&value, 4, 1, fp) != 1) {
        return true;
    }

    return false;
}

bool rgssa3_writeSz(RgssaKey key, const char* value, FILE* fp) {
    for(int i = 0; value[i]; i++) {
        char c = ((value[i] == '/') ? '\\' : value[i]) ^ key.c[i % 4];
        if(fwrite(&c, 1, 1, fp) != 1) {
            return true;
        }
    }
    return false;
}

bool rgssa3_writeFile(FileList* fileList, FILE* out) {
    RgssaKey key;
    key.i = rand_key();
    if(fwrite(&key, 4, 1, out) != 1) {
        return true;
    }
    key.i *= 9;
    key.i += 3;
    
    //Calculate initial embedded file offset
    uint32_t embedOffset = 12 + (fileList->nFiles + 1) * (4 * 4);
    for(int i = 0; i < fileList->nFiles; i++) {
        embedOffset += strlen(fileList->files[i].name);
    }

    //Write the file info (16 bytes + length of name)
    for(int i = 0; i < fileList->nFiles; i++) {
        //Generate a random key to encrypt this file
        fileList->files[i].key.i = rand_key();
        
        //Write misc data
        uint32_t sizeName = strlen(fileList->files[i].name);
        if(rgssa3_writeInt(key, embedOffset, out) ||
           rgssa3_writeInt(key, fileList->files[i].size, out) ||
           rgssa3_writeInt(key, fileList->files[i].key.i, out) ||
           rgssa3_writeInt(key, sizeName, out)) {
            return true;
        }
        //Write name
        if(rgssa3_writeSz(key, fileList->files[i].name, out)) {
            return true;
        }
        
        //Update file offset
        embedOffset += fileList->files[i].size;
    }
    //Write "eof" entry
    for(int i = 0; i < 4; i++) {
        rgssa3_writeInt(key, 0, out);
    }
    //Now embed the file data
    for(int i = 0; i < fileList->nFiles; i++) {
        rgssa_embed(fileList->files[i].key, fileList->files[i].name,
              fileList->files[i].size, out);
    }
    
    return false;
}
