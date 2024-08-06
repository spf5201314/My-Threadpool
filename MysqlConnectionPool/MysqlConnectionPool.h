#pragma once
/*ʵ�����ӳ�ģ��*/
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
	//��ȡ���ӳض����ʵ��
	static ConnectionPool* getConnectionPool();
	//���ⲿ�ṩ�ӿ� ��ȡһ�����õ����ӽӿ�
	shared_ptr<Connection> getConnection();
private:

	ConnectionPool();//���캯��˽�л�
	~ConnectionPool();
	
	//�ڳ��������������
	void proConnTasker();
	//�������ļ��м���������
	bool loadConfigFile();

	void scannerConnTasker();

	string _ip;
	unsigned short _port;
	string _username;
	string _password;
	string _dbname;
	int _initSize;
	int _maxSize;
	int _maxIdleTime; //������ʱ��
	int _connTimeout; //���ӳػ�ȡ���ӵĳ�ʱʱ��
	atomic_int _curConnNum;  //��ǰ��������ڿ���conn
	queue<Connection*> _connQue;//�洢���ӵĶ���
	mutex _queueMutex;  //ά�����ӵ��̰߳�ȫ������
	condition_variable notFull;  //���������ߺ��������߳�
	condition_variable notEmpty;
	condition_variable cv;
	condition_variable wait;
	condition_variable isend;
	atomic_bool isStart=false;

};