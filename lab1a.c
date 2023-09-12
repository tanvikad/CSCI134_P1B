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



void set_current_terminal(struct termios * current_terminal) {
    int return_tcsetattr =  tcsetattr(0, TCSANOW, current_terminal);
    if(return_tcsetattr != 0) {
        fprintf(stderr, "Unable to set terminal attributes because of error: %s\n", strerror(errno));
        exit(1);    
    }
    exit(0);
}


//if it returns -1, the child should not be written to
int input_read(int fd,  int shell_fd, int pid) {
    int size_to_read = 100;
    char buffer[size_to_read]; 
    int how_much_read = read(fd, buffer, size_to_read);
    if(how_much_read == -1) {
        fprintf(stderr, "Reading failed due to error from %s\n", strerror(errno));
        exit(1);
    }
    for (int i = 0; i < how_much_read; i++) {
        // EOF is a 0x04
        if(buffer[i] == 4) {
            if(fd == 0) {
                return -1; 
            } else {

                return -2; 
            }
        }
        if(buffer[i] == 3) {
            kill(pid, SIGINT);
            return 0;
        }
        if(buffer[i] == '\r' || buffer[i] == '\n') {
            if(fd == 0) {
                //if we get input from the keyboard 
                int ret = write(1,"\r\n", 2);
                if(ret == -1) {
                    fprintf(stderr, "Writing to stdout failed due to %s\n", strerror(errno));
                    exit(1);
                }
                //should be sent the shell as well
                ret = write(shell_fd, "\n", 1);
                if(ret == -1 || errno == EPIPE) {
                    return -3;
                }
            } else {
                int ret = write(1, "\r\n", 2);
                if(ret == -1) {
                    fprintf(stderr, "Writing to stdout failed due to %s\n", strerror(errno));
                    exit(1);
                }
            }
        } else {
            int ret = write(1, buffer+i, 1);
            if(fd == 0) {
                if(ret == -1) {
                    fprintf(stderr, "Writing to stdout failed due to %s\n", strerror(errno));
                    exit(1);
                }
                ret = write(shell_fd, buffer+i, 1);
                if(ret == -1 || errno == EPIPE) {
                    return -3;
                }
            }
        }
    }
    return 0;
}


int main(int argc, char *argv[]){
    signal(SIGPIPE, SIG_IGN);
    int curr_option;
    const struct option options[] = {
        { "shell",  required_argument, NULL,  's' },
        { 0, 0, 0, 0}
    };

    char* name_of_program = NULL;
    while((curr_option = getopt_long(argc, argv, "s", options, NULL)) != -1)  {
        switch(curr_option) {
            case 's':
                name_of_program = optarg;
                break;
            default:
                fprintf(stderr, "Use the options --shell. Getting error %s \n", strerror(errno));
                exit(1);
                break;
        }
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
        exit(1);    
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
            printf("%s sad \n", name_of_program);
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
        poll_fds[0].fd = 0;
        poll_fds[0].events = POLLIN;
        
        int keyboard_alive = 1;
        int child_alive = 1; 
        if(nfds == 1) child_alive = 0;
        while(keyboard_alive || child_alive) {
            
            int ret = poll(poll_fds, nfds, -1);
            if (ret <= 0) {
                printf("Polling failed\r\n");
                exit(1); 
            }
            for (int input_fd = 0; input_fd < nfds; input_fd++) {
                if (poll_fds[input_fd].revents & POLLIN) {
                    int read_return = input_read(poll_fds[input_fd].fd,  tToS_fd[1],  pid);
                    if(read_return == -1) {
                        close(tToS_fd[1]);
                        keyboard_alive = 0;
                        continue;

                    } else if (read_return == -2 ) {
                        //we got an EOF from the shell
                        keyboard_alive = 0;
                        child_alive = 0;
                        continue;

                    } else if (read_return == -3 && nfds == 2) {
                        //we got a EPIPE
                        keyboard_alive = 0;
                        child_alive = 0;
                        continue;
                    }
                }

                if (poll_fds[input_fd].revents & POLLERR || poll_fds[input_fd].revents & POLLHUP) {


                    if(poll_fds[input_fd].fd != 0) {
                        //the poll error happens on the child
                        keyboard_alive = 0;
                        child_alive = 0;
                        break;
                    } else {
                        fprintf(stderr, "Polling failed on the keyboard %s\n", strerror(errno));
                        exit(1);
                    }
                    poll_fds[input_fd].fd = -1;
                }
            }
            
        }

    }


    int status;
    waitpid(pid, &status, 0);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d \r\n", (status&0x7F), (status&0xff00)>>8);
    set_current_terminal(&current_terminal);

    
}