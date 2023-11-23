#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include "data.h"

sqlite3 *db; // 该数据库只有一个表，所以可以放在全局变量而不用担心多线程问题

// 数据库初始化（打开数据库、创建数据表）
int database_init()
{
    char *errmsg;
    // 打开数据库
    if (sqlite3_open(DATABASE_NAME, &db) < 0)
    {
        printf("db open error: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    // 创建表
    const char *sql = SQL_TABLE_CREATE;
    int ret = sqlite3_exec(db, sql, NULL, 0, &errmsg);
    if (ret != 0)
    {
        printf("table create error: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }
    return 0;
}

// 判断用户是否注册，1表示注册过，0表示没注册过
int re_callback(void *data, int argc, char **argv, char **azColName)
{
    int *register_flag = (int *)data;
    if (argc == 0) // 没有查询到结果
        *register_flag = 0;
    else
        *register_flag = 1;
    return 0;
}
int is_user_registered(int name)
{
    char *errmsg;
    int register_flag;
    // 先格式化写入sql语句到字符串中
    char sql[1024];
    snprintf(sql, sizeof(sql), "select register from %s where name = %d;", TABLE_NAME, name);
    // 执行sql查询该用户名
    int ret = sqlite3_exec(db, sql, re_callback, &register_flag, &errmsg);
    if (ret != 0)
    {
        printf("sqlite3 select error: %s\n", sqlite3_errmsg(db));
        sqlite3_free(errmsg); // 释放错误信息内存
        return -1;
    }
    return register_flag; // 由回调函数设置
}
// 添加用户
int add_user(int name, char *password)
{
    char sql[1024];
    char *errmsg;
    snprintf(sql, sizeof(sql), "insert into %s values(%d, '%s', -1, 1);", TABLE_NAME, name, password);
    int ret = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (ret != 0)
    {
        printf("sqlite3 insert error: %s\n", sqlite3_errmsg(db));
        sqlite3_free(errmsg); // 释放错误信息内存
        return -1;
    }
    return 0;
}
// 检查密码是否正确
int check_flag;
int ch_callback(void *data, int argc, char **argv, char **azColName)
{
    char *password = (char *)data;
    if (strcmp(argv[0], password) == 0) // 字符串相等函数返回0
        check_flag = 0;
    else
        check_flag = -1;
    return 0;
}
int password_check(int name, char *password)
{
    char sql[1024];
    char *errmsg;
    snprintf(sql, sizeof(sql), "select password from %s where name = %d", TABLE_NAME, name);
    int ret = sqlite3_exec(db, sql, ch_callback, password, &errmsg);
    if (ret != 0)
    {
        printf("sqlite3 select error: %s\n", sqlite3_errmsg(db));
        sqlite3_free(errmsg); // 释放错误信息内存
        return -1;
    }
    return check_flag; // 由回调函数设置
}

/**
 * @brief 更新用户上线或下线状态（更新数据库的fd）
 * 
 * @param fd 传入对应客户端的套接字，然后更新到数据库中
 * @param name 传入用户名用来进行查询操作
 * @param on_off 传入0或1，0表示下线，1表示上线
 * 
 * @return 成功返回0，失败返回-1
 */
int user_on_off(int fd, int name, int on_off)
{
    char sql[1024];
    int fd_value;
    char *errmsg;
    if (on_off == 0) // 下线
        fd_value = -1;
    else
        fd_value = fd;
    
    snprintf(sql, sizeof(sql), "update %s set fd=%d where name=%d;", TABLE_NAME, fd_value, name);
    int ret = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (ret != 0)
    {
        printf("sqlite3 update error: %s\n", sqlite3_errmsg(db));
        sqlite3_free(errmsg); // 释放错误信息内存
        return -1;
    }
    return 0;
}
// 判断用户是否在线，在线返回1，否则返回-1
int on_callback(void *data, int argc, char **argv, char **azColName)
{
    // int online_flag = *((int *)data);
    int *online_flag = (int *)data;
/*  上面第一种是创建一个局部变量，改变这个online_flag并不会对exec函数中的online_flag造成影响
    而第二种是声明一个指针来指向data，那么改变这个指针会对exec函数中的online_flag造成影响*/
    
    if (strcmp(argv[0], "-1") == 0) // 下线状态
        *online_flag = -1;
    else
        *online_flag = 1;
    return 0;
}
int user_if_online(int name)
{
    char sql[1024];
    char *errmsg;
    int online_flag;
    snprintf(sql, sizeof(sql), "select fd from %s where name=%d;", TABLE_NAME, name);
    int ret = sqlite3_exec(db, sql, on_callback, &online_flag, &errmsg);
    if (ret != 0)
    {
        printf("sqlite3 select error: %s\n", sqlite3_errmsg(db));
        sqlite3_free(errmsg); // 释放错误信息内存
        return -1;
    }
    return online_flag;
}
