/*
*        程序名：Threadpool.cpp
*	     功能：线程池类中的方法实现
*	     作者：dyy
*/
#include"Threadpool.h"
#include<functional>
#include<thread>
#include<string>
#include<iostream>
const int TASK_MAX_THREADHOLD = INT32_MAX;
const int THREAD_MAX_THREADHOLD = 100;
const int THREAD_MAX_IDLE_TIME = 10;//单位:秒
//线程池构造函数
ThreadPool::ThreadPool():initThreadSize_(4)
        , taskSize_(0)
        , idleThreadSize_(0)
        , threadSizeThreadHold_(200)
        , curThreadSize_(0)
        , taskQueMaxThreadHold_(TASK_MAX_THREADHOLD)
        , poolMode_(PoolMode::MODE_FIXED)
        , isPoolRunning_(false)
{}

//线程池析构函数
ThreadPool::~ThreadPool() {

    isPoolRunning_ = false;
    //等待线程池中 所有的线程返回 有两种状态 ： 阻塞 & 正在执行任务中
    std::unique_lock<std::mutex>lock(taskQueMutex_);
    notEmpty_.notify_all();
    exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

//设置线程池的工作模式
void ThreadPool::setMode(PoolMode mode) {
    if (checkRunningState()) {
        return;
    }
    poolMode_ = mode;
}

//设置task任务队列上限的阈值
void ThreadPool::setTaskQueMaxThreadHold(int threadhold) {
    if (checkRunningState()) {
        return;
    }
    taskQueMaxThreadHold_ = threadhold;
}

//设置线程池cached模式下线程上限的阈值
void ThreadPool::setthreadSizeThreadHold(int threadhold) {
    if (checkRunningState()) {
        return;
    }
    if (poolMode_ == PoolMode::MODE_CACHED) {
        threadSizeThreadHold_ = threadhold;
    }
}

//开启线程池
void ThreadPool::start(int initThreadSize) {

    //线程启动时 将线程池的运行状态该为true
    isPoolRunning_ = true;

    //初始化线程池中初始线程个数
    initThreadSize_ = initThreadSize;

    //线程总数
    curThreadSize_ = initThreadSize;

    //创建线程对象
    for (int i = 0; i < initThreadSize_; i++) {
        //创建线程对象的时候 把线程函数给到thread线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
        //threads_.emplace_back(std::move(ptr));
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
    }

    //启动所有线程
    for (int i = 0; i < initThreadSize_; i++) {
        //每个线程需要执行一个线程函数
        threads_[i]->start();
        //每启动一个线程空闲线程数据加1
        idleThreadSize_++;
    }
}

//给线程池提交任务--用户调用接口生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {

    //获取锁
    std::unique_lock<std::mutex>lock(taskQueMutex_);

    //线程间的通信 等待任务队列有空余---当然了不能一直等待着 最长等待时间不超过1s 否则返回添加任务失败
    if (!notFull_.wait_for(lock
            , std::chrono::seconds(1)
            , [&]()->bool {return taskQue.size() < (size_t)taskQueMaxThreadHold_; }))
    {
        std::cerr << "taskQue is full,submitTask error." << std::endl;
        return Result(sp,false);
    }

    //如果有空余 将任务放入队列中
    taskQue.emplace(sp);
    taskSize_++;

    //添加任务后 任务队列不为空 在notEmpty上进行通知线程执行任务
    notEmpty_.notify_all();

    //cached模式 需要根据任务数量和空闲线程的数量 判断是否需要创建新的线程出来？
    if (poolMode_ == PoolMode::MODE_CACHED
        && taskSize_ > idleThreadSize_
        && curThreadSize_ < threadSizeThreadHold_)
    {
        std::cout << ">>> create new threads ..." << std::endl;
        //创建新线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getId();
        //将新创建的线程对象添加到线程队列
        threads_.emplace(threadId, std::move(ptr));
        //线程启动起来
        threads_[threadId]->start();
        //修改线程个数相关的变量
        idleThreadSize_++;
        curThreadSize_++;
    }

    return Result(sp);
}

//定义线程函数--线程池里面的线程从任务队列中消费任务
void ThreadPool::threadFunc(int threadId) {
    auto lastTime = std::chrono::high_resolution_clock().now();
    for(;;)
    {
        std::shared_ptr<Task>task;
        {
            //先获取锁
            std::unique_lock<std::mutex>lock(taskQueMutex_);

            std::cout << "tid : " << std::this_thread::get_id() << " 尝试获取任务..." << std::endl;

            //在cached模式下 可能已经创建了很多线程 但是空闲时间超过60s 应该把多余的线程结束回收掉
            //一秒返回一次
            while (taskQue.size() == 0) {
                //线程池要结束 回收线程资源
                if (!isPoolRunning_) {
                    threads_.erase(threadId);
                    exitCond_.notify_all();
                    std::cout << "threadid : " << std::this_thread::get_id() << " exit!" << std::endl;
                    return;
                }
                if (poolMode_ == PoolMode::MODE_CACHED) {
                    //条件变量超时返回
                    if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
                        auto nowTime = std::chrono::high_resolution_clock().now();
                        auto  dur = std::chrono::duration_cast<std::chrono::seconds>(nowTime - lastTime);
                        if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_) {
                            //开始回收当前线程
                            //记录线程相关数量的值修改
                            //把线程对象从线程列表容器中删除 没有办法匹配当前线程对应池中的哪一个线程对象
                            threads_.erase(threadId);
                            curThreadSize_--;
                            idleThreadSize_--;
                            std::cout << "threadid : " << std::this_thread::get_id() << " exit!" << std::endl;
                            return;
                        }
                    }
                }
                else {
                    //等待notEmoty_条件 等待队列不空
                    notEmpty_.wait(lock);
                }
                //线程池要结束 回收线程资源
                //if (!isPoolRunning_) {
                //	threads_.erase(threadId);
                //	std::cout << "threadid : " << std::this_thread::get_id() << " exit!" << std::endl;
                //	exitCond_.notify_all();
                //	return;
                //}
            }


            //线程被唤醒去执行任务空闲线程数量减1
            idleThreadSize_--;

            std::cout << "tid : " << std::this_thread::get_id() << " 获取任务成功..." << std::endl;

            //从任务队列中取一个任务出来
            task = taskQue.front();
            taskQue.pop();
            taskSize_--;
            if (taskQue.size() > 0)
            {
                notEmpty_.notify_all();
            }
            notFull_.notify_all();
        }
        if (task != nullptr) {
            //task->run();//执行任务 把任务的返回值用Setval方法给到Result对象
            task->exec();
        }
        //线程执行完任务 线程数量加1
        idleThreadSize_++;
        //更新线程执行完任务之后的时间
        auto lastTime = std::chrono::high_resolution_clock().now();
    }

}

//检测线程池的运行状态
bool ThreadPool::checkRunningState() const {

    return isPoolRunning_;
}

int Thread::generatedId_ = 0;

//获取线程ID
int Thread::getId()const {
    return threadId_;
}

//线程构造函数
Thread::Thread(ThreadFunc func):func_(func),threadId_(generatedId_++) {

}

//线程析构函数
Thread::~Thread() {

}

//线程启动函数
void Thread::start() {

    //创建一个线程来执行线程函数
    std::thread t(func_,threadId_);
    //设置分离线程
    t.detach();
}

//构造函数
Task::Task():result_(nullptr) {

}

//将用户重写的run方法封装起来 方便Result类型的对象调用
void Task::exec() {
    if (result_ != nullptr) {
        result_->setVal(run());//这里发生多肽调用
    }
}

//给Result对象赋值
void Task::setResult(Result* res) {

    result_ = res;
}

//Result类的构造函数
Result::Result(std::shared_ptr<Task> task, bool isvalid ):isValid_(isvalid)
        ,task_(task)
{
    task_->setResult(this);
}

//用户调用这个方法获取task的返回值
Any Result::get() {

    if (!isValid_) {
        return "";
    }

    //task任务如果没有执行完成 会阻塞住用户的线程
    sem_.wait();
    return std::move(any_);
}

//setVal获取任务执行完的返回值
void Result::setVal(Any any) {

    //首先获取task的返回值
    this->any_ = std::move(any);

    //已经获取任务的返回值 增加信号量资源
    sem_.post();
}