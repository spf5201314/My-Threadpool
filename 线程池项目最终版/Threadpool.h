
#ifndef ThREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include<thread>
#include<queue>
#include<vector>
#include<unordered_map>
#include<memory> //����ָ��
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<future>
//ģ���۵Ĵ��붼ֻ��д��ͷ�ļ���
//any���� ����������������
//�򵥵�������ԭ������atomic

const int TASK_MAX_THRSHHOLD = 20;
const int THREAD_MAX_THRSHHOLD = 5;
const int THREAD_MAX_FREE_TIME = 5;




enum class PoolMode {


	MODE_FIXED, //�̶��߳�����
	MODE_CACHED //�ɱ��߳���Ŀ
};

//////////////////////////////////////////
//����������


//////////////////////////////
//�߳���
class Thread
{
public:
	//����һ������ֵΪ�� û�������������ͣ�
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc fun)
		: Func(fun)
		, threadId(generateId++)
	{

	}
	~Thread() = default;

	void start()
	{
		//����һ���߳���ִ���̺߳��� //����������t�ͻ����� �������̺߳�����������
		std::thread t(Func, threadId);
		t.detach(); //�����̷߳���
	}
	int getId() const
	{
		return threadId;
	}
private:
	ThreadFunc Func;  //���̳߳���ĺ������а󶨽���
	static int generateId;  //���ñ�����߳�ID ��Ϊ���ø���һ���
	int threadId; //�����߳�Id
};
int Thread::generateId = 0;



//�̳߳���
class ThreadPool
{
public:

	//����
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


	//����
	~ThreadPool()
	{
		isPoolRuning = false;


		//�����߳����������������߳��ڹ�����
		std::unique_lock<std::mutex> lock(m_mutex);
		/***************************************************/
		notEmpty.notify_all();//����������Ȼ�ܹ�����
		//�����������Զ��������ȥ�������Ƿ����㣬���뻽�ѲŻ�ȥ�������Ƿ����㣬�������Ż�����ִ�У���Ȼ��Ȼ������
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
	

	//�����������������ֵ
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

	//���̳߳��ύ����
	//pool.submitTask(std::make_shared<myTask>(1, 2));
	template<typename Func,typename ...Args>
	//�����������۵�
	//�����ķ���ֵ��future  decltypeͨ�����ʽ���Ƶ�����
	auto submitTask(Func&& func, Args&& ...args) ->std::future< decltype(func(args...))>
	{
		//�������
		using Rtype = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<Rtype()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));//��ֵ���ñ����ù������ֱ������ֵ
		std::future<Rtype> result = task->get_future();
		
		//��ȡ��
		std::unique_lock<std::mutex> lock(m_mutex);


		//�ź��� �п��в��ܷ���
		/*while (taskSize == taskQueMax)
		{
			notFull.wait(lock);
		}*/
		if (!notFull.wait_for(lock, std::chrono::seconds(1), [&]() {return taskQue.size() < (size_t)taskQueMax; }))
		{
			//�������ֵ��false ��ʾ1s��������Ȼû������
			std::cerr << "task queue is full, submit task fail" << std::endl;

			//task->getResult()����  taskִ����֮��ͱ�������������task��getResultҲֱ����ʧ 
			//�����������������������
			auto task = std::make_shared<std::packaged_task<Rtype()>>(
				[]() {return Rtype(); });
			//�ύʧ�� ���ص�ǰ���͵�0ֵ
			(*task)();
			return task->get_future();
			//get_future �����������ǵõ��˷���ֵ
		}
		//�п��࣬������Ž�ȥ
		//taskQue.emplace(task);
		taskQue.emplace(
			[task]()  {
			//ִ������
			(*task)();
			}
		);
		taskSize++;


		//�������ˣ�֪ͨ������

		notEmpty.notify_all();
		// cached�ʺ���ҪС���������  
		// ------------------------------------�������߳�
		if (poolMode == PoolMode::MODE_CACHED
			&& taskSize > freeSize
			&& curThreadSize < threadMax)
		{
			std::cout << "creat new thread...." << std::endl;
			//�����߶���ȥ���̳߳ص������ߺ���   Ŀ���ǽ������ߺ�����������
			std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));

			int Id = ptr->getId();
			threads.emplace(Id, std::move(ptr));
			threads[Id]->start();
			curThreadSize++;
			freeSize++;
		}
		// ��Ҫ�������������Ϳ����������̣߳��ж��Ƿ���Ҫ������ɾ���߳�
		//notEmpty.notify_all();

		return result;
	}

	


	//�����̳߳�
	void start(int initSize = 4)
	{
		//�����̳߳ص�����״̬
		isPoolRuning = true;
		//��¼��ʼ�̸߳���
		InitThreadSize = initSize;
		curThreadSize = initSize;
		for (int i = 0; i < initSize; ++i)
		{	//�����̶߳����ʱ�򣬰��̳߳������̺߳��������̶߳�������
			//Thread(std::bind(&ThreadPool::threadFunc, this));
			//�������洫�����Ĳ����Ϳ���
			//unique_ptr �����������ͨ�Ŀ�������͸�ֵ   ����һ��ռλ��
			std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,	std::placeholders::_1));

			threads.emplace(ptr->getId(), std::move(ptr));

		}
		//���������߳�   std::vector<Thread*> threads;
		for (int i = 0; i < initSize; ++i)
		{	//������
			threads[i]->start();
			freeSize++;


		}
	}

	//�����̳߳ع���ģʽ
	
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool & operator=(const ThreadPool&) = delete;
private:

	//�����̺߳���

	void threadFunc(int threadId)
	{
		auto lastTime = std::chrono::high_resolution_clock().now();
		//�������������ִ�����֮������ͷ�������Դ
		//while(isPoolRuning)
		for (;;)
		{
			Task task;
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


						if (std::cv_status::timeout == notEmpty.wait_for(lock, std::chrono::seconds(1)))//[&]() {return freeSize>InitThreadSize; }
						{	//�����߳�֧Ԯ

							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_MAX_FREE_TIME && curThreadSize > InitThreadSize)
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
				task();//ִ��function<void()>

				std::cout << "end thread" << std::this_thread::get_id() << std::endl;
			}

			freeSize++;
			lastTime = std::chrono::high_resolution_clock().now();  //ִ����������и���
		}




	}

	//�߳��б�
	//std::vector<std::unique_ptr<Thread>> threads;
	std::unordered_map<int, std::unique_ptr<Thread>> threads;

	//��ʼ�߳�����  size_t��һ���޷�������
	size_t InitThreadSize;

	//��ָ�� ������ʱ����������  Task��������������
	//share��Ϊ�˱���������������
	//	std::queue<std::shared_ptr<Task>> taskQue;
	using Task = std::function<void()>;  //��ŵ�����ͬ������ֵҲ��ͬ����ͬ�ϲ���һ��
	//���Բ�����Ϊ�գ��������ֵ������в���Ҫ��ȡ
	std::queue<Task> taskQue;

	//��ǰ�������� �������ԭ�Ӳ�������������
	std::atomic_int taskSize;

	std::atomic_int freeSize;

	//�����������
	int taskQueMax;
	//�߳���������
	int threadMax;
	//��ǰ�߳�����
	std::atomic_int curThreadSize;
	//�̲߳�����
	std::mutex m_mutex;

	//����
	std::condition_variable notFull;

	//����
	std::condition_variable notEmpty;

	std::condition_variable exitCond; //�ȴ���Դ����
	//��ǰ�̳߳صĹ���ģʽ
	PoolMode poolMode;

private:
	//��ǰ�̵߳�����״̬
	std::atomic_bool isPoolRuning;
	//��⵱ǰpool������״̬
	bool  checkRuningState() const
	{
		return isPoolRuning;
	}
};

#endif