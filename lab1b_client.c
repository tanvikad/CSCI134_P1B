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
#include <sys/wait.h>
#include <sys/types.h> 
#include <netinet/in.h>
#include <netdb.h> 

#include "test.c"


void set_current_terminal(struct termios * current_terminal) {
    int return_tcsetattr =  tcsetattr(0, TCSANOW, current_terminal);
    if(return_tcsetattr != 0) {
        fprintf(stderr, "Unable to set terminal attributes because of error: %s\n", strerror(errno));
        exit(1);    
    }
    exit(0);
}


//if it returns -1, the child should not be written to
int input_read(int fd,  int socket_fd) {
    // int size_to_read = 1000000;
    char buffer[BUFFER_SIZE]; 
    char compressed_buffer[BUFFER_SIZE];
    int original_size; 
    // int how_much_read = read(fd, buffer, size_to_read);
    int how_much_read;
    if (fd != 0) {
        //we are reading from the socket
        how_much_read = decompress_buffer(fd, buffer);

    } else {
        // int ret = write(socket_fd, buffer, how_much_read);
        // if(ret == -1) {
        //     fprintf(stderr, "Writing to socket failed due to %s\n", strerror(errno));
        //     return 2;
        // }

        //  we are going to write to the socket
        int compressed_size = compress_buffer(fd, compressed_buffer, buffer, &original_size);
        how_much_read = original_size;
        // printf("How much read is %d \n\r", how_much_read);
        // printf("The compressed version is %d \n\r", compressed_size);
        // write(1, buffer, how_much_read);

        int ret = write(socket_fd, compressed_buffer, compressed_size);
        // printf("\r\n");
        // write(1, compressed_buffer, compressed_size);
        // printf("\r\n");
        if(ret == -1) {
            fprintf(stderr, "Writing to socket failed due to %s\n", strerror(errno));
            return 2;
        }
    }
    if (how_much_read == -1) {
        fprintf(stderr, "Reading failed due to error from %s\n\r", strerror(errno));
        exit(1);
    } else if (how_much_read == 0 && fd == socket_fd){
        return 1; 
    }
    
    for (int i = 0; i < how_much_read; i++) {
        if(buffer[i] == 4 && fd == socket_fd) {
            return 1;
        }
        if(buffer[i] == '\r' || buffer[i] == '\n') {
            int ret = write(1, "\r\n", 2);
            if(ret == -1) {
                fprintf(stderr, "Writing to stdout failed due to %s\n", strerror(errno));
                return 2;
            }
            
        } else {
            int ret = write (1, buffer + i, 1);
            if(ret == -1) {
                fprintf(stderr, "Writing to stdout failed due to %s\n", strerror(errno));
                return 2;
            }
        }

    }
    
    return 0;
}


int main(int argc, char *argv[]){
    signal(SIGPIPE, SIG_IGN);
    int curr_option;
    const struct option options[] = {
        { "port",  required_argument, NULL,  'p' },
        { 0, 0, 0, 0}
    };

    int port = -1;
    while((curr_option = getopt_long(argc, argv, "p", options, NULL)) != -1)  {
        switch(curr_option) {
            case 'p':
                // port = optarg;
                sscanf(optarg, "%d", &port);
                break;
            default:
                fprintf(stderr, "Use the options --port. Getting error %s \n", strerror(errno));
                exit(1);
                break;
        }
    }

    if (port == -1) {
        fprintf(stderr, "Port not specified \n");
        exit(1);
    }


    struct termios current_terminal;
    int return_tcgetattr = tcgetattr(0, &current_terminal);
    if(return_tcgetattr != 0) {
        // TODO: check if error checking later 
        fprintf(stderr, "Unable to get terminal attributes because of error: %s\n", strerror(errno));
        exit(1);
    }

    struct termios new_terminal = current_terminal; 

    new_terminal.c_iflag = ISTRIP;
    new_terminal.c_oflag = 0;
    new_terminal.c_lflag = 0;

    int return_tcsetattr =  tcsetattr(0, TCSANOW, &new_terminal);
    if(return_tcsetattr != 0) {
        fprintf(stderr, "LUnable to set terminal attributes because of error: %s\n", strerror(errno));
        set_current_terminal(&current_terminal);
        exit(1);    
    }

    // This code was taken from cs.rpi 
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd < 0) {
        fprintf(stderr, "ERROR opening socket due to erro %s \n", strerror(errno));
        set_current_terminal(&current_terminal);
        exit(1);
    }
    struct sockaddr_in serv_addr;
    // char buffer[256];
    struct hostent *server = gethostbyname("localhost");


    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(socketfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)  {
        set_current_terminal(&current_terminal);
        fprintf(stderr, "ERROR accepting socket due to error %s \r\n", strerror(errno));
        exit(1);
    }
    
    int nfds = 2;
    struct pollfd poll_fds[nfds];
    poll_fds[0].fd = socketfd;
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = 0;
    poll_fds[1].events = POLLIN; 


    int keyboard_alive = 1;
    int tcp_alive = 1; 
    while(keyboard_alive || tcp_alive) {
        int ret = poll(poll_fds, nfds, -1);
        if (ret <= 0) {
            printf("Polling failed\r\n");
            set_current_terminal(&current_terminal);
            exit(1); 
        }
        for (int input_fd = 0; input_fd < nfds; input_fd++) {
            if (poll_fds[input_fd].revents & POLLIN) {
                int read_return = input_read(poll_fds[input_fd].fd,  socketfd);
                if(read_return == 1) {
                    set_current_terminal(&current_terminal);
                    exit(0);
                }else if (read_return == 2) {
                    //something failed;
                    set_current_terminal(&current_terminal);
                    exit(0);
                }

            }
            if (poll_fds[input_fd].revents & POLLERR || poll_fds[input_fd].revents & POLLHUP) {
                // set_current_terminal(&current_terminal);
                // exit(1);
                poll_fds[input_fd].fd = -1; 
            }
        }
        
    }
    


    // fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d \r\n", (status&0x7F), (status&0xff00)>>8);
    set_current_terminal(&current_terminal);

    
}