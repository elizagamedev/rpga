#include "rgssa1.h"

#define RGSSA1_KEY 0xDEADCAFE

/*
 * READ
 */

bool rgssa1_readInt(RgssaKey* key, uint32_t* value, FILE* fp) {
    uint32_t rvalue;
    if(fread(&rvalue, 4, 1, fp) != 1) {
        return true;
    }
    *value = rvalue ^ key->i;

    //Change key
    key->i *= 7;
    key->i += 3;

    return false;
}

bool rgssa1_readSz(RgssaKey* key, char* value, size_t size, FILE* fp) {
    for(unsigned int i = 0; i < size; i++) {
        char c;
        if(fread(&c, 1, 1, fp) != 1) {
            return true;
        }
        value[i] = c ^ key->c[0];
        if(value[i] == '\\') {
            value[i] = '/';
        }

        key->i *= 7;
        key->i += 3;
    }
    value[size] = 0;
    return false;
}

bool rgssa1_readFile(FILE* fp) {
    RgssaKey key;
    key.i = RGSSA1_KEY;
    
    fseek(fp, 0, SEEK_END);
    size_t sizeFile = ftell(fp);
    fseek(fp, 8, SEEK_SET);

    for(;;) {
        //Read name
        uint32_t sizeName;
        if(rgssa1_readInt(&key, &sizeName, fp)) {
            return true;
        }

        char* szName = (char*)malloc(sizeName + 1);
        if(rgssa1_readSz(&key, szName, sizeName, fp)) {
            free(szName);
            return true;
        }

        //Read size
        uint32_t size;
        if(rgssa1_readInt(&key, &size, fp)) {
            free(szName);
            return true;
        }

        //Extract the file
        if(rgssa_extract(key, szName, size, fp)) {
            free(szName);
            return true;
        }
        free(szName);

        if(ftell(fp) == sizeFile) {
            break;
        }
    }
    return false;
}

/*
 * WRITE
 */
bool rgssa1_writeInt(RgssaKey* key, uint32_t value, FILE* fp) {
    value ^= key->i;
    if(fwrite(&value, 4, 1, fp) != 1) {
        return true;
    }

    //Change key
    key->i *= 7;
    key->i += 3;

    return false;
}

bool rgssa1_writeSz(RgssaKey* key, const char* value, FILE* fp) {
    for(int i = 0; value[i]; i++) {
        char c = ((value[i] == '/') ? '\\' : value[i]) ^ key->c[0];
        if(fwrite(&c, 1, 1, fp) != 1) {
            return true;
        }

        key->i *= 7;
        key->i += 3;
    }
    return false;
}

bool rgssa1_writeFile(const FileList* fileList, FILE* out) {
    RgssaKey key;
    key.i = RGSSA1_KEY;
    for(int i = 0; i < fileList->nFiles; i++) {
        //Write the filename
        int sizeName = strlen(fileList->files[i].name);
        if(rgssa1_writeInt(&key, sizeName, out)) {
            return true;
        }
        if(rgssa1_writeSz(&key, fileList->files[i].name, out)) {
            return true;
        }
        
        //Write the size
        if(rgssa1_writeInt(&key, fileList->files[i].size, out)) {
            return true;
        }
        //And embed the file
        if(rgssa_embed(key, fileList->files[i].name, fileList->files[i].size, out)) {
            return true;
        }
    }
    return false;
}
