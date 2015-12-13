#include <time.h>
#include <errno.h>

#include "rgssa1.h"
#include "rgssa3.h"
#include "wolf.h"

enum {
    MODE_UNDEFINED,
    MODE_UNPACK,
    MODE_PACK_V1,
    MODE_PACK_V3,
    MODE_PACK_WOLF,
};

static inline void usage() {
    fprintf(stderr, "usage: rpga [v] [x ARCHIVE] [13w ARCHIVE FILES...]\n");
}

int main(int argc, char** argv) {
    if(argc < 3) {
        usage();
        return 1;
    }

    int mode = MODE_UNDEFINED;

    //Check first argument for options: only check args from this argument
    for(int i = 0; argv[1][i]; i++) {
        switch(argv[1][i]) {
        case 'v':
            verbose = true;
            break;
        case 'x':
            mode = MODE_UNPACK;
            break;
        case '1':
            mode = MODE_PACK_V1;
            break;
        case '3':
            mode = MODE_PACK_V3;
            break;
        case 'w':
            mode = MODE_PACK_WOLF;
            break;
        default:
            usage();
            return 1;
        }
    }
    
    int code = 1;
    FILE* fp = NULL;
    FileList fileList = {0};

    //Begin reading file
    switch(mode)
    {
    case MODE_UNPACK:
    {
        if(!(fp = fopen(argv[2], "rb"))) {
            fprintf(stderr, "error: cannot read file \"%s\"\n", argv[2]);
            return 1;
        }

        //Variables
        unsigned char sig[8];

        //BEGIN READ
        //Read the first eight characters. If they're RGSSAD\0, it's an rgssa
        //archive. Otherwise, attempt to decrypt/read a wolf file.
        if(fread(sig, sizeof(sig), 1, fp) != 1) {
            goto error_read;
        }
        if(memcmp(sig, "RGSSAD", 7)) {
            fseek(fp, 0, SEEK_SET);
            if(wolf_readFile(fp, getFileSize(argv[2]))) {
                goto error_read;
            }
        } else {
            switch(sig[7]) {
            case 1:
                if(rgssa1_readFile(fp)) {
                    goto error_read;
                }
                break;
            case 3:
                if(rgssa3_readFile(fp)) {
                    goto error_read;
                }
                break;
            default:
                fprintf(stderr, "error: unknown rgssa archive version: %d\n", sig[7]);
                goto end;
            }
        }
    }
    break;

    case MODE_PACK_V1:
    case MODE_PACK_V3:
    {
        if(argc < 4) {
            usage();
            return 1;
        }
        //Generate a list of files
        for(int i = 3; i < argc; i++) {
            FileList_add(&fileList, argv[i]);
        }
        FileList_sort(&fileList);
        
        //File-relevant variables
        unsigned char version = (mode == MODE_PACK_V1) ? 1 : 3;
        
        if(!(fp = fopen(argv[2], "wb"))) {
            goto error_write;
        }
        
        //Write header + version
        if(fwrite("RGSSAD", 7, 1, fp) != 1) {
            goto error_write;
        }
        
        if(fwrite(&version, 1, 1, fp) != 1) {
            goto error_write;
        }
        
        //Write the files
        if(mode == MODE_PACK_V1) {
            if(rgssa1_writeFile(&fileList, fp)) {
                goto error_write;
            }
        } else {
            srand(time(NULL));
            if(rgssa3_writeFile(&fileList, fp)) {
                goto error_write;
            }
        }
    }
    break;
    
    case MODE_PACK_WOLF:
    {
        if(argc < 4) {
            usage();
            return 1;
        }
        //Generate a list of files
        for(int i = 3; i < argc; i++) {
            FileList_add(&fileList, argv[i]);
        }
        FileList_sort(&fileList);
        
        if(!(fp = fopen(argv[2], "wb"))) {
            goto error_write;
        }
        
        //Write the file
        if(wolf_writeFile(&fileList, fp)) {
            goto error_write;
        }
    }
    break;

    default:
        usage();
        return 1;
    }
    
    //Cleanup
    code = 0;
    goto end;
error_read:
    fprintf(stderr, "error: invalid archive: \"%s\"\n", argv[2]);
    goto end;
error_write:
    fprintf(stderr, "error: cannot write to file \"%s\"\n", argv[2]);
    goto end;

end:
    if(fp) {
        fclose(fp);
    }
    FileList_free(&fileList);
    return code;
}
