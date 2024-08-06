#include<iostream>
#include"Threadpool.h"
#include<chrono>


/*��ʱ����Ҫ�̵߳ķ���ֵ*/

/*����һ ��ô���run�����ķ���ֵ�� ���Ա�ʾ���������
* java Python Object ������������Ļ���
* C++17  Any����

*/
class myTask : public Task
{public:
	myTask(int begin, int end)
		:begin(begin)
		,end(end)
	{
		
	}
	Any run()  //run�������������̷߳���ط�����߳�ȥִ��
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
{ //��ӡ�̵߳��߳�վ ����˼��ֹͣ������
	//���������Ժ� ������ô����
	
		ThreadPool pool;
		//pool.setMode(PoolMode::MODE_CACHED);

		//�������ú�֮���ڿ�ʼ����
		pool.start(4);

		Result res = pool.submitTask(std::make_shared<myTask>(1, 2));
		Result res1 = pool.submitTask(std::make_shared<myTask>(3, 4));
		pool.submitTask(std::make_shared<myTask>(1, 2));
		pool.submitTask(std::make_shared<myTask>(3, 4));

		pool.submitTask(std::make_shared<myTask>(1, 2));
		pool.submitTask(std::make_shared<myTask>(3, 4));
		

		
		//int sum1 = res1.get().cast<int>();
		int sum = res.get().cast<int>();

		//Master-Slave �߳�����ģ��  Master�����������񣬸�Slave�̷߳�������
		//Master����ϲ���������
		std::cout << "���Ϊ " << sum << std::endl;
		
	
	

}