//使用epoll_wait第四个参数手撕定时器韩式代码
//精彩绝伦

#include<sys/epoll.h>
#include<functional>
#include<set>
#include<chrono>
#include<iostream>
#include<memory>

using namespace std;

struct TimerNodeBase{
    time_t expire;       //表示定时器的过期时间
    int64_t id;   //当设置的定时时间相同时，id小的放在前面
};

struct TimerNode : public TimerNodeBase{
    //起别名，并使用函数包装提供统一的接口
    using Callback=std::function<void(const TimerNode &node)>;
    //func是一个函数对象，作为参数传给构造函数，是回调函数用于在定时器触发执行的特定操作
    Callback func;
    //构造函数
    TimerNode(int64_t id,time_t expire,Callback func):func(func){
        this->expire=expire;
        this->id=id;
    } 
};

//运算符重载,不进可以比较基类，派生类也可以比较
bool operator < (const TimerNodeBase &lhd,const TimerNodeBase &rhd){
    if(lhd.expire<rhd.expire){
        return true;
    }else if(lhd.expire>rhd.expire){
        return false;
    }
    return lhd.id<rhd.id;
}

/*
- **Unix 纪元（Epoch）：** 在 Unix 系统中，纪元通常被定义为 1970 年 1 月 1 日 00:00:00 UTC（协调世界时）。

- **Windows 纪元（Epoch）：** 在 Windows 系统中，纪元通常被定义为 1601 年 1 月 1 日 00:00:00 UTC。

- **C++ `<chrono>` 纪元（Epoch）：** C++ 的 `<chrono>` 库使用了一个自定义的纪元，通常与 Unix 纪元相同，即 1970 年 1 月 1 日 00:00:00 UTC。
*/

class Timer{
public:
    //这段代码用于获取当前时间的毫秒级时间戳
    static time_t GetTick(){
        // `chrono::system_clock::now()` 返回当前时刻的时间点
        // `chrono::time_point_cast<chrono::milliseconds>(...)` 将时间点转换为毫秒级精度的时间点，并将结果赋给 `sc`
        auto sc = chrono::time_point_cast<chrono::milliseconds>(chrono::system_clock::now());
        // `sc.time_since_epoch()` 返回当前时间点相对于时钟的起点（纪元）的时间段
        // `chrono::duration_cast<chrono::milliseconds>(...)` 将时间段转换为毫秒单位的时间段，并将结果赋给 `temp`
        //总的来说这个代码是获取当前时间点sc相对于时钟的起点的时间段并转为毫秒级别
        auto temp = chrono::duration_cast<chrono::milliseconds>(sc.time_since_epoch());
        //返回持续的毫秒数，即当前时间相对于时钟的其实的毫秒数
        return temp.count();
    }

    TimerNodeBase AddTimer(time_t msec,TimerNode::Callback func){
        time_t expire=GetTick() + msec;   //期待的超时时间戳
        //这个if里面的条件是容器为空，或者添加的超时时间戳比容器内最后一个要小
        if(timeouts.empty() || expire<=timeouts.crbegin()->expire){   //crbegin是容器反向第一个元素
            //emplace函数用于在容器中构造一个新的元素，返回pair类型其中first是指向新元素的迭代器
            auto pairs=timeouts.emplace(GenID(),expire,std::move(func));
            //将 `pairs.first` 指向的对象转换为 `TimerNodeBase` 类型，并将其作为返回值
            return static_cast<TimerNodeBase>(*pairs.first);
        }
        // 如果新的超时时间比当前容器中最晚的超时时间晚，那么根据逆序迭代器的位置提示，将新的定时器节点插入到容器中
        auto ele=timeouts.emplace_hint(timeouts.crbegin().base(),GenID(),expire,std::move(func));
        return static_cast<TimerNodeBase>(*ele);
    }

    bool DelTimer(TimerNodeBase &node){
        auto iter=timeouts.find(node);
        if(iter !=timeouts.end()){
            timeouts.erase(iter);
            return true;
        }
        return false;
    }

    void HandleTimer(time_t now){
        auto iter=timeouts.begin();
        //没遍历到末尾，且当前过期时间早于或等于当前时间
        while(iter !=timeouts.end() && iter->expire<=now){
            iter->func(*iter);
            iter=timeouts.erase(iter);
        }
    }

    //用于计算下一个定时器到期需要等待的时间
    time_t TimeToSleep() {
        auto iter = timeouts.begin();
        if (iter == timeouts.end()) {
            return -1;
        }
        time_t diss = iter->expire - GetTick();
        return diss > 0 ? diss : 0;
    }


private:
    //用于生成唯一的id
    static int64_t GenID(){
        return gid++;
    }
    static int64_t gid;
    //当元素类型没有提供自定义的比较函数时，`std::less<>` 会被用作默认的比较函数
    set<TimerNode,std::less<>> timeouts;  //这里使用了运算符重载
};
int64_t Timer::gid=0;

int main(){
    int epfd=epoll_create(1);

    //Timer* timer = new Timer(); // 手动创建 Timer 对象
    unique_ptr<Timer> timer=make_unique<Timer>();

    int i =0;
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
    epoll_event ev[64] = {0};

    while(true){
        //每有定时器到期时epoll_wait退出
        int n=epoll_wait(epfd,ev,64,timer->TimeToSleep());
        time_t now =Timer::GetTick();
        for (int i = 0; i < n; i++) {
            /**/
        }
        /* 处理定时事件*/
        timer->HandleTimer(now);

    }

    return 0;
}