#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include "rbt-timer.h"

void hello_world(timer_entry_t *te) {
    printf("hello world time = %u\n", te->rbnode.key);
}

int main()
{
    init_timer();

    add_timer(1000, hello_world);
    add_timer(2000, hello_world);
    add_timer(3000, hello_world);
    add_timer(3000, hello_world);

    int epfd = epoll_create(1);
    struct epoll_event events[512];

    for (;;) {
        int nearest = find_nearest_expire_timer();
        int n = epoll_wait(epfd, events, 512, nearest);
        for (int i=0; i < n; i++) {
            // 
        }
        expire_timer();
    }
    return 0;
}

// gcc rbt-timer.c rbtree.c -o rbt -I./