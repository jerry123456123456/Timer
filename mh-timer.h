#ifndef MARK_MINHEAP_TIMER_H
#define MARK_MINHEAP_TIMER_H

//检查是否在苹果系统上
#if defined(__APPLE__)     
#include <AvailabilityMacros.h>
#include <sys/time.h>
#include <mach/task.h>
#include <mach/mach.h>
#else

#include <time.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "minheap.h"

static min_heap_t min_heap;

//计算当前时间戳
static uint32_t
current_time() {
	uint32_t t;
#if !defined(__APPLE__) || defined(AVAILABLE_MAC_OS_X_VERSION_10_12_AND_LATER)
	struct timespec ti;
	clock_gettime(CLOCK_MONOTONIC, &ti);
	t = (uint32_t)ti.tv_sec * 1000;
	t += ti.tv_nsec / 1000000;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	t = (uint32_t)tv.tv_sec * 1000;
	t += tv.tv_usec / 1000;
#endif
	return t;
}

void init_timer(){
    min_heap_ctor_(&min_heap);
}

//用于向定时器堆中添加一个计时器
timer_entry_t *add_timer(uint32_t msec,timer_handler_pt callback){
    timer_entry_t *te=(timer_entry_t*)malloc(sizeof(*te));
    if(!te){
        return NULL;
    }
    memset(te,0,sizeof(timer_entry_t));
    te->handler=callback;
    te->time=current_time()+msec;
    if(min_heap_push_(&min_heap,te)!=0){
        free(te);
        return NULL;
    }
    printf("add timer time = %u now = %u\n", te->time, current_time());
    return te;
}

bool del_timer(timer_entry_t *e) {
    return 0 == min_heap_erase_(&min_heap, e);
}

//找到最接近的定时器，请返回和当前时间戳的时间差
int find_nearest_expire_timer(){
    timer_entry_t *te=min_heap_top_(&min_heap);
    if(!te)return -1;
    int diff=(int)te->time-(int)current_time();
    return diff > 0 ? diff : 0;
}

//这段代码是一个函数 `expire_timer()`，用于处理到期的定时器条目
void expire_timer() {
    uint32_t cur = current_time();
    for (;;) {
        timer_entry_t *te = min_heap_top_(&min_heap);
        if (!te) break;
        if (te->time > cur) break;
        te->handler(te);
        min_heap_pop_(&min_heap);
        free(te);
    }
}
#endif