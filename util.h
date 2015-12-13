#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern bool verbose;

#define PACKED __attribute__((__packed__))

/*
 * DATA STRUCTURES
 */
//Describes a "key" for rgssa encryption/decryption
typedef union RgssaKey {
    uint32_t i;
    char c[4];
} RgssaKey;

//Describes a file
typedef struct File {
    char* name;
    size_t size;
    time_t timeCreated;
    time_t timeModified;
    time_t timeAccessed;
    RgssaKey key;
} File;

//FILE LIST
typedef struct FileList {
    int nFiles;
    File* files;
    int nDirectories;
    char** directories;
} FileList;

void FileList_free(FileList* list);
bool FileList_add(FileList* list, char* path);
int FileList_sort_func(const void* a, const void* b);
void FileList_sort(FileList* list);

//General functions
void mkdirp(const char* path);
bool isDirectory(const char* path);
size_t getFileSize(const char* filename);
void updateFileSizeAndTime(File* file);
uint32_t rand_key();
time_t w32toUnixTime(uint64_t timeW32);
uint64_t unixToW32Time(time_t timeUnix);

//RGSSA
//Extract an rgssa file
bool rgssa_extract(RgssaKey key, const char* szFile, size_t size, FILE* in);
//Embed an rgssa file
bool rgssa_embed(RgssaKey key, const char* szFile, size_t size, FILE* out);

#endif

