#ifndef _RBT_
#define _RBT_

#include<stdio.h>
#include<stdint.h>  // 包含标准整数类型库，提供了精确宽度的整数类型，如 uint32_t
#include<string.h>
#include<unistd.h> // 包含 Unix 标准库，提供了 usleep 等函数
#include<stdlib.h>
#include<stddef.h>  // 包含标准库，提供了 offsetof 宏，用于计算结构体成员的偏移量

// 如果是苹果系统
#if defined(__APPLE__)
// 包含苹果系统的可用性宏定义
#include <AvailabilityMacros.h>
// 包含系统时间相关的头文件
#include <sys/time.h>
// 包含苹果系统的任务管理相关头文件
#include <mach/task.h>
// 包含苹果系统的 Mach 内核相关头文件
#include <mach/mach.h>
// 非苹果系统
#else
#include<time.h>
#endif

#include"skiplist.h"

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

zskiplist *init_timer(){
    return zslCreate();
}

zskiplistNode *add_timer(zskiplist *zsl,uint32_t msec,handler_pt func){
    msec += current_time();
    printf("add_timer expire at msec = %u\n", msec);
    return zslInsert(zsl, msec, func);
}

void del_timer(zskiplist *zsl, zskiplistNode *zn) {
    zslDelete(zsl, zn);
}


void expire_timer(zskiplist *zsl) {
    zskiplistNode *x;
    uint32_t now = current_time();
    for (;;) {
        x = zslMin(zsl);
        if (!x) break;
        if (x->score > now) break;
        printf("touch timer expire time=%lu, now = %u\n", x->score, now);
        x->handler(x);
        zslDeleteHead(zsl);
    }
}

#endif