#include"Threadpool.h"
#include<functional>
#include<iostream>

//线程池构造   不要出现魔鬼数字
const int TASK_MAX_THRSHHOLD = 7;
const int THREAD_MAX_THRSHHOLD = 5;
const int THREAD_MAX_FREE_TIME = 5;


ThreadPool::ThreadPool()
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

//设置线程池工作模式
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRuningState())
	{
		return;
	}
	poolMode = mode;

}
//设置任务队列上仙阈值
void ThreadPool::setTaskQueMax(int max)
{
	if (checkRuningState())
	{
		return;
	}
	taskQueMax = max;
}

void ThreadPool::setThreadMax(int max)
{
	if (poolMode == PoolMode::MODE_CACHED)
	{
		if(checkRuningState())
		{
			return;
		}
		threadMax = max;
	}
}

//给线程池提交任务  生产者提交任务
Result ThreadPool::submitTask(std::shared_ptr<Task> task)
{
	//获取锁
	std::unique_lock<std::mutex> lock(m_mutex);


	//信号量 有空闲才能放入
	/*while (taskSize == taskQueMax)
	{
		notFull.wait(lock);
	}*/
	if (!notFull.wait_for(lock, std::chrono::seconds(1), [&]() {return taskQue.size() < (size_t) taskQueMax; }))
	{
		//如果返回值是false 表示1s后，条件仍然没有满足
		std::cerr << "task queue is full, submit task fail" << std:: endl;

		//task->getResult()不可  task执行完之后就被析构，依赖于task的getResult也直接消失 
		//搞清这两个对象的生存周期
		return Result(task, false);
	}
	//有空余，把任务放进去
	taskQue.emplace(task);
	taskSize++;


	// cached适合需要小而快的任务  
	// ------------------------------------管理者线程
	if (poolMode == PoolMode::MODE_CACHED
		&&taskSize>freeSize
		&&curThreadSize<threadMax)
	{
		std::cout<<"creat new thread...." << std::endl;
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));

		int Id = ptr->getId();
		threads.emplace(Id, std::move(ptr));
		threads[Id]->start();
		curThreadSize++;
		freeSize++;
	}
	// 需要根据任务数量和空闲数量的线程，判断是否需要创建或删除线程


	//有任务了，通知消费者

	notEmpty.notify_all();
	return Result(task);

}




//消费者消费任务
void ThreadPool::start(int initSize)//现成函数返回，线程结束
{	//设置线程池的运行状态
	isPoolRuning = true;
	//记录初始线程个数
	InitThreadSize = initSize;
	curThreadSize=initSize;
	for (int i = 0; i < initSize; ++i)
	{	//创建线程对象的时候，把线程池里面线程函数给到线程对象里面
		//Thread(std::bind(&ThreadPool::threadFunc, this));
		//括号里面传入对象的参数就可以
		//unique_ptr 不允许进行普通的拷贝构造和赋值   留下一个占位符
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,
										std::placeholders::_1));
		
		threads.emplace(ptr->getId(), std::move(ptr));

	}
	//启动所有线程   std::vector<Thread*> threads;
	for (int i = 0; i < initSize; ++i)
	{	//消费者
		threads[i]->start();
		freeSize++;
		

	}
	
}

//定义线程函数 消费者消费任务
void ThreadPool::threadFunc(int threadId)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	//等所有任务必须执行完成之后才能释放所有资源
	//while(isPoolRuning)
	for(;;)
	{
		std::shared_ptr<Task> task;
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
				
				
					if (std::cv_status::timeout== notEmpty.wait_for(lock, std::chrono::seconds(1)))//[&]() {return freeSize>InitThreadSize; }
					{	//回收线程支援
						
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_FREE_TIME&&curThreadSize>InitThreadSize)
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
			task->exec();

			std::cout << "end thread" << std::this_thread::get_id() << std::endl;
		}
		
		freeSize++;
		lastTime = std::chrono::high_resolution_clock().now();  //执行完任务进行更新
	}
	
	
		
	
	
}
bool ThreadPool::checkRuningState() const
{
	return isPoolRuning;
};

ThreadPool::~ThreadPool()
{
	isPoolRuning = false;
	

	//任务线程在阻塞或者任务线程在工作中
	std::unique_lock<std::mutex> lock(m_mutex);
	/***************************************************/
	notEmpty.notify_all();//后抢到锁仍然能够唤醒
	//这里的阻塞永远不是自主去看条件是否满足，必须唤醒才会去看条件是否满足，如果满足才会向下执行，不然任然会阻塞
	exitCond.wait(lock, [&]() {return threads.size() == 0; });

}
/// <summary>
/// /////////////////////////////////////////
/// </summary>
Task::Task()
	:result(nullptr)
{

}
void Task::exec()
{
	if (result != nullptr)
	{
		result->setVal(run());
	}  //这里发生多态


}
void Task::setResult(Result* res)
{
	result = res; 
}


/// <summary>
/// /////////////////////////////////////////
/// </summary>
Result::Result(std::shared_ptr<Task> task, bool isValid)
:task_(task)
, isValid(isValid)
{
	task_->setResult(this);
}


Any Result::get()
{	//无效直接返回，有效继续等待
	if (!isValid)
	{
		return"";
	}
	//task任务如果美誉执行完 ，这会阻塞用户的线程
	sem.wait();
	return std::move(any);


}
void Result::setVal(Any any)
{
	//存储task的返回值
	this->any = std::move(any);
	//已经获取了返回值
	sem.post();

}
/// <summary>
/// /////////////////////////////////////////
/// </summary>

//////////////////////////////////////////
/*--------------线程方法实现------------------------------*/
//线程构造
Thread::Thread(ThreadFunc fun)
	: Func(fun)
	, threadId(generateId++)
{  
	
}
int Thread::generateId = 0;
//线程析构
Thread::~Thread()
{

}
int Thread::getId() const
{
	return threadId;
}
//启动线程
void Thread::start()
{
	//创建一个线程来执行线程函数 //除了作用域t就会析构 ，但是线程函数不能析构
	std::thread t(Func,threadId);
	t.detach(); //设置线程分离

}

