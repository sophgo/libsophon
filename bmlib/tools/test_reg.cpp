#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define QUEUE2_NAME "/my_queue2"

int main() {
    struct mq_attr attr2;
    attr2.mq_flags = 0;
    attr2.mq_maxmsg = 10;
    attr2.mq_msgsize = sizeof(int); 
    attr2.mq_curmsgs = 0;


    printf("mq_flags: %d\n", attr2.mq_flags);
    printf("mq_maxmsg: %ld\n", attr2.mq_maxmsg);
    printf("mq_msgsize: %ld\n", attr2.mq_msgsize);
    printf("mq_curmsgs: %ld\n", attr2.mq_curmsgs);

    mqd_t complete_mq = mq_open(QUEUE2_NAME, O_WRONLY | O_CREAT, 0666, &attr2);

    if (complete_mq == (mqd_t)-1) {
        perror("mq_open");
        printf("Error code: %d\n", errno);
        return EXIT_FAILURE;
    }


    if (mq_close(complete_mq) == -1) {
        perror("mq_close");
        return EXIT_FAILURE;
    }


    if (mq_unlink(QUEUE2_NAME) == -1) {
        perror("mq_unlink");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
