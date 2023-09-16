#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h> 
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <termios.h>
#include <sys/types.h> 
#include <netinet/in.h>
#include <sys/wait.h>
#include "zlib.h"


void decompress_buffer(int fd) {
    int CHUNK = 400;
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK) {
        fprintf(stderr, "Inflate failed \n");
        exit(1);
    }
    /* decompress until deflate stream ends or end of file */
     /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = read(fd, in, CHUNK);
        if (strm.avail_in == -1) {
            fprintf(stderr, "Failed on the read \n");
            exit(1);
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            // assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            if(ret == Z_STREAM_ERROR) {
                printf(stderr, "Inflating failed \n");
                exit(1);
            }
            switch (ret) {
                case Z_NEED_DICT:
                    ret = Z_DATA_ERROR;     /* and fall through */
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    (void)inflateEnd(&strm);
                    return ret;
            }
            have = CHUNK - strm.avail_out;
            if (write(1, out, have) != have) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

}
void compress_buffer( int fd) {
    int CHUNK = 400;
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    flush = Z_SYNC_FLUSH;
    printf("The chunk is %d \n", CHUNK);
    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);

    if(ret != Z_OK) {
        fprintf(stderr, "Error with z \n");
        exit(1);
    }


    strm.avail_in = read(fd, in, CHUNK);
    strm.next_in = in; 

    printf("\n-------------\n");
    do {
        printf("\n ------ \n ");
        strm.avail_out = CHUNK;
        strm.next_out = out;
        ret = deflate(&strm, flush);    /* no bad return value */   
        have = CHUNK - strm.avail_out;

        printf("Have is %d \n", have);
        if (write(1, out, have) != have) {
            (void)deflateEnd(&strm);
            fprintf(stderr, "Error with stream %d\n", Z_ERRNO);
            exit(1);
        }   
    } while (strm.avail_out == 0);

    write(fd, out, have);
    // printf("%s\n", out);
    decompress_buffer(fd);
}

