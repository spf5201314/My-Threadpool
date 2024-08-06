#ifndef ThREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include<thread>
#include<queue>
#include<vector>
#include<unordered_map>
#include<memory> //智能指针
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
//模版累的代码都只能写在头文件中
//any类型 接受任意数据类型
//简单的数据用原子类型atomic
class Any
{
public:

	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any & operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	//这个构造函数能够让Any接受任意类型的数据
	template<typename T>
	Any(T data) :  ptr(std::make_unique<Derive<T>>(data))
	{

	}
	//把Any对象里的数据提取出来
	template<typename T>
	T cast()
	{
		//从基类找到它所指向的子类对象 从里面提取出来data成员变量 RTTI
		Derive<T>* pd = dynamic_cast<Derive<T>*>(ptr.get());
		if (pd == nullptr)
		{
			throw "type is unmatch";
		}
		return  pd->data;
	}
private:
	//基类类型
	class Base
	{
	public:	
		virtual ~Base() = default;
	};
	//派生类类型
	template<typename T>
	class Derive :public Base
	{
	public:
		Derive(T data) :data(data) 
		{

		}
		T data;//保存数据
	private:
		
	};
	//定义一个基类指针

	std::unique_ptr<Base> ptr;
};

//实现一个信号量类
class Semaphore
{
public:
	Semaphore(int limit = 0)
		:resLimit(limit)
	{
	
	}
	~Semaphore() = default;
	void wait()
	{	//等待信号量有资源，没资源就阻塞当前线程
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cond.wait(lock, [&]() {return resLimit > 0; });
		resLimit--;

	}
	void post()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		resLimit++;
		m_cond.notify_all();
	}
private:
	std::mutex m_mutex;
	std::condition_variable m_cond;
	int resLimit;

};

//Task类型的前置声明
class Task;

//实现接受提交的任务完成的返回值
class Result 
{
public:
	Result(std::shared_ptr<Task> task, bool isValid=true);
	
	~Result() = default;
	//问题一 怎么获取任务的返回值
	void setVal(Any any);
	//问题二 get 用户调用这个方法获得返回值 
	
	Any get();
	
private:
	Any any;
	Semaphore sem;
	std::shared_ptr<Task> task_;  //指向对应获取返回值的任务对象
	std::atomic_bool isValid;

};

enum class PoolMode {


	MODE_FIXED, //固定线程数量
	MODE_CACHED //可变线程数目
};

//////////////////////////////////////////
//任务抽象基类
class Task
{

public:
	Task();
	~Task() = default;
	//用户自定义任意数据类型，从Task继承，实现任务自定义处理
	void exec();

	void setResult(Result* result);
	

	virtual Any run() = 0;
	
private:
	Result* result; //Result 对象的生命周期长于Task 所以用普通指针

};


//////////////////////////////
//线程类
class Thread
{
public:
	//定义一个返回值为空 没参数的数据类型，
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc fun);
	~Thread();

	void start();
	int getId() const;
private:
	ThreadFunc Func;  //与线程池类的函数进行绑定接收
	static int generateId;  //不用本身的线程ID 因为不好复制一大窜
	int threadId; //保存线程Id
};
/*
example:
ThreadPool pool;
pool.start(4);
class myTask: public Task
{ public:
	void run()
	{//线程代码
	};

}
pool.submitTask(std::make_shared<myTask>());

*/


//线程池类
class ThreadPool
{
public:

	//构造
	ThreadPool();

	//析构
	~ThreadPool();

	//设置任务队列上仙阈值
	void setTaskQueMax(int taskQuemax);

	void setThreadMax(int threadQuemax);

	//给线程池提交任务
	Result submitTask(std::shared_ptr<Task> task);


	//开启线程池
	void start(int initThreadSize=4);

	//设置线程池工作模式
	void setMode(PoolMode mode);
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	//定义线程函数
	void threadFunc(int threadId);

	//线程列表
	//std::vector<std::unique_ptr<Thread>> threads;
	std::unordered_map<int, std::unique_ptr<Thread>> threads;

	//初始线程数量  size_t是一个无符号整型
	size_t InitThreadSize;

	//裸指针 对于临时任务发生问题
	std::queue<std::shared_ptr<Task>> taskQue;

	//当前任务数量 定义的是原子操作，而不用锁
	std::atomic_int taskSize;

	std::atomic_int freeSize;

	//任务队列上限
	int taskQueMax;
	//线程数量上限
	int threadMax;
	//当前线程数量
	std::atomic_int curThreadSize;
	//线程操作锁
	std::mutex m_mutex;

	//不空
	std::condition_variable notFull;

	//不满
	std::condition_variable notEmpty;

	std::condition_variable exitCond; //等待资源回收
	//当前线程池的工作模式
	PoolMode poolMode;

private:
	//当前线程的启动状态
	std::atomic_bool isPoolRuning;
	//检测当前pool的运行状态
	bool  checkRuningState() const;
};

#endif