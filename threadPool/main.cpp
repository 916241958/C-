/*
*        程序名：main.cpp
*	     功能：线程池各项功能代码测试 附测试代码
*	     作者：yjam
*/
#include <iostream>
#include <chrono>
#include <thread>
#include "Threadpool.h"
/*
example 1:
class MyTask : public Task {
public:
    void run() {
        std::cout << "tid : " << std::this_thread::get_id() << " has begin." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "tid : " << std::this_thread::get_id() << " has end." << std::endl;
    }
};
*/

/*
example 2:
有些场景是希望能够取得线程执行任务的返回值的 例如：计算1到3w的和
线程1 负责计算1到1w的和
线程2 负责计算1w到2w的和
线程3 负责计算2w到3w的和
主线程main 负责给每个线程分配计算的区间 并等待他们算完返回结果进行合并
问题描述：
1.提交任务后 应该返回一个Result对象 然后用这个对象调对应的方法去获取任务的返回值 
(1)如果调用方法去获取返回值的时候，这个线程将任务执行完了 没说的 直接获取返回值
(2)如果调用方法的时候这个线程没有执行完呢？这个方法应该阻塞住，等待线程将任务执行完毕
如何设计Result？
2.但是每个任务执行的结果返回值类型不同 虚函数和模板不能同时使用 应该如何去设计run的返回值 去接收任意类型呢？
解决方案：用前面所学的C++知识，构建一个可用接收任意类型的Any类
Any => Base* -> Drive:public Base Drive里面有一个data 模版类型
*/
using uLong = unsigned long long;
class MyTask : public Task {
public:
    MyTask(uLong begin, uLong end):begin_(begin)
            ,end_(end){}
    Any run() {
        std::cout << "tid : " << std::this_thread::get_id() << " has begin." << std::endl;
        uLong sum = 0;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        for (uLong i = begin_; i <= end_; i++) {
            sum += i;
        }
        std::cout << "tid : " << std::this_thread::get_id() << " has end." << std::endl;
        return sum;
    }
private:
    int begin_;
    int end_;
};
int main()
{
    //example 4:死锁问题分析
    {
        ThreadPool pool;
        pool.setMode(PoolMode::MODE_CACHED);
        pool.start(2);
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        pool.submitTask(std::make_shared<MyTask>(1, 100000000));

    }
    std::cout << "main over" << std::endl;
    /*
    //问题1：ThreadPool对象析构以后 怎么把线程池相关的线程资源全部回收？
    //example 3: 用户自定义的可增长的线程池工作模式
    {
        ThreadPool pool;
        //用户可以自定义线程池的工作模式
        pool.setMode(PoolMode::MODE_CACHED);
        pool.start(4);
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        uLong sum1 = res1.get().cast_<uLong>();
        uLong sum2 = res2.get().cast_<uLong>();
        uLong sum3 = res3.get().cast_<uLong>();
        std::cout << "-------------" << (sum1 + sum2 + sum3) << std::endl;
    }
    */
    /*example 2: 获取用户任务的返回值
    //Master - Slave线程模型
    //1.Master线程将整个大任务分解 然后给各个Slave线程分配任务
    ThreadPool pool;
    pool.start(4);
    Result res1 = pool.submitTask(std::make_shared<MyTask>(1,100000000));
    Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
    Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    //2.等待各个Slave线程执行完任务 返回结果
    uLong sum1 = res1.get().cast_<uLong>();//get返回了一个Any类型，怎么转换成对应的具体类型呢？
    uLong sum2 = res2.get().cast_<uLong>();
    uLong sum3 = res3.get().cast_<uLong>();
    //3.Master 线程合并各个结果并输出
    std::cout << (sum1 + sum2 + sum3) << std::endl;*/
    getchar();
    //return 0;
}