#include<sys/epoll.h>
#include<functional> // 用于 std::function 回调函数
#include<chrono>  //高精度时间处理
#include<set> //有序集合
#include<memory> //智能指针
#include<iostream>

using namespace std;

//定时器节点基类
struct TimerNodeBase {
    time_t expire;   //定时器过期时间，单位ms，从epoch
    int64_t id;   //定时器唯一标识，用于在相同过期时间情况下区分
};

//定时器节点的派生类，包含回调函数
struct TimerNode : public TimerNodeBase {
    /*
    using 等价于
    typedef std::function<void(const TimerNode &node)> Callback;
    */
    using Callback = std::function<void(const TimerNode &node)>;  //回调函数类型定义
    Callback func;   //定时器触发时执行的回调
    //构造函数：初始化id、expire和回调函数
    TimerNode(int64_t id,time_t expire,Callback func) : func(func){
        this->expire = expire;
        this->id = id;
    }
};

// 定义 TimerNodeBase 的小于运算符（用于 set 排序）
// 排序规则：先按 expire 升序，若相同则按 id 升序
bool operator < (const TimerNodeBase &lhd,const TimerNodeBase &rhd){
    if(lhd.expire < rhd.expire){
        return true;
    }else if(lhd.expire > rhd.expire){
        return false;
    }
    return lhd.id < rhd.id;
}

//定时器管理类
class Timer {
public:
    // 获取当前时间（毫秒级，基于 std::chrono::steady_clock）
    static time_t GetTick(){
        //将当前的时间转换为毫秒时间戳
        /*
        1. auto sc = chrono::time_point_cast<chrono::milliseconds>(chrono::steady_clock::now());
        chrono::steady_clock::now()：
        std::chrono::steady_clock 属于 C++ 标准库中的时钟类，now() 是该类的静态成员函数，其功能是返回当前时间点。这个时间点是相对于 std::chrono::steady_clock 的纪元而言的。
        chrono::time_point_cast<chrono::milliseconds>()：
        std::chrono::time_point_cast 是一个模板函数，其作用是将一个时间点从一种精度转换为另一种精度。这里把 std::chrono::steady_clock::now() 返回的时间点转换为以毫秒为单位的时间点。
        auto sc：
        auto 是 C++ 的自动类型推导关键字，编译器会依据初始化表达式的类型自动推断 sc 的类型。sc 实际上是一个 std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> 类型的对象，代表以毫秒为单位的当前时间点。
        */
        auto sc = chrono::time_point_cast<chrono::milliseconds>(chrono::steady_clock::now());
        /*
        sc.time_since_epoch()：
        time_since_epoch() 是 std::chrono::time_point 类的成员函数，它会返回从纪元到当前时间点所经过的时间间隔。这个时间间隔的类型是 std::chrono::steady_clock::duration，其精度取决于 std::chrono::steady_clock 的实现。
        chrono::duration_cast<chrono::milliseconds>()：
        std::chrono::duration_cast 是一个模板函数，用于将一个时间间隔从一种精度转换为另一种精度。这里把 sc.time_since_epoch() 返回的时间间隔转换为以毫秒为单位的时间间隔。
        auto temp：
        同样使用 auto 关键字，编译器会自动推断 temp 的类型。temp 实际上是一个 std::chrono::milliseconds 类型的对象，代表从纪元到当前时间点所经过的毫秒数。
        */
        auto temp = chrono::duration_cast<chrono::milliseconds>(sc.time_since_epoch());
        /*
        temp.count()：
        count() 是 std::chrono::duration 类的成员函数，其作用是返回时间间隔的计数值。对于 std::chrono::milliseconds 类型的对象，count() 会返回以毫秒为单位的计数值。
        return：
        将这个计数值作为函数的返回值，也就是当前时间的毫秒级时间戳。
        */
        return temp.count();  
    }

    //添加定时器：参数为延迟时间（ms）和回调函数
    TimerNodeBase AddTimer(time_t msec,TimerNode::Callback func){
        //计算过期时间
        time_t expire = GetTick() + msec;
        //判断是否插入到集合末尾（优化性能）
        if(timeouts.empty() || expire <= timeouts.crbegin()->expire){
            // emplace 直接构造元素并插入（返回值为 pair<iterator, bool>）
            //在容器内部构造，防止外部构造之后再拷贝
            //里面的move是把func的资源直接转移给容器内新构造的对象，避免拷贝
            auto pairs = timeouts.emplace(GenID(),expire,std::move(func));
            // 返回基类对象（通过 static_cast 转换）,避免暴漏子类的内部实现
            return static_cast<TimerNodeBase>(*pairs.first);
        }
        //如果一直使用同一个msec，可能会一直向最右边插入，模版提供crbegin直接访问到红黑树最右侧节点
        /*
        emplace_hint 允许你提供一个迭代器作为插入位置的提示。容器会尝试在该提示位置附近插入新元素，这样可以减少插入操作所需的查找时间，从而优化性能
        timeouts.crbegin()：crbegin() 是 std::set 容器的一个成员函数，它返回一个常量反向迭代器，指向容器的最后一个元素。反向迭代器的方向与正向迭代器相反，所以 crbegin() 指向的是容器中按排序规则最大的元素。
        .base()：base() 是反向迭代器的一个成员函数，它将反向迭代器转换为对应的正向迭代器。因为 emplace_hint 函数需要的是正向迭代器作为提示位置，所以需要将反向迭代器转换为正向迭代器。
        综合起来，timeouts.crbegin().base() 得到的是指向容器中最后一个元素之后位置的正向迭代器，作为插入位置的提示。
        */
        auto ele = timeouts.emplace_hint(timeouts.crbegin().base(),GenID(),expire,std::move(func));
        return static_cast<TimerNodeBase>(*ele);
    }

    //删除定时器：根据TimerNodeBase对象删除
    bool DelTimer(TimerNodeBase &node){
        auto iter = timeouts.find(node);  //在set中查找节点
        if(iter != timeouts.end()){
            timeouts.erase(iter);
            return true;
        }
        return false;
    }

    //处理到期的定时器，遍历并执行回调
    void HandleTimer(time_t now){
        auto iter = timeouts.begin();
        //循环处理所有过期时间 <= now的定时器
        while(iter != timeouts.end() && iter->expire <= now){
            iter->func(*iter);  // 执行回调函数（传入当前节点引用）
            // 删除节点并获取下一个迭代器（避免迭代器失效）
            iter = timeouts.erase(iter);
        }
    }

    //计算剩余睡眠时间：返回距离下一个定时器到期的时间（ms）
    time_t TimeToSleep(){
        auto iter = timeouts.begin();
        if(iter == timeouts.end()){
            //无定时器返回-1，epoll永久阻塞
            return -1;
        }
        time_t diss = iter->expire - GetTick(); // 计算当前时间到最近过期时间的差值
        return diss > 0  ? diss : 0; // 差值为负时返回 0（立即触发）
    }

private:
    //生成唯一ID（静态成员，保证每个定时器的ID唯一）
    static int64_t GenID(){
        return gid++;   
    }
    static int64_t gid;
    /*
    std::less<Key> 是一个 函数对象（ functor），它重载了 operator()，并在调用时等价于调用 a < b。例如：
    std::less<Key> comp;
    comp(a, b);  // 等价于 a < b
    因此，当 std::set 使用 std::less<> 作为比较器时（此处 <> 会自动推导为 TimerNode），它实际上是通过调用 TimerNode 对象的 operator< 来决定元素的顺序和唯一性。
    */
    set<TimerNode,std::less<>> timeouts;    // 有序集合（按 operator< 排序）
};

// 初始化静态成员 gid
int64_t Timer::gid = 0;

int main(){
    //创建epoll实例
    int epfd = epoll_create(1);

    //创建定时器管理对象
    //std::unique_ptr<Timer> timer(new Timer());
    unique_ptr<Timer> timer = make_unique<Timer>();

    int i = 0;  //用于统计定时器的触发次数

    //添加第一个定时器：1000ms后触发，回调输出时间、ID和触发次数
    timer->AddTimer(1000, [&](const TimerNode &node){
        cout << Timer::GetTick() << " node id:" << node.id << " revoked times:" << ++i << endl;
    });

    // 添加第二个定时器：1000ms 后触发（与第一个 ID 不同）
    timer->AddTimer(1000, [&](const TimerNode &node) {
        cout << Timer::GetTick() << " node id:" << node.id << " revoked times:" << ++i << endl;
    });

    // 添加第三个定时器：3000ms 后触发
    timer->AddTimer(3000, [&](const TimerNode &node) {
        cout << Timer::GetTick() << " node id:" << node.id << " revoked times:" << ++i << endl;
    });

    // 添加第四个定时器：2100ms 后触发，但随后被删除
    auto node = timer->AddTimer(2100, [&](const TimerNode &node) {
        cout << Timer::GetTick() << " node id:" << node.id << " revoked times:" << ++i << endl;
    });
    timer->DelTimer(node);  // 删除刚添加的第四个定时器（不会触发）

    // 输出当前时间（验证定时器起始点）
    cout << "now time:" << Timer::GetTick() << endl;

    // 定义 epoll 事件数组（大小 64，实际项目中按需调整）
    epoll_event ev[64] = {0};

    //主循环：事件驱动处理
    while(true){
        //超时事件由Timer::TimeToSleep()计算（最近定时器的剩余时间），也就是说，时间到了，epoll就不阻塞了
        int n = epoll_wait(epfd,ev,64,timer->TimeToSleep());
        time_t now = Timer::GetTick();  //获取当前时间

        // 处理 epoll 事件（此处预留占位符，实际可添加网络事件处理）
        for (int i = 0; i < n; i++) {
            /**/  // 此处应添加实际事件处理逻辑（如网络 I/O）
        }

        // 处理到期的定时器（无论 epoll 是否触发，都检查定时器）
        timer->HandleTimer(now);
    }

    return 0;
}