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
    int64_t id;
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
    if (lhd.expire < rhd.expire)
        return true;
    else if (lhd.expire > rhd.expire) 
        return false;
    return lhd.id < rhd.id;
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
    //以便根据 `timeouts` 集合中最早到期的定时器节点来设置定时器的到期时间
    virtual void UpdateTimerfd(const int fd){
        struct timespec abstime;
        auto iter=timeouts.begin();
        if(iter!=timeouts.end()){
            abstime.tv_sec = iter->expire / 1000;
            abstime.tv_nsec = (iter->expire % 1000) * 1000000;
        }else{
            abstime.tv_sec = 0;
            abstime.tv_nsec = 0;
        }
        struct itimerspec its = {
            .it_interval = {},
            .it_value = abstime
        };
        //FD_TIMER_ABSTIME  表示绝对时间
        timerfd_settime(fd, TFD_TIMER_ABSTIME, &its, NULL);
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
    int epfd=epoll_create(1);

    int timerfd=timerfd_create(CLOCK_MONOTONIC,0);
    struct epoll_event ev={.events=EPOLLIN | EPOLLET};
    epoll_ctl(epfd,EPOLL_CTL_ADD,timerfd,&ev);

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

    struct epoll_event evs[64]={0};
    while(true){
        timer->UpdateTimerfd(timerfd);
        //不仅时间到会退出，timerfd对应的时间触发也会推出
        int n=epoll_wait(epfd,evs,64,-1);
        time_t now=Timer::GetTick();
        for(int i=0;i<n;i++){
            // for network event handle
        }
        timer->HandleTimer(now);
    }
    epoll_ctl(epfd, EPOLL_CTL_DEL, timerfd, &ev);
    close(timerfd);
    close(epfd);
    return 0;
}