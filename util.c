#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ftw.h>

bool verbose = false;

void mkdirp(const char* path) {
    char* pathNew = (char*)malloc(strlen(path) + 1);
    for(int i = 0; path[i]; i++) {
        if(path[i] == '/') {
            pathNew[i] = 0;
            mkdir(pathNew, 0755);
            pathNew[i] = '/';
        } else {
            pathNew[i] = path[i];
        }
    }
    free(pathNew);
}

bool isDirectory(const char* path) {
    struct stat st;
    lstat(path, &st);
    return S_ISDIR(st.st_mode);
}

size_t getFileSize(const char* filename) {
    struct stat st;
    stat(filename, &st);
    return st.st_size;
}

void updateFileSizeAndTime(File* file) {
    struct stat st;
    stat(file->name, &st);
    file->size = st.st_size;
//#ifdef st.st_birthtime
//  file->timeCreated = st.st_birthtime;
//#else
    file->timeCreated = st.st_ctime;
//#endif
    file->timeModified = st.st_ctime;
    file->timeAccessed = st.st_atime;
}

uint32_t rand_key() {
    return ((rand() & 0xFF) << 24) |
           ((rand() & 0xFF) << 16) |
           ((rand() & 0xFF) << 8) |
           (rand() & 0xFF);
}

time_t w32toUnixTime(uint64_t timeW32) {
    return (time_t)((timeW32 / 10000000) - 11644473600LL);
}

uint64_t unixToW32Time(time_t timeUnix) {
    return (uint64_t)((timeUnix * 10000000) + 11644473600LL);
}

//FILE LIST

void FileList_free(FileList* list) {
    for(int i = 0; i < list->nFiles; i++) {
        free(list->files[i].name);
    }
    free(list->files);
}

bool FileList_add(FileList* list, char* path) {
    if(isDirectory(path)) {
        struct dirent* file;
        DIR* dir = opendir(path);

        if(!dir) {
            fprintf(stderr, "error: could not open directory \"%s\"\n", path);
            return true;
        }

        while((file = readdir(dir))) {
            if(!strcmp(file->d_name, ".") || !strcmp(file->d_name, "..")) {
                continue;
            }

            char* fullpath = (char*)malloc(strlen(path) + strlen(file->d_name) + 2);
            strcpy(fullpath, path);
            strcat(fullpath, "/");
            strcat(fullpath, file->d_name);
        
            if(FileList_add(list, fullpath)) {
                free(fullpath);
            }
        }

        //Free and return
        closedir(dir);
        
        return true;
    }
    
    //Add single file
    if(list->files) {
        list->files = (File*)realloc(list->files, sizeof(File) * (list->nFiles + 1));
        list->files[list->nFiles].name = path;
        updateFileSizeAndTime(&list->files[list->nFiles]);
        list->nFiles++;
    } else {
        list->files = (File*)malloc(sizeof(File));
        list->files[0].name = path;
        updateFileSizeAndTime(&list->files[list->nFiles]);
        list->nFiles = 1;
    }
    return false;
}

int FileList_sort_func(const void* a, const void* b) {
    return strcmp(((const File*)a)->name, ((const File*)b)->name);
}

void FileList_sort(FileList* list) {
    qsort(list->files, list->nFiles, sizeof(File), FileList_sort_func);
}

/*
 * RGSSA
 */
bool rgssa_extract(RgssaKey key, const char* szFile, size_t size, FILE* in) {
    if(verbose) {
        printf("x %s\n", szFile);
    }
    mkdirp(szFile);
    FILE* out = fopen(szFile, "wb");
    if(!out) {
        return true;
    }
    for(unsigned int i = 0; i < size; i++) {
        char c;
        if(fread(&c, 1, 1, in) != 1) {
            fclose(out);
            return true;
        }
        c ^= key.c[i % 4];
        if(fwrite(&c, 1, 1, out) != 1) {
            fclose(out);
            return true;
        }

        //Update key every 4 bytes
        if(i % 4 == 3) {
            key.i *= 7;
            key.i += 3;
        }
    }
    fclose(out);
    return false;
}

bool rgssa_embed(RgssaKey key, const char* szFile, size_t size, FILE* out) {
    if(verbose) {
        printf("a %s\n", szFile);
    }
    FILE* in = fopen(szFile, "rb");
    if(!in) {
        return true;
    }
    for(unsigned int i = 0; i < size; i++) {
        unsigned char c;
        if(fread(&c, 1, 1, in) != 1) {
            return true;
        }
        c ^= key.c[i % 4];
        if(fwrite(&c, 1, 1, out) != 1) {
            return true;
        }

        //Update key every 4 bytes
        if(i % 4 == 3) {
            key.i *= 7;
            key.i += 3;
        }
    }
    fclose(in);
    return false;
}
