#pragma once
/*实现连接池模块*/
#include<string>
#include<queue>
#include "Connection.h"
#include <mutex>
#include<condition_variable>
#include<atomic>
#include<memory>
#include<ctime>
using namespace std;

class ConnectionPool
{
public:
	//获取连接池对象的实例
	static ConnectionPool* getConnectionPool();
	//给外部提供接口 获取一个可用的连接接口
	shared_ptr<Connection> getConnection();
private:

	ConnectionPool();//构造函数私有化
	~ConnectionPool();
	
	//在池里独立的生产者
	void proConnTasker();
	//从配置文件中加载配置项
	bool loadConfigFile();

	void scannerConnTasker();

	string _ip;
	unsigned short _port;
	string _username;
	string _password;
	string _dbname;
	int _initSize;
	int _maxSize;
	int _maxIdleTime; //最大空闲时间
	int _connTimeout; //连接池获取连接的超时时间
	atomic_int _curConnNum;  //当前队列里存在空余conn
	queue<Connection*> _connQue;//存储连接的队列
	mutex _queueMutex;  //维护连接的线程安全互斥锁
	condition_variable notFull;  //连接生产者和消费者线程
	condition_variable notEmpty;
	condition_variable cv;
	condition_variable wait;
	condition_variable isend;
	atomic_bool isStart=false;

};