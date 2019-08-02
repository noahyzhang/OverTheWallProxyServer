#include "mysql.hpp"


MysqlAPI::MysqlAPI()
{
    m_bConnect = false;
    m_num_count = 0;
    m_num_count = 0;
    m_query = NULL;
}

MysqlAPI::~MysqlAPI()
{
    if(m_bConnect)
        DisConnectDB();
}


bool MysqlAPI::ConnectDB() 
{
    if(!mysql_init(&m_mysql))
    {
        std::cout << "mysql init failed!" << std::endl;
        return false;
    }

    if (mysql_real_connect(&m_mysql, host, user, password, database, 0, NULL, 0) == NULL) 
    {
        //std::cout << "mysql_real_connect failed!" << std::endl;
        perror("mysql_real_connect");
        mysql_close(&m_mysql);
        return false;
    }

    if (mysql_set_character_set(&m_mysql, "utf8") != 0) 
    {
        std::cout << "mysql_set_character_set failed!" << std::endl;
        mysql_close(&m_mysql);
        return false;
    }
    m_bConnect = true;
    return true;
}

void MysqlAPI::DisConnectDB()  //关闭数据库
{
    if (m_bConnect == false)
        return;
    FreeResult();
    mysql_close(&m_mysql);
    m_bConnect = false;

}
bool MysqlAPI::ExecSql(std::string& sqlStr) //执行sql 语句
{
    //std::cout << "aaaaaaaaaaaaaaaaaa" << std::endl;
    if (m_bConnect == false)
    {
        if(ConnectDB() == false)
            return false;
    }
    //std::cout << "bbbbbbbbbbbbbbbbbbbb" << std::endl;
    int ret = mysql_query(&m_mysql, sqlStr.c_str());
    if (ret)
    {
        //int n_mysql_errno = mysql_errno(&m_mysql);
        //perror("mysql_query");
        std::cout << "ExMySQL : " << sqlStr.c_str() << std::endl;
        std::cout << "Exec errno : " << mysql_error(&m_mysql) << std::endl;
        return false;
    }
    return true;

}

bool MysqlAPI::StoreResult()  //返回查找  select、show、describe、explain 等语句执行的结果
{
    if (m_bConnect == false)
        return false;
    FreeResult();

    m_query = mysql_store_result(&m_mysql);
    if (m_query)
    {
        m_num_field = mysql_num_fields(m_query); //返回结果集中的行数
        m_num_count = mysql_num_rows(m_query);  //返回结果集中的行数，如果使用mysql_store_result(),可以立刻使用mysql_num_rows()。
        return true;
    }
    return false;
}

MYSQL_ROW MysqlAPI::GetNextRow() //获取查询结果集中的下一条记录
{
    if (m_bConnect == false)
        return NULL;
    if (!m_query)
        return NULL;
    m_row = mysql_fetch_row(m_query); //获取结果集的下一行
    return m_row;
}

void MysqlAPI::GotoRowsFirst()  // 移动到数据集的开始
{
    if (m_bConnect == false)
        return;
    if (!m_query)
        return;
    mysql_data_seek(m_query, 0);
}

void MysqlAPI::PrintRows() //打印记录
{
    if (m_bConnect == false)
        return;
    if (!m_query)
        return;
    if (m_num_count > 0)
        mysql_data_seek(m_query, 0);

    while (MYSQL_ROW row = mysql_fetch_row(m_query))
    {
        for (int i = 0; i < m_num_field; ++i)
            std::cout << row[i] << "    ";
        std::endl(std::cout);

    }
    std::cout << "____________________________________________" << std::endl;
    std::cout << m_num_count << std::endl;
    if (m_num_count > 0)
        mysql_data_seek(m_query, 0);
}

void MysqlAPI::FreeResult() //释放查询的数据集的内存资源
{
    if (m_query)
    {
        mysql_free_result(m_query);
        m_num_field = 0;
        m_num_count = 0;
        m_query = NULL;
    }
}


