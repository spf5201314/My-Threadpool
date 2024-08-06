#pragma once
/*实现数据库的增删改查操作*/
#include<mysql.h>
#include<string>
#include<ctime>
using namespace std;

class Connection
{
public:

	// 初始化数据库连接
	Connection();
	//释放数据库连接资源
	~Connection();
	
	// 连接数据库
	bool connect(string ip,
		unsigned short port,
		string user,
		string password,
		string dbname);
	
	// 更新操作 insert、delete、update
	bool update(string sql);
	
	// 查询操作 select
	MYSQL_RES* query(string sql);

	//刷新连接存活时间

	void refreshAlivetime() 
	{ 
		_alivetime = clock(); 
	}

	clock_t getAlivetime() const
	{
		return clock()-_alivetime;
	}
	

private:
	MYSQL * _conn;  //表示和MySQL server的一条连接
	clock_t _alivetime; //进入队列的空闲时间
};