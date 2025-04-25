### 零声教育出品  Mark 老师 QQ：2548898954

### 编译

#### 最小堆

```shell
# 关联文件 mh-timer.c mh-timer.h minheap.h minheap.c
gcc mh-timer.c minheap.c -o mh -I./
```

#### 红黑树

```shell
# 关联文件 rbt-timer.c rbt-timer.h rbtree.c rbtree.h
gcc rbt-timer.c rbtree.c -o rbt -I./
```

#### 跳表

```shell
# 关联文件 skiplist.h skiplist.c skl-timer.c
gcc skiplist.c skl-timer.c -o skl -I./
```

#### 多层级时间轮

```shell
# 关联文件 timewheel.h timewheel.c tw-timer.c spinlock.h
gcc timewheel.c tw-timer.c -o tw -I./ -lpthread
```

#### 模拟时间表盘
```shell
# 关联文件 clock-timer.h clock-timer.c clock-main.c spinlock.h
gcc clock-timer.c clock-main.c -o clock -I./ -lpthread
```

#### C++ 面试手撕定时器演示代码（epoll_wait第4个参数驱动）
```shell
g++ timer.cc -o timer -std=c++14
```

#### C++ 面试定时器演示代码（timerfd驱动）
```shell
g++ timer_with_timerfd.cc -o timer_with_timerfd -std=c++14
```

