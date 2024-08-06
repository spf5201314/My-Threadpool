#include<iostream>
#include<functional>
#include<thread>
#include<future>
#include"Threadpool.h"
using namespace std;


//对任务参数使用可变参数模版

//使用future代替result 
int sum1(int a, int b)
{
	this_thread::sleep_for(std::chrono::seconds(2));
	return a + b;
}
int sum2(int a, int b,int c)
{
	this_thread::sleep_for(std::chrono::seconds(2));
	return a + b+c;
}
int main()
{	/*return int();*/
	ThreadPool pool;
	pool.setMode(PoolMode::MODE_CACHED);
	pool.start(2);
	future<int> res = pool.submitTask(sum2, 1, 2,3);
	future<int> res1 = pool.submitTask([](int a, int b) {
		int sum = 0;
		for (int i = a; i <= b; ++i)
		{
			sum = sum + i;
		}
		return sum;
		}, 1, 100);
	
	future<int> res4 = pool.submitTask(sum1, 1, 2);
	
	cout << res1.get() << endl;
	
	

}