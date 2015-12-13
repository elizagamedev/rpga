/* This file contains some source code adapted from DxLib.
 * DX Library Copyright (C) 2001-2008 Takumi Yamada.
 */

#include "wolf.h"

#include <sys/types.h>
#include <utime.h>

#include <iconv.h>

#define ATTRIBUTE_DIRECTORY 0x10
#define ATTRIBUTE_FILE      0x20

#define WOLF_PATH_MAX       1024

//DXLIB
#define MIN_COMPRESS        4
#define MAX_SEARCHLISTNUM   64
#define MAX_SUBLISTNUM      65536
#define MAX_COPYSIZE        (0x1fff + MIN_COMPRESS)
#define MAX_ADDRESSLISTNUM  (1024 * 1024 * 1)
#define MAX_POSITION        (1 << 24)

typedef struct PACKED wolf_Filename {
    uint16_t sizeDivBy4;
    uint16_t checksum;
} wolf_Filename;

typedef struct PACKED wolf_File {
    uint32_t offName;
    uint32_t attributes;
    uint64_t timeCreated;
    uint64_t timeAccessed;
    uint64_t timeModified;
    uint32_t offData;
    uint32_t size;
    uint32_t sizePress;
} wolf_File;

typedef struct PACKED wolf_Directory {
    uint32_t offFile;
    uint32_t offParentDir;
    uint32_t nChildren;
    uint32_t offFirstChild;
} wolf_Directory;

typedef struct FileEntry {
    char name[WOLF_PATH_MAX];   //base filename
    size_t nameSize;            //size of base filename
    int indexList;              //file list index
    int indexFullName;          //index of base filename in full name
} FileEntry;

const unsigned char WOLF_KEY[WOLF_KEY_SIZE] = {
    0xb3, 0x9d, 0xa0, 0x84, 0xd7, 0x37,
    0x53, 0x1f, 0xf1, 0x08, 0x18, 0x80,
};

const unsigned char WOLF_HEADER[WOLF_HEADER_SIZE] = {
    'D', 'X', 3, 0
};

#if 0
int wolf_compress(uint8_t *dst, uint8_t *src, size_t srcSize) {
    int dstsize ;
    int    bonus,    conbo,    conbosize,    address,    addresssize ;
    int maxbonus, maxconbo, maxconbosize, maxaddress, maxaddresssize ;
    BYTE keycode, *srcp, *destp, *dp, *sp, *sp2, *sp1 ;
    DWORD srcaddress, nextprintaddress, code ;
    int j ;
    DWORD i, m ;
    DWORD maxlistnum, maxlistnummask, listaddp ;
    DWORD sublistnum, sublistmaxnum ;
    LZ_LIST *listbuf, *listtemp, *list, *newlist ;
    BYTE *listfirsttable, *usesublistflagtable, *sublistbuf ;

    //Determine size of sublist
    {
        if(srcSize < 100 * 1024) {
            sublistmaxnum = 1;
        } else if( SrcSize < 3 * 1024 * 1024 ) {
            sublistmaxnum = MAX_SUBLISTNUM / 3 ;
        } else {
            sublistmaxnum = MAX_SUBLISTNUM ;
        }
    }

    //Determine size of list
    {
        maxlistnum = MAX_ADDRESSLISTNUM ;
        if( maxlistnum > SrcSize ) {
            while( ( maxlistnum >> 1 ) > 0x100 && ( maxlistnum >> 1 ) > SrcSize ) {
                maxlistnum >>= 1 ;
            }
        }
        maxlistnummask = maxlistnum - 1 ;
    }

    //Allocate memory
    usesublistflagtable   = (BYTE *)DXALLOC(
                            sizeof( DWORD_PTR ) * 65536 +
                            sizeof( LZ_LIST   ) * maxlistnum +
                            sizeof( BYTE      ) * 65536 +
                            sizeof( DWORD_PTR ) * 256 * sublistmaxnum );

    //Set addresses
    listfirsttable =     usesublistflagtable + sizeof( BYTE      ) * 65536 ;
    sublistbuf     =          listfirsttable + sizeof( DWORD_PTR ) * 65536 ;
    listbuf        = (LZ_LIST *)( sublistbuf + sizeof( DWORD_PTR ) * 256 * sublistmaxnum ) ;

    //Initialize memory
    _MEMSET( usesublistflagtable, 0, sizeof( BYTE      ) * 65536               ) ;
    _MEMSET(          sublistbuf, 0, sizeof( DWORD_PTR ) * 256 * sublistmaxnum ) ;
    _MEMSET(      listfirsttable, 0, sizeof( DWORD_PTR ) * 65536               ) ;
    list = listbuf ;
    for( i = maxlistnum / 8 ; i ; i --, list += 8 ) {
        list[0].address =
            list[1].address =
            list[2].address =
                list[3].address =
                    list[4].address =
                        list[5].address =
                            list[6].address =
                                list[7].address = 0xffffffff ;
    }

    srcp  = (BYTE *)Src ;
    destp = (BYTE *)Dest ;

    //Search for most infrequent byte?
    {
        DWORD qnum, table[256], mincode ;

        for( i = 0 ; i < 256 ; i ++ ) {
            table[i] = 0 ;
        }

        sp   = srcp ;
        qnum = SrcSize / 8 ;
        i    = qnum * 8 ;
        for( ; qnum ; qnum --, sp += 8 ) {
            table[sp[0]] ++ ;
            table[sp[1]] ++ ;
            table[sp[2]] ++ ;
            table[sp[3]] ++ ;
            table[sp[4]] ++ ;
            table[sp[5]] ++ ;
            table[sp[6]] ++ ;
            table[sp[7]] ++ ;
        }
        for( ; i < SrcSize ; i ++, sp ++ ) {
            table[*sp] ++ ;
        }

        keycode = 0 ;
        mincode = table[0] ;
        for( i = 1 ; i < 256 ; i ++ ) {
            if( mincode < table[i] ) {
                continue ;
            }
            mincode = table[i] ;
            keycode = (BYTE)i ;
        }
    }

    //Write some values to the header
    ((DWORD *)destp)[0] = SrcSize;
    destp[8] = keycode ;

    //Begin compression
    dp               = destp + 9 ;
    sp               = srcp ;
    srcaddress       = 0 ;
    dstsize          = 0 ;
    listaddp         = 0 ;
    sublistnum       = 0 ;
    nextprintaddress = 1024 * 100 ;
    while( srcaddress < SrcSize ) {
        // 残りサイズが最低圧縮サイズ以下の場合は圧縮処理をしない
        if( srcaddress + MIN_COMPRESS >= SrcSize ) {
            goto NOENCODE ;
        }

        // リストを取得
        code = *((WORD *)sp) ;
        list = (LZ_LIST *)( listfirsttable + code * sizeof( DWORD_PTR ) ) ;
        if( usesublistflagtable[code] == 1 ) {
            list = (LZ_LIST *)( (DWORD_PTR *)list->next + sp[2] ) ;
        } else {
            if( sublistnum < sublistmaxnum ) {
                list->next = (LZ_LIST *)( sublistbuf + sizeof( DWORD_PTR ) * 256 * sublistnum ) ;
                list       = (LZ_LIST *)( (DWORD_PTR *)list->next + sp[2] ) ;

                usesublistflagtable[code] = 1 ;
                sublistnum ++ ;
            }
        }

        // 一番一致長の長いコードを探す
        maxconbo   = -1 ;
        maxaddress = -1 ;
        maxbonus   = -1 ;
        for( m = 0, listtemp = list->next ; /*m < MAX_SEARCHLISTNUM &&*/ listtemp != NULL ; listtemp = listtemp->next, m ++ ) {
            address = srcaddress - listtemp->address ;
            if( address >= MAX_POSITION ) {
                if( listtemp->prev ) {
                    listtemp->prev->next = listtemp->next ;
                }
                if( listtemp->next ) {
                    listtemp->next->prev = listtemp->prev ;
                }
                listtemp->address = 0xffffffff ;
                continue ;
            }

            sp2 = &sp[-address] ;
            sp1 = sp ;
            if( srcaddress + MAX_COPYSIZE < SrcSize ) {
                conbo = MAX_COPYSIZE / 4 ;
                while( conbo && *((DWORD *)sp2) == *((DWORD *)sp1) ) {
                    sp2 += 4 ;
                    sp1 += 4 ;
                    conbo -- ;
                }
                conbo = MAX_COPYSIZE - ( MAX_COPYSIZE / 4 - conbo ) * 4 ;

                while( conbo && *sp2 == *sp1 ) {
                    sp2 ++ ;
                    sp1 ++ ;
                    conbo -- ;
                }
                conbo = MAX_COPYSIZE - conbo ;
            } else {
                for( conbo = 0 ;
                    conbo < MAX_COPYSIZE &&
                    conbo + srcaddress < SrcSize &&
                    sp[conbo - address] == sp[conbo] ;
                    conbo ++ ) {}
            }

            if( conbo >= 4 ) {
                conbosize   = ( conbo - MIN_COMPRESS ) < 0x20 ? 0 : 1 ;
                addresssize = address < 0x100 ? 0 : ( address < 0x10000 ? 1 : 2 ) ;
                bonus       = conbo - ( 3 + conbosize + addresssize ) ;

                if( bonus > maxbonus ) {
                    maxconbo       = conbo ;
                    maxaddress     = address ;
                    maxaddresssize = addresssize ;
                    maxconbosize   = conbosize ;
                    maxbonus       = bonus ;
                }
            }
        }

        // リストに登録
        newlist = &listbuf[listaddp] ;
        if( newlist->address != 0xffffffff ) {
            if( newlist->prev ) {
                newlist->prev->next = newlist->next ;
            }
            if( newlist->next ) {
                newlist->next->prev = newlist->prev ;
            }
            newlist->address = 0xffffffff ;
        }
        newlist->address = srcaddress ;
        newlist->prev    = list ;
        newlist->next    = list->next ;
        if( list->next != NULL ) {
            list->next->prev = newlist ;
        }
        list->next       = newlist ;
        listaddp         = ( listaddp + 1 ) & maxlistnummask ;

        // 一致コードが見つからなかったら非圧縮コードとして出力
        if( maxconbo == -1 ) {
    NOENCODE:
            // キーコードだった場合は２回連続で出力する
            if( *sp == keycode ) {
                if( destp != NULL ) {
                    dp[0]  =
                        dp[1]  = keycode ;
                    dp += 2 ;
                }
                dstsize += 2 ;
            } else {
                if( destp != NULL ) {
                    *dp = *sp ;
                    dp ++ ;
                }
                dstsize ++ ;
            }
            sp ++ ;
            srcaddress ++ ;
        } else {
            // 見つかった場合は見つけた位置と長さを出力する

            // キーコードと見つけた位置と長さを出力
            if( destp != NULL ) {
                // キーコードの出力
                *dp++ = keycode ;

                // 出力する連続長は最低 MIN_COMPRESS あることが前提なので - MIN_COMPRESS したものを出力する
                maxconbo -= MIN_COMPRESS ;

                // 連続長０〜４ビットと連続長、相対アドレスのビット長を出力
                *dp = (BYTE)( ( ( maxconbo & 0x1f ) << 3 ) | ( maxconbosize << 2 ) | maxaddresssize ) ;

                // キーコードの連続はキーコードと値の等しい非圧縮コードと
                // 判断するため、キーコードの値以上の場合は値を＋１する
                if( *dp >= keycode ) {
                    dp[0] += 1 ;
                }
                dp ++ ;

                // 連続長５〜１２ビットを出力
                if( maxconbosize == 1 ) {
                    *dp++ = (BYTE)( ( maxconbo >> 5 ) & 0xff ) ;
                }

                // maxconbo はまだ使うため - MIN_COMPRESS した分を戻す
                maxconbo += MIN_COMPRESS ;

                // 出力する相対アドレスは０が( 現在のアドレス−１ )を挿すので、−１したものを出力する
                maxaddress -- ;

                // 相対アドレスを出力
                *dp++ = (BYTE)( maxaddress ) ;
                if( maxaddresssize > 0 ) {
                    *dp++ = (BYTE)( maxaddress >> 8 ) ;
                    if( maxaddresssize == 2 ) {
                        *dp++ = (BYTE)( maxaddress >> 16 ) ;
                    }
                }
            }

            // 出力サイズを加算
            dstsize += 3 + maxaddresssize + maxconbosize ;

            // リストに情報を追加
            if( srcaddress + maxconbo < SrcSize ) {
                sp2 = &sp[1] ;
                for( j = 1 ; j < maxconbo && (DWORD_PTR)&sp2[2] - (DWORD_PTR)srcp < SrcSize ; j ++, sp2 ++ ) {
                    code = *((WORD *)sp2) ;
                    list = (LZ_LIST *)( listfirsttable + code * sizeof( DWORD_PTR ) ) ;
                    if( usesublistflagtable[code] == 1 ) {
                        list = (LZ_LIST *)( (DWORD_PTR *)list->next + sp2[2] ) ;
                    } else {
                        if( sublistnum < sublistmaxnum ) {
                            list->next = (LZ_LIST *)( sublistbuf + sizeof( DWORD_PTR ) * 256 * sublistnum ) ;
                            list       = (LZ_LIST *)( (DWORD_PTR *)list->next + sp2[2] ) ;

                            usesublistflagtable[code] = 1 ;
                            sublistnum ++ ;
                        }
                    }

                    newlist = &listbuf[listaddp] ;
                    if( newlist->address != 0xffffffff ) {
                        if( newlist->prev ) {
                            newlist->prev->next = newlist->next ;
                        }
                        if( newlist->next ) {
                            newlist->next->prev = newlist->prev ;
                        }
                        newlist->address = 0xffffffff ;
                    }
                    newlist->address = srcaddress + j ;
                    newlist->prev = list ;
                    newlist->next = list->next ;
                    if( list->next != NULL ) {
                        list->next->prev = newlist ;
                    }
                    list->next = newlist ;
                    listaddp = ( listaddp + 1 ) & maxlistnummask ;
                }
            }

            sp         += maxconbo ;
            srcaddress += maxconbo ;
        }
    }

    // 圧縮後のデータサイズを保存する
    *((DWORD *)&destp[4]) = dstsize + 9 ;

    // 確保したメモリの解放
    DXFREE( usesublistflagtable ) ;

    // データのサイズを返す
    return dstsize + 9 ;
}
#endif

size_t wolf_decompress(uint8_t *dst, const uint8_t *src) {
    uint8_t *dstOrig = dst;

    //Get destination and source buffer sizes
    size_t dstSize = *((uint32_t *)&src[0]);
    size_t srcSize = *((uint32_t *)&src[4]) - 9;

    //Control keycode
    int keycode = src[8];

    //Begin decompression
    src += 9;
    while (srcSize) {
        if (src[0] != keycode) {
            *dst = *src;
            dst++;
            src++;
            srcSize--;
            continue;
        }

        if (src[1] == keycode) {
            *dst = (uint8_t)keycode;
            dst++;
            src += 2;
            srcSize -= 2;
            continue;
        }

        int code = src[1];
        if (code > keycode)
            code--;

        src += 2;
        srcSize -= 2;

        int combo = code >> 3;
        if (code & (1<<2)) {
            combo |= *src << 5;
            src++;
            srcSize--;
        }
        combo += MIN_COMPRESS;

        int index;
        switch (code & 0x3) {
        case 0:
            index = *src;
            src++;
            srcSize--;
            break;
        case 1:
            index = *((uint16_t *)src);
            src += 2;
            srcSize -= 2;
            break;
        case 2:
            index = *((uint16_t *)src) | (src[2] << 16);
            src += 3;
            srcSize -= 3;
            break;
        default:
            return 0;
        }
        index++;

        //Decompress
        if (index < combo) {
            int num = index;
            while (combo > num) {
                memcpy(dst, dst - num, num);
                dst += num;
                combo -= num;
                num += num;
            }
            if (combo != 0) {
                memcpy(dst, dst - num, combo);
                dst += combo;
            }
        } else {
            memcpy(dst, dst - index, combo);
            dst += combo;
        }
    }

    return dstSize;
}

bool wolf_read(const unsigned char* key, void* value, size_t size, FILE* fp) {
    int offset = ftell(fp) % WOLF_KEY_SIZE;
    if(fread(value, size, 1, fp) != 1) {
        return true;
    }
    for(unsigned int i = 0; i < size; i++) {
        ((unsigned char*)value)[i] ^= key[offset];
        offset = (offset + 1) % WOLF_KEY_SIZE;
    }
    return false;
}

inline static bool wolf_readInt32(const unsigned char* key, uint32_t* value, FILE* fp) {
    return wolf_read(key, value, 4, fp);
}

size_t wolf_getFilename(char* name, unsigned char* filenames, unsigned int offset) {
    size_t size = ((wolf_Filename*)(filenames + offset))->sizeDivBy4 * 4;
    char* szWolfName = (char*)(filenames + offset + 4 + size);

    for(size = 0; szWolfName[size]; size++) {
        name[size] = szWolfName[size];
    }
    name[size] = 0;
    return size;
}

bool wolf_extract(const unsigned char* key, const char* name, const wolf_File* file, FILE* in) {
    if(verbose) {
        printf("x %s\n", name);
    }
    mkdirp(name);
    FILE* out = fopen(name, "wb");
    if(!out) {
        return true;
    }

    //Write the file to the disk
    size_t offset = 0x18 + file->offData;
    size_t size = file->size;
    fseek(in, offset, SEEK_SET);

    if(file->sizePress == 0xffffffff) {
        for(unsigned int i = 0; i < size; i++) {
            fputc(fgetc(in) ^ key[(offset + i) % WOLF_KEY_SIZE], out);
        }
    } else {
        //Read and decompress the data
        uint8_t *dataSrc = (uint8_t *)malloc(file->sizePress);
        uint8_t *dataDst = (uint8_t *)malloc(size);
        wolf_read(key, dataSrc, file->sizePress, in);
        size = wolf_decompress(dataDst, dataSrc);

        //Write to the file
        fwrite(dataDst, size, 1, out);
        free(dataSrc);
        free(dataDst);
    }
    fclose(out);

    //Change the file timestamp
    struct utimbuf ut;
    ut.actime = w32toUnixTime(file->timeAccessed);
    ut.modtime = w32toUnixTime(file->timeModified);
    utime(name, &ut);
    return false;
}

bool wolf_readFile(FILE* fp, size_t sizeFile) {
    unsigned char key[WOLF_KEY_SIZE];

    //The first 12 bytes will help us decrypt the file
    if(fread(key, 12, 1, fp) != 1) {
        return true;
    }

    //Let's try decrypting this thing
    //xor this with the header to get the first 4 bytes
    for(int i = 0; i < 4; i++) {
        key[i] ^= WOLF_HEADER[i];
    }
    //The last four bytes are 0x00000018
    key[8] ^= 0x18;

    //We can now decrypt the filenames offset...
    uint32_t offFilenames;
    if(wolf_readInt32(key, &offFilenames, fp)) {
        return true;
    }

    //The size of the file minus the size of the filenames offset
    //is the same as the size of the file info.
    uint32_t sizeFileInfo = sizeFile - offFilenames;
    *(uint32_t*)(key + 4) ^= sizeFileInfo;

    //And we're done! Read the remainder of the header.
    uint32_t offFiles, offDirectories;
    if(wolf_readInt32(key, &offFiles, fp) ||
       wolf_readInt32(key, &offDirectories, fp)) {
        return true;
    }

    //Open the iconv descriptor for converting shift_jis->utf-8
    iconv_t jis2utf8 = iconv_open("UTF-8", "SHIFT_JIS");

    //Read the file info into memory
    unsigned char* filenames = malloc(sizeFileInfo);
    wolf_File* files = (wolf_File*)(filenames + offFiles);
    wolf_Directory* directories = (wolf_Directory*)(filenames + offDirectories);
    fseek(fp, offFilenames, SEEK_SET);
    if(wolf_read(key, filenames, sizeFileInfo, fp)) {
        goto error;
    }

    //Calculate number of things in each block
    int nFiles = (offDirectories - offFiles) / sizeof(wolf_File);
    int nDirectories = (sizeFileInfo - offDirectories) / sizeof(wolf_Directory);

    //Let's traverse the files
    for(int i = 0; i < nFiles; i++) {
        //Skip directories
        if(files[i].attributes == ATTRIBUTE_DIRECTORY) {
            continue;
        }

        //Begin to construct full file path
        char name[WOLF_PATH_MAX+1];
        char path[WOLF_PATH_MAX+1];
        path[WOLF_PATH_MAX] = 0;
        int iPath = WOLF_PATH_MAX;

        //Get this file's name and put it at the end
        size_t sizeName = wolf_getFilename(name, filenames, files[i].offName);
        iPath -= sizeName;
        memcpy(path + iPath, name, sizeName);

        //Find our parent directory
        int j = 0;
        for(; j < nDirectories; j++) {
            int iFirstChild = directories[j].offFirstChild / sizeof(wolf_File);
            if(iFirstChild <= i && iFirstChild + directories[j].nChildren > i) {
                //This is our parent. Traverse the tree backwards till we hit root.
                while(directories[j].offParentDir != -1) {
                    path[--iPath] = '/';
                    sizeName = wolf_getFilename(name, filenames,
                               files[directories[j].offFile / sizeof(wolf_File)].offName);
                    iPath -= sizeName;
                    memcpy(path + iPath, name, sizeName);
                    j = directories[j].offParentDir / sizeof(wolf_Directory);
                }
                break;
            }
        }
        if(j == nDirectories) {
            goto error;
        }

        //Convert from shift-jis to utf-8
        char* inname = path + iPath;
        size_t inbytes = WOLF_PATH_MAX - iPath;
        char* outname = name;
        size_t outbytes = WOLF_PATH_MAX;
        iconv(jis2utf8, &inname, &inbytes, &outname, &outbytes);
        name[WOLF_PATH_MAX - outbytes] = 0;

        //Extract the file
        if(wolf_extract(key, name, files + i, fp)) {
            goto error;
        }
    }

end:
    iconv_close(jis2utf8);
    free(filenames);
    return false;

error:
    iconv_close(jis2utf8);
    free(filenames);
    return true;
}

/*
 * WRITE
 */

bool wolf_write(const void* value, size_t size, FILE* out) {
    int offset = ftell(out) % WOLF_KEY_SIZE;
    for(unsigned int i = 0; i < size; i++) {
        if(fputc(((unsigned char*)value)[i] ^ WOLF_KEY[offset], out) == EOF) {
            return true;
        }
        offset = (offset + 1) % WOLF_KEY_SIZE;
    }
    return false;
}

bool wolf_writeFile(FileList* fileList, FILE* out) {
    //Let's determine the size of everything.

    //Write the header
    if(wolf_write(WOLF_HEADER, WOLF_HEADER_SIZE, out)) {
        return true;
    }
}
