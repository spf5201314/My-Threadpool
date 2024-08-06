// MysqlConnectionPool.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "MysqlConnectionPool.h"
#include "public.h"
#include "Connection.h"
#include<thread>
#include<functional>
//线程安全的懒汉单例函数接口
ConnectionPool* ConnectionPool::getConnectionPool()
{
	static ConnectionPool pool;//静态局部变量初始化 编译器自动进行lock和unlock
	return &pool;
}

//从配置文件中加载配置项
bool ConnectionPool::loadConfigFile()
{
	FILE* pf = fopen("mysql.ini", "r");
	if (pf == nullptr)
	{
		LOG("mysql.ini file is not exis");
		return false;
	}
	while (!feof(pf))
	{
		char line[1024] = { 0 };
		fgets(line, 1024, pf);
		string str = line;
		int idx = str.find("=", 0);
		if (idx == -1)
		{
			continue;
		}
		int endidx = str.find("\n", idx);
		string key = str.substr(0, idx);
		string value = str.substr(idx + 1, endidx - 1 - idx);
		if (key == "ip")
		{
			_ip = value;
		}
		else if (key=="port")
		{
			_port = atoi(value.c_str());
		}
		else if (key == "username")
		{
			_username = value;
		}
		else if (key =="password")
		{
			_password = value;
		}
		else if (key == "dbname")
		{
			_dbname = value;
		}
		else if (key == "initSize")
		{
			_initSize = atoi(value.c_str());
		}
		else if (key == "maxSize")
		{
			_maxSize = atoi(value.c_str());
		}
		else if (key == "maxIdleTime")
		{
			_maxIdleTime = atoi(value.c_str());
		}
		else if (key == "connTimeout")
		{
			_connTimeout = atoi(value.c_str());
		}
	}
	return true;
}


//连接池的构造函数
ConnectionPool::ConnectionPool()
{
	isStart = true;
	//加载配置项
	if (!loadConfigFile())
	{
		return;
	}
	for (int i = 0; i < _initSize; ++i)
	{

		Connection* ptr = new Connection();
		
		if (ptr->connect(_ip, _port, _username, _password, _dbname))
		{
			ptr->refreshAlivetime(); //刷新每个空闲的起始时间
			_connQue.push(ptr);
			_curConnNum++;
		}
		else
		{
			LOG("用户信息错误, 连接失败");
			return;
		}
		
	}
	//启动一个新的线程  作为连接的生产者
	thread produce(std::bind(&ConnectionPool::proConnTasker,this));
	produce.detach();

	//启动一个新的线程  查看是否需要删除连接
	thread scanner(std::bind(&ConnectionPool::scannerConnTasker, this));
	scanner.detach();
	

}

ConnectionPool::~ConnectionPool()
{
	cout << "我要析构了" << endl;
	unique_lock<mutex> lock(_queueMutex);
	isStart = false;
	cv.notify_all();
	isend.wait(lock, [&]() {return _connQue.size() == 0; });
}
//在池里独立的生产者
void ConnectionPool::proConnTasker()
{
	for (;;)
	{
		unique_lock<mutex> lock( _queueMutex);
		while (!_connQue.empty())
		{
			cv.wait(lock);
		}
		if (isStart)
			//连接数量没有到达最大上限
		{
			if (_curConnNum < _maxSize)
			{
				for (int i = 0; i < 3; i++)
				{
					if (_curConnNum < _maxSize)
					{
						Connection* ptr = new Connection();

						ptr->connect(_ip, _port, _username, _password, _dbname);
						ptr->refreshAlivetime(); //刷新每个空闲的起始时间
						_connQue.push(ptr);
						_curConnNum++;
						
						//cout << "正在生产，有" << _curConnNum << " 个连接" << endl;
					}
				}
				
				cv.notify_all();
			}
			
		}
		
		
	}
}

//扫描超过最大时间的空闲连接
void ConnectionPool::scannerConnTasker()
{
	for (;;)
	{
		this_thread::sleep_for(chrono::milliseconds(_maxIdleTime));
		unique_lock<mutex> lock(_queueMutex);
		if (isStart)
		{
			while (_curConnNum > _initSize)
			{
				Connection* p = _connQue.front();
				if (p->getAlivetime() > (_maxIdleTime * 1000))
				{
					_connQue.pop();
					_curConnNum--;

					delete p; //调用连接的析构函数
					cout << "我被删除了" << "还有 " << _curConnNum << " 个连接" << endl;
					isend.notify_all();
					cout << 5 << endl;
					

				}
				else
				{
					break; //对头的连接没有超时 ，因为是队列后面的时间一定比前面的短
				}
			}
		}
		else
		{
			while(_curConnNum!=0)
			{
				Connection* sp = _connQue.front();
				_connQue.pop();
				delete sp;
				_curConnNum--;
				cout << "因为析构我被删除了" << "还有 " << _curConnNum << " 个连接" << endl;
				isend.notify_all();
					cout << 3 << endl;
				
			}
			return;
		}
	}
}


shared_ptr<Connection> ConnectionPool::getConnection()
{
	unique_lock<mutex> lock(_queueMutex);
	//if (_connQue.empty()) 

	while (_connQue.empty()) 
	{
		cout << "我正在等待" << endl;
		if(wait.wait_for(lock, std::chrono::milliseconds(_connTimeout))==cv_status::timeout)
		{
			
			
			
				LOG("获取连接超时， 获取连接失败！");
				return nullptr;
			

		}
		if (!isStart)
		{
			cout << "因为析构我被删除了" << "还有 " << _curConnNum << " 个连接" << endl;
			isend.notify_all();
			
			return nullptr;
		}
		cout << "我被唤醒了" << endl;
	}
	/*重新定义智能指针的析构函数，不然会把链接给释放*/
	
	shared_ptr<Connection> sp(_connQue.front(),
		[&](Connection * pcon) {
			unique_lock<mutex> lock(_queueMutex);
			if (isStart)
			{
				if (_curConnNum < _maxSize)
				{
					
					_connQue.push(pcon);
					pcon->refreshAlivetime();
					_curConnNum++;
					
					cv.notify_all();
					cout << "现在有" << _curConnNum<< " 个连接" << endl;
					
				}
				else
				{
					delete pcon;
				}
			}
			else
			{
				delete pcon;
				_curConnNum--;
				cout << 1 << endl;
				cout << "因为析构我被删除了" << "还有 " << _curConnNum << " 个连接" << endl;
				isend.notify_all();
				return nullptr;
			}
		});
	_connQue.pop();
	_curConnNum--;
	
	if (_connQue.empty())
	{
		cv.notify_all(); //谁消费后一个conn  谁负责通知生产者去生产 
	}

	return sp;


}