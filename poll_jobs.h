#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
// #include "file_ops.h"

struct poller_args{
	int job_id;
	char *fname;
};

void *poll_file(void* args){

    struct poller_args* pargs = (struct poller_args*)args;
    char* ret; 

    struct pollfd *pfd;
    int num_open_fd = 0;
    int nfds;
    pfd = calloc(1, sizeof(struct pollfd));
    if (pfd == NULL){
        perror("Failed to malloc");
        goto clean;
    }
    
    pfd[0].fd = open(pargs->fname, O_RDONLY);
    if (pfd[0].fd == -1){
        perror("Failed to open file");
        goto clean;
    }
    
    pfd[0].events = POLLIN;
    num_open_fd = 1;
    nfds = 1;

    while (num_open_fd > 0){
        int ready;
        char buf[4096]; // Change the size

        ready = poll(pfd, nfds, 30);
        if (ready == -1){
            printf("Polling Failed for job id: %d\n", pargs->job_id);
            goto clean;
        }

        if (pfd[0].revents != 0){
            printf("fd=%d; events: %s%s%s\n", pfd[0].fd,
                               (pfd[0].revents & POLLIN)  ? "POLLIN "  : "",
                               (pfd[0].revents & POLLHUP) ? "POLLHUP " : "",
                               (pfd[0].revents & POLLERR) ? "POLLERR " : "");
        }

        if (pfd[0].revents & POLLIN){
            ssize_t s = read(pfd[0].fd, buf, sizeof(buf));
            if (s == -1){
                printf("Failed to read data");
                goto clean;
            }
            printf("read %zd bytes: %.*s\n", s, (int) s, buf);

        } else {                
                /* POLLERR | POLLHUP */
                printf("closing fd %d\n", pfd[0].fd);
                if (close(pfd[0].fd) == -1) {
                    printf("closing the file failed \n");
                    goto clean;
                }
                num_open_fd--;
        }
    }

    clean:
      if (pfd != NULL){
          free(pfd);
      }
    
    if ((ret = (char*) malloc(20)) == NULL) {
        perror("malloc() error");
        exit(2);
    }
    strcpy(ret, "This is a test");
    pthread_exit(ret);

}
