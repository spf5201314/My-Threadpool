#include<iostream>
#include"Threadpool.h"
#include<chrono>





class myTask : public Task
{public:
	myTask(int begin, int end)
		:begin(begin)
		,end(end)
	{
		
	}
	Any run()  //run方法最终在在线程分配池分配的线程去执行
	{
		std::cout << "tid: " << std::this_thread::get_id() 
			<< " begin " << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		int num = 0;
		for (int i = begin; i <=end; ++i)
		{
			num = num + i;
		}

		std::cout << "tid: " << std::this_thread::get_id() 
			<< " end" << std::endl;
		return num;
	}
private:
	int begin;
	int end;
};
int main()
{ 
	
		ThreadPool pool;
		//pool.setMode(PoolMode::MODE_CACHED);

		//所有设置好之后，在开始运行
		pool.start(4);

		Result res = pool.submitTask(std::make_shared<myTask>(1, 2));
		Result res1 = pool.submitTask(std::make_shared<myTask>(3, 4));
		pool.submitTask(std::make_shared<myTask>(1, 2));
		pool.submitTask(std::make_shared<myTask>(3, 4));

		pool.submitTask(std::make_shared<myTask>(1, 2));
		pool.submitTask(std::make_shared<myTask>(3, 4));
		

		
		//int sum1 = res1.get().cast<int>();
		int sum = res.get().cast<int>();

		//Master-Slave 线程任务模型  Master用来分配任务，给Slave线程分配任务
		//Master任务合并各个任务
		std::cout << "结果为 " << sum << std::endl;
		
	
	

}
