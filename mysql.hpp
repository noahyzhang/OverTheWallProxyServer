#ifndef __MYSQL_HPP__
#define __MYSQL_HPP__

#include <mysql.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string>
#include <pthread.h> 
#include <iostream>

static const char *host = "localhost";
static const char *user = "root";
static const char *password = "1234";
static const char *database = "transfer";

static const char* ErrLevel[] = {
	"Normal", //正常
	"Warning", //警告
	"Error"   //错误
};

class MysqlAPI
{
    public:
        MysqlAPI();
        void DisConnectDB();
        bool ConnectDB();
        bool ExecSql(std::string&);
        bool StoreResult();
        MYSQL_ROW GetNextRow(); //获取查询结果集中的下一条记录
        void GotoRowsFirst();  // 移动到数据集的开始
        void PrintRows();     //打印记录
        void FreeResult();   //释放查询的数据集的内存资源
    public:  // test 
        bool M_bConnect()
        {
            return m_bConnect;
        }

        ~MysqlAPI();
    private:
        MYSQL m_mysql;
        MYSQL_RES *m_query;
        MYSQL_ROW m_row;

        bool m_bConnect;
        int m_num_field;
        int m_num_count;
};

#if 0
class Singleton{
    private:
        Singleton();
        Singleton(const Singleton&);
        Singleton& operator=(const Singleton&);
        static pthread_mutex_t lock;
        static MysqlAPI* p;
    public:
        static MysqlAPI* GetInstance()
        {
            if(p == NULL)
            {
                pthread_mutex_lock(&lock);
                if(p == NULL)
                {
                    p = new MysqlAPI;
                }
                pthread_mutex_unlock(&lock);
            }
            return p;
        }
};

pthread_mutex_t Singleton::lock = PTHREAD_MUTEX_INITIALIZER;
MysqlAPI* Singleton::p = NULL;
#endif 

#endif
