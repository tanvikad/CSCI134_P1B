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



void decompress_buffer(char* buffer, int size_of_buffer) {
    int CHUNK = size_of_buffer;
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
        fprinf("Inflate failed \n");
        exit(1);
    }
    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = CHUNK;
        strm.next_in = buffer;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                return ret;
            }
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

}
void compress_buffer(char* buffer, int size_of_buffer) {
    int CHUNK = size_of_buffer;
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char out[CHUNK];

    flush = Z_FINISH;
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
    strm.next_in = buffer;
    strm.avail_in = CHUNK; 
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

    // printf("%s\n", out);
    decompress_buffer(out, have);
    
}

//if it returns -1, the child should not be written to
int input_read(int fd,  int shell_fd, int newsockfd) {
    int size_to_read = 300;
    char buffer[size_to_read]; 
    int how_much_read = read(fd, buffer, size_to_read);
    if(how_much_read == -1) {
        fprintf(stderr, "Reading failed due to error from %s\n", strerror(errno));
        exit(1);
    }
    if (fd == newsockfd) {
        for (int i = 0; i < how_much_read; i++) {
            if(buffer[i] == 4) {
                return 1;
            } 
            int ret = write(shell_fd, buffer + i, 1);
            if(ret == -1) {
                fprintf(stderr, "Writing to shell failed due to %s\n", strerror(errno));
                exit(1);
            }
        }
        return 0;
    }

    int ret =  write(newsockfd, buffer, how_much_read);
    compress_buffer(buffer, how_much_read);
    if(ret == -1) {
        fprintf(stderr, "Writing to socket failed due to %s\n", strerror(errno));
        exit(1);
    }

    return 0;
}


int main(int argc, char *argv[]){
    signal(SIGPIPE, SIG_IGN);
    int curr_option;
    const struct option options[] = {
        { "shell",  required_argument, NULL,  's' },
        { "port",   required_argument, NULL,  'p'},
        { "compress",   no_argument, NULL,  'c'},
        { 0, 0, 0, 0}
    };

    char* name_of_program = NULL;
    int port = -1;
    int got_compress = 0;
    while((curr_option = getopt_long(argc, argv, "s", options, NULL)) != -1)  {
        switch(curr_option) {
            case 's':
                name_of_program = optarg;
                break;
            case 'p':
                sscanf(optarg, "%d", &port);
                break;
            case 'c':
                got_compress = 1;
                break;
            default:
                fprintf(stderr, "Use the options --shell, --port, --compress. Getting error %s \n", strerror(errno));
                exit(1);
                break;
        }
    }

    if (port == -1) {
        fprintf(stderr, "Valid port not specified \n");
        exit(1);
    } 

    if (name_of_program  != NULL) {
        printf("The name of the program is %s \n", name_of_program);
    }
    //read write 
    int tToS_fd[2];
    int sToT_fd[2];
    
    int return_from_pipe = pipe(tToS_fd);
    int return_from_pipe2 = pipe(sToT_fd);

    if(return_from_pipe == -1) {
        fprintf(stderr, " Pipe 1 failed due to error error %s \n", strerror(errno));
        exit(1);
    }

    if(return_from_pipe2 == -1) {
        fprintf(stderr, " Pipe 2 failed due to error error %s \n", strerror(errno));
        exit(1);
    }

    //waht should be the type of socket that I open
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd < 0) {
        fprintf(stderr, "ERROR opening socket due to erro %s \n", strerror(errno));
        exit(1);
    }
    printf("the socket fd is %d \r\n", socketfd);

    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr)); //make the server address all 0s
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port); //use htons to convert it into the network byte order
    serv_addr.sin_addr.s_addr = INADDR_ANY; //you use the IP address of the one one the computer
    
    printf("The socket address is %d \r\n",serv_addr.sin_addr.s_addr);
    if (bind(socketfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "ERROR binding to socket due to error %s \n", strerror(errno));
        exit(1);
    }

    listen(socketfd,5);
    struct sockaddr_in client_addr;
    socklen_t clilen = (socklen_t)(sizeof(client_addr));
    int newsockfd = accept(socketfd, (struct sockaddr *) &client_addr, &clilen);
    if (newsockfd < 0){
        fprintf(stderr, "ERROR on accept due to error:  %s \n", strerror(errno));
        exit(1);
    }

    printf("Got a connection \r\n");
    // close(newsockfd);
    
    int pid = fork();
    if (pid == -1) {
        fprintf(stderr, "Forking failed due to %s\n", strerror(errno));
        exit(1);
    }
    if(pid == 0) {

        close(0);
        close(1);
        close(2);
        dup(tToS_fd[0]);
        dup(sToT_fd[1]);
        dup(sToT_fd[1]);
        close(tToS_fd[0]);
        close(sToT_fd[1]);

        //we have to close the ones we don't need
        close(sToT_fd[0]); 
        close(tToS_fd[1]);

        if (name_of_program != NULL) {
            if(execl(name_of_program, name_of_program, NULL) == -1) {
                fprintf(stderr, "Exec failed due to %s\n", strerror(errno));
                exit(1);
            }
        }

    }else {

        close(tToS_fd[0]);
        close(sToT_fd[1]);

        int nfds = 2;
        struct pollfd poll_fds[nfds];
        if(name_of_program == NULL) {
            nfds = 1;
        }else {
            poll_fds[1].fd = sToT_fd[0];
            poll_fds[1].events = POLLIN;
        }
        poll_fds[0].fd = newsockfd;
        poll_fds[0].events = POLLIN;

        printf("fs are %d and %d \n\n", poll_fds[0].fd, poll_fds[1].fd);
        
        // int keyboard_alive = 1;
        int child_alive = 1; 
        int tcp_alive = 1; 
        if(nfds == 1) child_alive = 0;
        while(tcp_alive || child_alive) {
            
            int ret = poll(poll_fds, nfds, -1);
            if (ret <= 0) {
                printf("Polling failed\r\n");
                exit(1); 
            }
            for (int input_fd = 0; input_fd < nfds; input_fd++) {
                if(poll_fds[input_fd].fd == sToT_fd[0]) {
                    //the poll error happens on the child
                    if (!(poll_fds[input_fd].revents & POLLIN) && (poll_fds[input_fd].revents & POLLHUP)) {
                        tcp_alive = 0;
                        child_alive = 0;
                        continue;
                    }
                } 
                
                if (poll_fds[input_fd].revents & POLLIN) {
                    int read_return = input_read(poll_fds[input_fd].fd,  tToS_fd[1], newsockfd);
                    if(read_return == 1) {
                        close(tToS_fd[1]);
                        tcp_alive = 0;
                        continue;
                    }

                }


            }
            
        }

    }


    int status;
    waitpid(pid, &status, 0);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d \r\n", (status&0x7F), (status&0xff00)>>8);

    
}