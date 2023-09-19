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

#define BUFFER_SIZE 1000
#define ACTUAL_SIZE 500

//we want to read
int decompress_buffer(int fd, char* buffer, int log_fd) {
    // int CHUNK = 400;
    // printf("entered decompress buffer 2 \r\n ");
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[BUFFER_SIZE];
    unsigned char out[BUFFER_SIZE];

    // printf("hi");
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

    // printf("the file desciptor is %d ", fd);
    strm.avail_in = read(fd, in, ACTUAL_SIZE);
    // printf("The avail in %d \r\n", strm.avail_in);
    strm.next_in = in;

    if(log_fd != -1) {
        char log_buf[100];
        snprintf(log_buf, 100, "SENT %d bytes:", strm.avail_in);
        write(log_fd, log_buf, strlen(log_buf));
        // printf("log files is %d \n", log_fd);
        write(log_fd, in, strm.avail_in);
        write(log_fd, "\n", 1);
    }

    // write(1, in, strm.avail_in);

    strm.avail_out = BUFFER_SIZE;
    strm.next_out = out;
    ret = inflate(&strm, Z_SYNC_FLUSH);
    have = BUFFER_SIZE - strm.avail_out;

    // write(1, out, have);
    memcpy(buffer, out, have);
    // buffer = out;

    inflateEnd(&strm);
    return have; 

}
int  compress_buffer(int read_fd, char* buffer, char* original_buffer, int* size_original, int log_fd) {
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char in[BUFFER_SIZE];
    unsigned char out[BUFFER_SIZE];

    flush = Z_SYNC_FLUSH;
    // printf("The chunk is %d \n", ACTUAL_SIZE);
    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);

    if(ret != Z_OK) {
        fprintf(stderr, "Error with z \n");
        exit(1);
    }


    strm.avail_in = read(read_fd, in, ACTUAL_SIZE);
    strncpy(original_buffer, (const char *)(in), strm.avail_in);
    *size_original = strm.avail_in; 
    strm.next_in = in; 

    strm.avail_out = BUFFER_SIZE;
    strm.next_out = out;
    ret = deflate(&strm, flush);
    have = BUFFER_SIZE - strm.avail_out;
    memcpy(buffer, out, have);

    if(log_fd != -1) {
        char log_buf[100];
        snprintf(log_buf, 100, "RECEIVED %d bytes:", have);
        write(log_fd, log_buf, strlen(log_buf));
        write(log_fd, out, have);
        write(log_fd, "\n", 1);
    }

    deflateEnd(&strm);


    // buffer = out;


    // // printf("\n-------------\n");
    // do {
    //     // printf("\n ------ \n ");
    //     strm.avail_out = BUFFER_SIZE;
    //     strm.next_out = out;
    //     ret = deflate(&strm, flush);    /* no bad return value */   
    //     have = BUFFER_SIZE - strm.avail_out;

    //     // printf("Have is %d \n", have);
    //     if (write(1, out, have) != have) {
    //         (void)deflateEnd(&strm);
    //         fprintf(stderr, "Error with stream %d\n", Z_ERRNO);
    //         exit(1);
    //     }   
    // } while (strm.avail_out == 0);

    // write(write_fd, out, have);

    return have; 
    // printf("%s\n", out);
    // decompress_buffer(fd);
}

