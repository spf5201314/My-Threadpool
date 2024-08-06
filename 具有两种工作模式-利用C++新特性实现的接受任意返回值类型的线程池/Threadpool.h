
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
#include<future>
//模版累的代码都只能写在头文件中
//any类型 接受任意数据类型
//简单的数据用原子类型atomic

const int TASK_MAX_THRSHHOLD = 20;
const int THREAD_MAX_THRSHHOLD = 5;
const int THREAD_MAX_FREE_TIME = 5;




enum class PoolMode {


	MODE_FIXED, //固定线程数量
	MODE_CACHED //可变线程数目
};

//////////////////////////////////////////
//任务抽象基类


//////////////////////////////
//线程类
class Thread
{
public:
	//定义一个返回值为空 没参数的数据类型，
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc fun)
		: Func(fun)
		, threadId(generateId++)
	{

	}
	~Thread() = default;

	void start()
	{
		//创建一个线程来执行线程函数 //除了作用域t就会析构 ，但是线程函数不能析构
		std::thread t(Func, threadId);
		t.detach(); //设置线程分离
	}
	int getId() const
	{
		return threadId;
	}
private:
	ThreadFunc Func;  //与线程池类的函数进行绑定接收
	static int generateId;  //不用本身的线程ID 因为不好复制一大窜
	int threadId; //保存线程Id
};
int Thread::generateId = 0;



//线程池类
class ThreadPool
{
public:

	//构造
	ThreadPool()
		: InitThreadSize(0)
		, taskSize(0)
		, taskQueMax(TASK_MAX_THRSHHOLD)
		, poolMode(PoolMode::MODE_FIXED)
		, isPoolRuning(false)
		, freeSize(0)
		, threadMax(THREAD_MAX_THRSHHOLD)
		, curThreadSize(0)

	{
	}


	//析构
	~ThreadPool()
	{
		isPoolRuning = false;


		//任务线程在阻塞或者任务线程在工作中
		std::unique_lock<std::mutex> lock(m_mutex);
		/***************************************************/
		notEmpty.notify_all();//后抢到锁仍然能够唤醒
		//这里的阻塞永远不是自主去看条件是否满足，必须唤醒才会去看条件是否满足，如果满足才会向下执行，不然任然会阻塞
		exitCond.wait(lock, [&]() {return threads.size() == 0; });

	}

	void setMode(PoolMode mode)
	{
		if (poolMode == PoolMode::MODE_CACHED)
		{
			if (checkRuningState())
			{
				return;
			}
			poolMode = mode;
		}
	}
	

	//设置任务队列上仙阈值
	void setTaskQueMax(int max)
	{
		if (checkRuningState())
		{
			return;
		}
		taskQueMax = max;
	}

	void setThreadMax(int max)
	{
		if (checkRuningState())
		{
			return;
		}
		if (poolMode == PoolMode::MODE_CACHED)
		{
			
			threadMax = max;
		}
	}

	//给线程池提交任务
	//pool.submitTask(std::make_shared<myTask>(1, 2));
	template<typename Func,typename ...Args>
	//这里有引用折叠
	//函数的返回值是future  decltype通过表达式来推导类型
	auto submitTask(Func&& func, Args&& ...args) ->std::future< decltype(func(args...))>
	{
		//打包任务
		using Rtype = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<Rtype()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));//右值引用变量拿过来用又变成了左值
		std::future<Rtype> result = task->get_future();
		
		//获取锁
		std::unique_lock<std::mutex> lock(m_mutex);


		//信号量 有空闲才能放入
		/*while (taskSize == taskQueMax)
		{
			notFull.wait(lock);
		}*/
		if (!notFull.wait_for(lock, std::chrono::seconds(1), [&]() {return taskQue.size() < (size_t)taskQueMax; }))
		{
			//如果返回值是false 表示1s后，条件仍然没有满足
			std::cerr << "task queue is full, submit task fail" << std::endl;

			//task->getResult()不可  task执行完之后就被析构，依赖于task的getResult也直接消失 
			//搞清这两个对象的生存周期
			auto task = std::make_shared<std::packaged_task<Rtype()>>(
				[]() {return Rtype(); });
			//提交失败 返回当前类型的0值
			(*task)();
			return task->get_future();
			//get_future 会阻塞，除非得到了返回值
		}
		//有空余，把任务放进去
		//taskQue.emplace(task);
		taskQue.emplace(
			[task]()  {
			//执行任务
			(*task)();
			}
		);
		taskSize++;


		//有任务了，通知消费者

		notEmpty.notify_all();
		// cached适合需要小而快的任务  
		// ------------------------------------管理者线程
		if (poolMode == PoolMode::MODE_CACHED
			&& taskSize > freeSize
			&& curThreadSize < threadMax)
		{
			std::cout << "creat new thread...." << std::endl;
			//消费者对象去绑定线程池的消费者函数   目的是将消费者函数保存起来
			std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));

			int Id = ptr->getId();
			threads.emplace(Id, std::move(ptr));
			threads[Id]->start();
			curThreadSize++;
			freeSize++;
		}
		// 需要根据任务数量和空闲数量的线程，判断是否需要创建或删除线程
		//notEmpty.notify_all();

		return result;
	}

	


	//开启线程池
	void start(int initSize = 4)
	{
		//设置线程池的运行状态
		isPoolRuning = true;
		//记录初始线程个数
		InitThreadSize = initSize;
		curThreadSize = initSize;
		for (int i = 0; i < initSize; ++i)
		{	//创建线程对象的时候，把线程池里面线程函数给到线程对象里面
			//Thread(std::bind(&ThreadPool::threadFunc, this));
			//括号里面传入对象的参数就可以
			//unique_ptr 不允许进行普通的拷贝构造和赋值   留下一个占位符
			std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,	std::placeholders::_1));

			threads.emplace(ptr->getId(), std::move(ptr));

		}
		//启动所有线程   std::vector<Thread*> threads;
		for (int i = 0; i < initSize; ++i)
		{	//消费者
			threads[i]->start();
			freeSize++;


		}
	}

	//设置线程池工作模式
	
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool & operator=(const ThreadPool&) = delete;
private:

	//定义线程函数

	void threadFunc(int threadId)
	{
		auto lastTime = std::chrono::high_resolution_clock().now();
		//等所有任务必须执行完成之后才能释放所有资源
		//while(isPoolRuning)
		for (;;)
		{
			Task task;
			{//获取锁
				//任务线程可能在这里 同时任务队列为空
				std::unique_lock<std::mutex> lock(m_mutex);

				std::cout << "tid: " << std::this_thread::get_id()
					<< " 尝试获取任务..." << std::endl;
				//cached 下多余的线程大于初始Initsize的线程等待60s后要进行回收
				//锁加双重判断
				while (taskQue.size() == 0)
				{
					if (!isPoolRuning)
					{
						threads.erase(threadId);
						std::cout << "threadid " << std::this_thread::get_id() << " is exit" << std::endl;
						exitCond.notify_all();
						return;  //线程函数结束 ，线程回收
					}

					if (poolMode == PoolMode::MODE_CACHED)
					{


						if (std::cv_status::timeout == notEmpty.wait_for(lock, std::chrono::seconds(1)))//[&]() {return freeSize>InitThreadSize; }
						{	//回收线程支援

							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_MAX_FREE_TIME && curThreadSize > InitThreadSize)
							{
								//回收线程
								//记录线程数目减少，线程对象从线程容器中删除 删除threadfun对应的线程对象

								threads.erase(threadId);
								freeSize++;
								curThreadSize--;
								std::cout << "threadid " << std::this_thread::get_id() << " is exit" << std::endl;
								return;

							}

						}

					}
					else
					{
						//notEmpty.wait(lock, [&]() {return taskQue.size() > 0; });
						notEmpty.wait(lock);

					}
					/*if (!isPoolRuning)
					{
						threads.erase(threadId);
						exitCond.notify_all();
						std::cout << "threadid " << std::this_thread::get_id() << " is exit" << std::endl;
						return;
					}*/
				}
				//等待empty

				freeSize--;

				std::cout << "tid: " << std::this_thread::get_id()
					<< " 获取任务成功" << std::endl;
				//去一个任务
				task = taskQue.front();
				taskQue.pop();
				taskSize--;

				//取出任务要通知
				if (taskQue.size() > 0)
				{
					notEmpty.notify_all();
				}
				//生产者可以生产任务
				notFull.notify_all();


			}

			//线程执行任务
			if (task != nullptr)
			{

				std::cout << "thread " << std::this_thread::get_id() << " is working " << std::endl;
				//执行任务 把任务的返回值setVal方法给到Result
				task();//执行function<void()>

				std::cout << "end thread" << std::this_thread::get_id() << std::endl;
			}

			freeSize++;
			lastTime = std::chrono::high_resolution_clock().now();  //执行完任务进行更新
		}




	}

	//线程列表
	//std::vector<std::unique_ptr<Thread>> threads;
	std::unordered_map<int, std::unique_ptr<Thread>> threads;

	//初始线程数量  size_t是一个无符号整型
	size_t InitThreadSize;

	//裸指针 对于临时任务发生问题  Task任务就是任务对象
	//share是为了保存它的生命周期
	//	std::queue<std::shared_ptr<Task>> taskQue;
	using Task = std::function<void()>;  //存放的任务不同，返回值也不同，不同合并成一类
	//所以不如作为空，这个返回值任务队列不需要读取
	std::queue<Task> taskQue;

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
	bool  checkRuningState() const
	{
		return isPoolRuning;
	}
};

#endif