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

#include"rbtree.h"

//定义一个红黑树对象，用于管理定时器
ngx_rbtree_t timer;
//定义一个红黑树的哨兵节点，用于表示红黑树的边界
static ngx_rbtree_node_t sentinel;

// 1. 前置声明结构体
struct timer_entry_s;

// 2. 定义函数指针（参数为前置声明的结构体指针，合法）
typedef void (*timer_handler_pt)(struct timer_entry_s *ev);

// 3. 完整定义结构体（此时 timer_handler_pt 已定义）
struct timer_entry_s {
    ngx_rbtree_node_t rbnode;
    timer_handler_pt handler; // 现在合法
};

// 4. 定义别名
typedef struct timer_entry_s timer_entry_t;

// 获取当前时间的函数，返回值为毫秒级时间
static uint32_t
current_time() {
    uint32_t t;
    // 如果不是苹果系统或者是 macOS 10.12 及以上版本
#if !defined(__APPLE__) || defined(AVAILABLE_MAC_OS_X_VERSION_10_12_AND_LATER)
    struct timespec ti;
    // 获取单调时间，不受系统时间调整的影响
    clock_gettime(CLOCK_MONOTONIC, &ti);
    // 计算秒级时间转换为毫秒
    t = (uint32_t)ti.tv_sec * 1000;
    // 加上纳秒级时间转换为毫秒
    t += ti.tv_nsec / 1000000;
// 苹果系统且版本低于 macOS 10.12
#else
    struct timeval tv;
    // 获取当前时间
    gettimeofday(&tv, NULL);
    // 计算秒级时间转换为毫秒
    t = (uint32_t)tv.tv_sec * 1000;
    // 加上微秒级时间转换为毫秒
    t += tv.tv_usec / 1000;
#endif
    return t;
}

//初始化定时器红黑树的函数，返回红黑树的指针
 ngx_rbtree_t *init_timer(){
    //初始化红黑树，传入红黑树对象、哨兵节点以及插入函数
    ngx_rbtree_init(&timer ,&sentinel,ngx_rbtree_insert_timer_value);
    return &timer;
}

// 向定时器红黑树中添加一个定时器的函数
 timer_entry_t* add_timer(uint32_t msec, timer_handler_pt func) {
    // 分配定时器条目结构体的内存
    timer_entry_t *te = (timer_entry_t *)malloc(sizeof(timer_entry_t));
    // 将分配的内存清零
    memset(te, 0, sizeof(timer_entry_t));
    // 设置定时器处理函数
    te->handler = func;
    // 计算定时器的到期时间，为当前时间加上指定的毫秒数
    msec += current_time();
    // 打印定时器的到期时间
    printf("add_timer expire at msec = %u\n", msec);
    // 设置红黑树节点的键为定时器的到期时间
    te->rbnode.key = msec;
    // 将定时器条目插入红黑树
    ngx_rbtree_insert(&timer, &te->rbnode);
    return te;
}

//从当前定时器红黑树中删除一个定时器函数
 void del_timer(timer_entry_t *te){
    //从红黑树中删除定时器条目中对应的红黑树节点
    ngx_rbtree_delete(&timer,&te->rbnode);
    //释放定时器条目结构体
    free(te);
}

//查找最近到期的定时器的函数，返回距离最近到期定时器的时间差（ms)
 int find_nearest_expire_timer(){
    ngx_rbtree_node_t *node;
    //如果红黑树根节点是哨兵节点，说明红黑树为空
    if(timer.root == timer.sentinel){
        return -1;
    }
    //找到红黑树中键值最屌的节点，即最近到期的定时器
    node = ngx_rbtree_min(timer.root,timer.sentinel);
    //计算距离最近的到期的定时器的时间差
    int diff = (int)node->key - (int)current_time();
    //如果时间差小于0，说明已经过期，返回0，否则返回时间差
    return diff > 0 ? diff : 0;
}

//处理到期定时器的函数
 void expire_timer(){
    timer_entry_t *te;
    ngx_rbtree_node_t *sentinel,*root,*node;
    //获取红黑树的哨兵节点
    sentinel = timer.sentinel;
    //获取当前时间
    uint32_t now = current_time();
    //循环处理到期的定时器
    for(;;){
        //获取红黑树的根节点
        root = timer.root;
        //如果根节点是哨兵节点,说明红黑树为空，退出循环
        if(root == sentinel) break;
        //找到红黑树中键值最小的节点，即最近到期的定时器
        node = ngx_rbtree_min(root,sentinel);
        //如果最近到期的定时器还没有到期，退出循环
        if(node->key > now) break;
        // 打印定时器的到期时间和当前时间
        printf("touch timer expire time=%u, now = %u\n", node->key, now);
        // 根据红黑树节点的地址和偏移量计算定时器条目结构体的地址
        te = (timer_entry_t *) ((char *) node - offsetof(timer_entry_t, rbnode));
        //调用定时处理函数
        te->handler(te);
        // 从红黑树中删除定时器条目对应的红黑树节点
        ngx_rbtree_delete(&timer, &te->rbnode);
        // 释放定时器条目结构体的内存
        free(te);
    }
}

#endif