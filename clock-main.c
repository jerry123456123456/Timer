//gcc clock-timer.c clock-main.c -o clock
//模拟时间表盘
#include <stdio.h>
#include "clock-timer.h"

void do_timer(timer_node_t *node) {
    (void)node;
    printf("do_timer expired now_time:%lu\n", now_time());
}

int stop = 0;
int main() {
    init_timer();
    add_timer(3, do_timer);
    add_timer(4, do_timer);
    add_timer(5, do_timer);

    check_timer(&stop);
    clear_timer();
    return 0;
}