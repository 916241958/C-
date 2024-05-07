#include "../include/public.h"
#include "../include/pch.h"
#include "../include/Connection.h"


Connection::Connection()
{
    _conn = mysql_init(nullptr);    // 初始化数据库连接
}

Connection::~Connection()
{
    if(_conn != nullptr)
        mysql_close(_conn); // 释放数据库连接资源
}

bool Connection::connect(string ip, unsigned short port, string user
        , string password, string dbname)
{
    // 连接数据库
    MYSQL *p = mysql_real_connect(_conn, ip.c_str(), user.c_str()
            , password.c_str(), dbname.c_str(), port, nullptr, 0);
    return  p != nullptr;
}

bool Connection::update(string sql)
{
    if(mysql_query(_conn, sql.c_str()))
    {
        LOG("更新失败：" + sql);
        cout << mysql_error(_conn) << endl;
        return false;
    }
    return true;
}

MYSQL_RES* Connection::query(string sql)
{
    // 查询操作 select
    if(mysql_query(_conn, sql.c_str()))
    {
        LOG("查询失败：" + sql);
        return nullptr;
    }
    return mysql_use_result(_conn);
}