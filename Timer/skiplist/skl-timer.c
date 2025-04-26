#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include "skl-timer.h"

void print_hello(zskiplistNode *zn) {
    printf("hello world time = %lu\n", zn->score);
}


int main()
{
    zskiplist *zsl = init_timer();
    add_timer(zsl, 3010, print_hello);
    add_timer(zsl, 4004, print_hello);
    zskiplistNode *zn = add_timer(zsl, 3005, print_hello);
    del_timer(zsl, zn);
    add_timer(zsl, 5008, print_hello);
    add_timer(zsl, 7003, print_hello);
    // zslPrint(zsl);
    for (;;) {
        expire_timer(zsl);
        usleep(10000);
    }
    return 0;
}


// gcc skiplist.c skl-timer.c -o skl -I./