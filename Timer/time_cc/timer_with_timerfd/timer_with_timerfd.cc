#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h> // for timespec itimerspec
#include <unistd.h> // for close

#include <functional>
#include <chrono>
#include <set>
#include <memory>
#include <iostream>

using namespace std;

struct TimerNodeBase {
    time_t expire;
    uint64_t id; 
};

struct TimerNode : public TimerNodeBase {
    using Callback = std::function<void(const TimerNode &node)>;
    Callback func;
    TimerNode(int64_t id, time_t expire, Callback func) : func(func) {
        this->expire = expire;
        this->id = id;
    }
};

bool operator < (const TimerNodeBase &lhd, const TimerNodeBase &rhd) {
    if (lhd.expire < rhd.expire) {
        return true;
    } else if (lhd.expire > rhd.expire) {
        return false;
    } else return lhd.id < rhd.id;
}

class Timer {
public:
    static inline time_t GetTick() {
        return chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now().time_since_epoch()).count();
    }
    
    TimerNodeBase AddTimer(int msec, TimerNode::Callback func) {
        time_t expire = GetTick() + msec;
        if (timeouts.empty() || expire <= timeouts.crbegin()->expire) {
            auto pairs = timeouts.emplace(GenID(), expire, std::move(func));
            return static_cast<TimerNodeBase>(*pairs.first);
        }
        auto ele = timeouts.emplace_hint(timeouts.crbegin().base(), GenID(), expire, std::move(func));
        return static_cast<TimerNodeBase>(*ele);
    }
    
    void DelTimer(TimerNodeBase &node) {
        auto iter = timeouts.find(node);
        if (iter != timeouts.end())
            timeouts.erase(iter);
    }

    void HandleTimer(time_t now) {
        auto iter = timeouts.begin();
        while (iter != timeouts.end() && iter->expire <= now) {
            iter->func(*iter);
            iter = timeouts.erase(iter);
        }
    }

public:
    /*
    UpdateTimerfd 函数的主要功能是依据定时器集合 timeouts 里最早到期的定时器，对 timerfd（定时器文件描述符）的到期时间进行更新。这一操作能够让 timerfd 在最早的定时器到期时产生事件，进而实现对定时器事件的高效处理。
    参数说明
    fd：这是一个整数类型的参数，代表 timerfd 的文件描述符。该描述符用于和内核的定时器进行交互。
    */
    // 此函数用于更新 timerfd 的到期时间，使其与最早到期的定时器相匹配
    virtual void UpdateTimerfd(const int fd) {
        // 定义一个 timespec 结构体变量 abstime，用于存储绝对时间
        // timespec 结构体包含秒（tv_sec）和纳秒（tv_nsec）两个成员
        struct timespec abstime;
        // 获取定时器集合 timeouts 中最早到期的定时器的迭代器
        auto iter = timeouts.begin();
        // 检查 timeouts 集合是否为空
        if (iter != timeouts.end()) {
            // 若集合不为空，将最早到期的定时器的到期时间（以毫秒为单位）转换为秒
            // 并存储到 abstime 的 tv_sec 成员中
            abstime.tv_sec = iter->expire / 1000;
            // 计算最早到期的定时器的到期时间的毫秒部分，并转换为纳秒
            // 存储到 abstime 的 tv_nsec 成员中
            abstime.tv_nsec = (iter->expire % 1000) * 1000000;
        } else {
            // 若集合为空，将 abstime 的秒和纳秒都设置为 0
            // 表示没有定时器需要处理
            abstime.tv_sec = 0;
            abstime.tv_nsec = 0;
        }
        // 定义一个 itimerspec 结构体变量 its，用于设置 timerfd 的到期时间和间隔
        // itimerspec 结构体包含两个 timespec 成员：it_interval 和 it_value
        // it_interval 表示定时器的间隔时间，这里设置为空，表示不使用周期性定时器
        // it_value 表示定时器的首次到期时间，设置为 abstime
        struct itimerspec its = {
            .it_interval = {},
            .it_value = abstime
        };
        // 调用 timerfd_settime 函数，设置 timerfd 的到期时间
        // fd 是 timerfd 的文件描述符
        // TFD_TIMER_ABSTIME 表示使用绝对时间
        // &its 是指向 itimerspec 结构体的指针，包含了到期时间的设置
        // nullptr 表示不获取旧的定时器设置
        timerfd_settime(fd, TFD_TIMER_ABSTIME, &its, nullptr);
    }

private:
    static inline uint64_t GenID() {
        return gid++;
    }
    static uint64_t gid; 

    set<TimerNode, std::less<>> timeouts;
};

uint64_t Timer::gid = 0;

int main(){
    int epfd = epoll_create(1);

    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    struct epoll_event ev = {.events=EPOLLIN | EPOLLET};
    epoll_ctl(epfd, EPOLL_CTL_ADD, timerfd, &ev);
    
    unique_ptr<Timer> timer = make_unique<Timer>();
    int i = 0;
    timer->AddTimer(1000, [&](const TimerNode &node) {
        cout << Timer::GetTick() << " node id:" << node.id << " revoked times:" << ++i << endl;
    });

    timer->AddTimer(1000, [&](const TimerNode &node) {
        cout << Timer::GetTick() << " node id:" << node.id << " revoked times:" << ++i << endl;
    });

    timer->AddTimer(3000, [&](const TimerNode &node) {
        cout << Timer::GetTick() << " node id:" << node.id << " revoked times:" << ++i << endl;
    });

    auto node = timer->AddTimer(2100, [&](const TimerNode &node) {
        cout << Timer::GetTick() << " node id:" << node.id << " revoked times:" << ++i << endl;
    });
    timer->DelTimer(node);

    cout << "now time:" << Timer::GetTick() << endl;

    struct epoll_event evs[64] = {0};
    while(true){
        timer->UpdateTimerfd(timerfd);
        int n = epoll_wait(epfd,evs,64,-1);
        time_t now = Timer::GetTick();
        for (int i = 0; i < n; i++) {
            // for network event handle
        }
        timer->HandleTimer(now);
    }

    epoll_ctl(epfd, EPOLL_CTL_DEL, timerfd, &ev);
    close(timerfd);
    close(epfd);
    
    return 0;
}