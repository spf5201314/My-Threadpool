#include"Threadpool.h"
#include<functional>
#include<iostream>

//�̳߳ع���   ��Ҫ����ħ������
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

//�����̳߳ع���ģʽ
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRuningState())
	{
		return;
	}
	poolMode = mode;

}
//�����������������ֵ
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

//���̳߳��ύ����  �������ύ����
Result ThreadPool::submitTask(std::shared_ptr<Task> task)
{
	//��ȡ��
	std::unique_lock<std::mutex> lock(m_mutex);


	//�ź��� �п��в��ܷ���
	/*while (taskSize == taskQueMax)
	{
		notFull.wait(lock);
	}*/
	if (!notFull.wait_for(lock, std::chrono::seconds(1), [&]() {return taskQue.size() < (size_t) taskQueMax; }))
	{
		//�������ֵ��false ��ʾ1s��������Ȼû������
		std::cerr << "task queue is full, submit task fail" << std:: endl;

		//task->getResult()����  taskִ����֮��ͱ�������������task��getResultҲֱ����ʧ 
		//�����������������������
		return Result(task, false);
	}
	//�п��࣬������Ž�ȥ
	taskQue.emplace(task);
	taskSize++;


	// cached�ʺ���ҪС���������  
	// ------------------------------------�������߳�
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
	// ��Ҫ�������������Ϳ����������̣߳��ж��Ƿ���Ҫ������ɾ���߳�


	//�������ˣ�֪ͨ������

	notEmpty.notify_all();
	return Result(task);

}




//��������������
void ThreadPool::start(int initSize)//�ֳɺ������أ��߳̽���
{	//�����̳߳ص�����״̬
	isPoolRuning = true;
	//��¼��ʼ�̸߳���
	InitThreadSize = initSize;
	curThreadSize=initSize;
	for (int i = 0; i < initSize; ++i)
	{	//�����̶߳����ʱ�򣬰��̳߳������̺߳��������̶߳�������
		//Thread(std::bind(&ThreadPool::threadFunc, this));
		//�������洫�����Ĳ����Ϳ���
		//unique_ptr �����������ͨ�Ŀ�������͸�ֵ   ����һ��ռλ��
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,
										std::placeholders::_1));
		
		threads.emplace(ptr->getId(), std::move(ptr));

	}
	//���������߳�   std::vector<Thread*> threads;
	for (int i = 0; i < initSize; ++i)
	{	//������
		threads[i]->start();
		freeSize++;
		

	}
	
}

//�����̺߳��� ��������������
void ThreadPool::threadFunc(int threadId)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	//�������������ִ�����֮������ͷ�������Դ
	//while(isPoolRuning)
	for(;;)
	{
		std::shared_ptr<Task> task;
		{//��ȡ��
			//�����߳̿��������� ͬʱ�������Ϊ��
			std::unique_lock<std::mutex> lock(m_mutex);

			std::cout << "tid: " << std::this_thread::get_id()
				<< " ���Ի�ȡ����..." << std::endl;
			//cached �¶�����̴߳��ڳ�ʼInitsize���̵߳ȴ�60s��Ҫ���л���
			//����˫���ж�
			while (taskQue.size() == 0)
			{
				if (!isPoolRuning)
				{
					threads.erase(threadId);
					std::cout << "threadid " << std::this_thread::get_id() << " is exit" << std::endl;
					exitCond.notify_all();
					return;  //�̺߳������� ���̻߳���
				}

				if (poolMode == PoolMode::MODE_CACHED)
				{
				
				
					if (std::cv_status::timeout== notEmpty.wait_for(lock, std::chrono::seconds(1)))//[&]() {return freeSize>InitThreadSize; }
					{	//�����߳�֧Ԯ
						
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_FREE_TIME&&curThreadSize>InitThreadSize)
						{
							//�����߳�
							//��¼�߳���Ŀ���٣��̶߳�����߳�������ɾ�� ɾ��threadfun��Ӧ���̶߳���
							
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
			//�ȴ�empty
								
				freeSize--;

				std::cout << "tid: " << std::this_thread::get_id()
					<< " ��ȡ����ɹ�" << std::endl;
				//ȥһ������
				task = taskQue.front();
				taskQue.pop();
				taskSize--;

				//ȡ������Ҫ֪ͨ
				if (taskQue.size() > 0)
				{
					notEmpty.notify_all();
				}
				//�����߿�����������
				notFull.notify_all();
			
			
		}

		//�߳�ִ������
		if (task != nullptr)
		{
			
			std::cout << "thread " << std::this_thread::get_id() << " is working " << std::endl;
			//ִ������ ������ķ���ֵsetVal��������Result
			task->exec();

			std::cout << "end thread" << std::this_thread::get_id() << std::endl;
		}
		
		freeSize++;
		lastTime = std::chrono::high_resolution_clock().now();  //ִ����������и���
	}
	
	
		
	
	
}
bool ThreadPool::checkRuningState() const
{
	return isPoolRuning;
};

ThreadPool::~ThreadPool()
{
	isPoolRuning = false;
	

	//�����߳����������������߳��ڹ�����
	std::unique_lock<std::mutex> lock(m_mutex);
	/***************************************************/
	notEmpty.notify_all();//����������Ȼ�ܹ�����
	//�����������Զ��������ȥ�������Ƿ����㣬���뻽�ѲŻ�ȥ�������Ƿ����㣬�������Ż�����ִ�У���Ȼ��Ȼ������
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
	}  //���﷢����̬


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
{	//��Чֱ�ӷ��أ���Ч�����ȴ�
	if (!isValid)
	{
		return"";
	}
	//task�����������ִ���� ����������û����߳�
	sem.wait();
	return std::move(any);


}
void Result::setVal(Any any)
{
	//�洢task�ķ���ֵ
	this->any = std::move(any);
	//�Ѿ���ȡ�˷���ֵ
	sem.post();

}
/// <summary>
/// /////////////////////////////////////////
/// </summary>

//////////////////////////////////////////
/*--------------�̷߳���ʵ��------------------------------*/
//�̹߳���
Thread::Thread(ThreadFunc fun)
	: Func(fun)
	, threadId(generateId++)
{  
	
}
int Thread::generateId = 0;
//�߳�����
Thread::~Thread()
{

}
int Thread::getId() const
{
	return threadId;
}
//�����߳�
void Thread::start()
{
	//����һ���߳���ִ���̺߳��� //����������t�ͻ����� �������̺߳�����������
	std::thread t(Func,threadId);
	t.detach(); //�����̷߳���

}

