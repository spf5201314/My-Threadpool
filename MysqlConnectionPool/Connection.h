#pragma once
/*ʵ�����ݿ����ɾ�Ĳ����*/
#include<mysql.h>
#include<string>
#include<ctime>
using namespace std;

class Connection
{
public:

	// ��ʼ�����ݿ�����
	Connection();
	//�ͷ����ݿ�������Դ
	~Connection();
	
	// �������ݿ�
	bool connect(string ip,
		unsigned short port,
		string user,
		string password,
		string dbname);
	
	// ���²��� insert��delete��update
	bool update(string sql);
	
	// ��ѯ���� select
	MYSQL_RES* query(string sql);

	//ˢ�����Ӵ��ʱ��

	void refreshAlivetime() 
	{ 
		_alivetime = clock(); 
	}

	clock_t getAlivetime() const
	{
		return clock()-_alivetime;
	}
	

private:
	MYSQL * _conn;  //��ʾ��MySQL server��һ������
	clock_t _alivetime; //������еĿ���ʱ��
};