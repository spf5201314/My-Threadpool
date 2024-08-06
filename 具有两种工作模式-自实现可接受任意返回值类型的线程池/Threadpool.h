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
//ģ���۵Ĵ��붼ֻ��д��ͷ�ļ���
//any���� ����������������
//�򵥵�������ԭ������atomic
class Any
{
public:

	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any & operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	//������캯���ܹ���Any�����������͵�����
	template<typename T>
	Any(T data) :  ptr(std::make_unique<Derive<T>>(data))
	{

	}
	//��Any�������������ȡ����
	template<typename T>
	T cast()
	{
		//�ӻ����ҵ�����ָ���������� ��������ȡ����data��Ա���� RTTI
		Derive<T>* pd = dynamic_cast<Derive<T>*>(ptr.get());
		if (pd == nullptr)
		{
			throw "type is unmatch";
		}
		return  pd->data;
	}
private:
	//��������
	class Base
	{
	public:	
		virtual ~Base() = default;
	};
	//����������
	template<typename T>
	class Derive :public Base
	{
	public:
		Derive(T data) :data(data) 
		{

		}
		T data;//��������
	private:
		
	};
	//����һ������ָ��

	std::unique_ptr<Base> ptr;
};

//ʵ��һ���ź�����
class Semaphore
{
public:
	Semaphore(int limit = 0)
		:resLimit(limit)
	{
	
	}
	~Semaphore() = default;
	void wait()
	{	//�ȴ��ź�������Դ��û��Դ��������ǰ�߳�
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

//Task���͵�ǰ������
class Task;

//ʵ�ֽ����ύ��������ɵķ���ֵ
class Result 
{
public:
	Result(std::shared_ptr<Task> task, bool isValid=true);
	
	~Result() = default;
	//����һ ��ô��ȡ����ķ���ֵ
	void setVal(Any any);
	//����� get �û��������������÷���ֵ 
	
	Any get();
	
private:
	Any any;
	Semaphore sem;
	std::shared_ptr<Task> task_;  //ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid;

};

enum class PoolMode {


	MODE_FIXED, //�̶��߳�����
	MODE_CACHED //�ɱ��߳���Ŀ
};

//////////////////////////////////////////
//����������
class Task
{

public:
	Task();
	~Task() = default;
	//�û��Զ��������������ͣ���Task�̳У�ʵ�������Զ��崦��
	void exec();

	void setResult(Result* result);
	

	virtual Any run() = 0;
	
private:
	Result* result; //Result ������������ڳ���Task ��������ָͨ��

};


//////////////////////////////
//�߳���
class Thread
{
public:
	//����һ������ֵΪ�� û�������������ͣ�
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc fun);
	~Thread();

	void start();
	int getId() const;
private:
	ThreadFunc Func;  //���̳߳���ĺ������а󶨽���
	static int generateId;  //���ñ�����߳�ID ��Ϊ���ø���һ���
	int threadId; //�����߳�Id
};
/*
example:
ThreadPool pool;
pool.start(4);
class myTask: public Task
{ public:
	void run()
	{//�̴߳���
	};

}
pool.submitTask(std::make_shared<myTask>());

*/


//�̳߳���
class ThreadPool
{
public:

	//����
	ThreadPool();

	//����
	~ThreadPool();

	//�����������������ֵ
	void setTaskQueMax(int taskQuemax);

	void setThreadMax(int threadQuemax);

	//���̳߳��ύ����
	Result submitTask(std::shared_ptr<Task> task);


	//�����̳߳�
	void start(int initThreadSize=4);

	//�����̳߳ع���ģʽ
	void setMode(PoolMode mode);
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	//�����̺߳���
	void threadFunc(int threadId);

	//�߳��б�
	//std::vector<std::unique_ptr<Thread>> threads;
	std::unordered_map<int, std::unique_ptr<Thread>> threads;

	//��ʼ�߳�����  size_t��һ���޷�������
	size_t InitThreadSize;

	//��ָ�� ������ʱ����������
	std::queue<std::shared_ptr<Task>> taskQue;

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
	bool  checkRuningState() const;
};

#endif