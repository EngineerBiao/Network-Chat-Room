#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <unistd.h>
#include <string.h>
#include "data.h"
#include "chat.h"

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
    // 创建表（已存在就不创建）
    const char *sql = SQL_TABLE_CREATE;
    int ret = sqlite3_exec(db, sql, NULL, 0, &errmsg);
    if (ret != 0)
    {
        printf("table create error: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }
    // 每次服务器启动先将大于0的fd重置为-1和重置聊天状态为0（服务器意外关闭时，数据库的用户句柄来不及下线）
    char *sql_fd = "update user set fd=-1 where fd>0; update user set chat=0;";
    ret = sqlite3_exec(db, sql_fd, NULL, 0, &errmsg);
    if (ret != 0)
    {
        printf("fd update error: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }
    return 0;
}

// 判断用户是否注册，1表示注册过，0表示没注册过
int re_callback(void *data, int argc, char **argv, char **azColName)
{
    int *register_flag = (int *)data;
    *register_flag = 1; // 能进入回调函数说明注册过了，所以设值为1
    return 0;
}
int is_user_registered(int name)
{
    char *errmsg;
    int register_flag = 0; // 0表示没注册过，1表示注册过
    // 先格式化写入sql语句到字符串中
    char sql[1024];
    snprintf(sql, sizeof(sql), "select name from %s where name=%d;", TABLE_NAME, name);
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
    snprintf(sql, sizeof(sql), "insert into %s values(%d, '%s', -1, 0);", TABLE_NAME, name, password);
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
// 判断用户是否在线，在线返回1，不在线返回-1，未注册返回-2
int on_callback(void *data, int argc, char **argv, char **azColName)
{
    // int online_flag = *((int *)data);
    int *online_flag = (int *)data;
/*  上面第一种是创建一个局部变量，改变这个online_flag并不会对exec函数中的online_flag造成影响
    而第二种是声明一个指针来指向data，那么改变这个指针会对exec函数中的online_flag造成影响*/
    
    if (argc == 0) // 未注册
        *online_flag = -2;
    else if (strcmp(argv[0], "-1") == 0) // 下线状态
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
// 设置聊天状态，更新chat为传入的chat值
int set_chat(int fd, int chat)
{
    char sql[1024];
    char *errmsg;
    snprintf(sql, sizeof(sql), "update %s set chat=%d where fd=%d;", TABLE_NAME, chat, fd);
    int ret = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (ret != 0)
    {
        printf("聊天状态退出错误: %s\n", sqlite3_errmsg(db));
        sqlite3_free(errmsg); // 释放错误信息内存
        return -1;
    }
    return 0;
}
// 向所有在线且处于聊天状态的用户发送消息，根据fd查找然后在回调函数中发送消息，fd是发送消息的客户端，message是消息
struct argst {
    int name;   // 发送消息的用户名
    char *message; // 消息内容
};
int br_callback(void *data, int argc, char **argv, char **azColName)
{
    struct argst *arg = (struct argst *)data;
    char str[1024];
    snprintf(str, sizeof(str), "[Message] User %d send message: %s\n", arg->name, arg->message);
    int ret = write(atoi(argv[0]), str, strlen(str));
    if (ret == -1)
    {
        printf("broadcast write error");
        return -1;
    }
    return 0;
}
int broadcast(int name, int fd, char *message)
{
    char sql[1024];
    char *errmsg;
    struct argst arg = {name, message};
    // 查找所有在线且进入聊天状态的用户
    snprintf(sql, sizeof(sql), "select fd from %s where fd>2 and chat=1;", TABLE_NAME);
    int ret = sqlite3_exec(db, sql, br_callback, &arg, &errmsg);
    if (ret != 0)
    {
        printf("sqlite3 select error: %s\n", sqlite3_errmsg(db));
        sqlite3_free(errmsg); // 释放错误信息内存
        return -1;
    }
    return 0;
}


int user_callback(void *data, int argc, char **argv, char **azColName)
{
    int fd = *((int *)data);
    struct Message msg;
    msg.state = ONLINEUSER_OK;
    snprintf(msg.data, sizeof(msg.data), "Username:%d\tchat_flag:%d", atoi(argv[0]), atoi(argv[1])); 
    int ret = write(fd, &msg, sizeof(msg));
    if (ret == -1)
    {
        printf("user_status write error");
        return -1;
    }
    return 0;
}
// 列出在线用户及其状态
int user_status(int fd)
{
    char sql[1024];
    char *errmsg;
    snprintf(sql, sizeof(sql), "select name, chat from %s where fd>0", TABLE_NAME);
    int ret = sqlite3_exec(db, sql, user_callback, &fd, &errmsg);
    if (ret != 0)
    {
        printf("列举在线用户错误: %s\n", sqlite3_errmsg(db));
        sqlite3_free(errmsg); // 释放错误信息内存
        return -1;
    }
    return 0;
}
// // 向指定用户名发送消息
// struct argst
// {
//     int name;      // 发送消息的用户名
//     char *message; // 消息内容
// };
// int br_callback(void *data, int argc, char **argv, char **azColName)
// {
//     struct argst *arg = (struct argst *)data;
//     char str[1024];
//     snprintf(str, sizeof(str), "[Message] User %d send message: %s\n", arg->name, arg->message);
//     int ret = write(atoi(argv[0]), str, strlen(str));
//     if (ret == -1)
//     {
//         printf("broadcast write error");
//         return -1;
//     }
//     return 0;
// }
// int private(int name, int fd, char *message) // 向用户发送私聊消息，name和fd是接收消息的客户端用户名和fd，message是消息
// {
//     char sql[1024];
//     char *errmsg;
//     struct argst arg = {name, message};
//     // 先更新自己为私聊状态
//     snprintf(sql, sizeof(sql), "update %s set chat=2 where fd=%d;", TABLE_NAME, fd);
//     int ret = sqlite3_exec(db, sql, NULL, &arg, &errmsg);
//     if (ret != 0)
//     {
//         printf("聊天状态更新错误: %s\n", sqlite3_errmsg(db));
//         sqlite3_free(errmsg); // 释放错误信息内存
//         return -1;
//     }
//     // 对要聊天的用户发送
//     snprintf(sql, sizeof(sql), "select fd from %s where fd>2 and chat=2;", TABLE_NAME);
//     ret = sqlite3_exec(db, sql, br_callback, &arg, &errmsg);
//     if (ret != 0)
//     {
//         printf("sqlite3 select error: %s\n", sqlite3_errmsg(db));
//         sqlite3_free(errmsg); // 释放错误信息内存
//         return -1;
//     }
//     return 0;
// }
// 根据用户名来查找套接字句柄并返回，如果返回-1表示该用户未上线或未注册
int fd;
int find_callback(void *data, int argc, char **argv, char **azColName)
{
    fd = atoi(argv[0]);
    return 0;
}
int find_fd(int name)
{
    char sql[1024];
    char *errmsg;
    fd = -1; // 重置fd
    snprintf(sql, sizeof(sql), "select fd from %s where name=%d", TABLE_NAME, name);
    int ret = sqlite3_exec(db, sql, find_callback, &fd, &errmsg);
    if (ret != 0)
    {
        printf("查找用户名错误: %s\n", sqlite3_errmsg(db));
        sqlite3_free(errmsg); // 释放错误信息内存
        return -1;
    }
    return fd; // 返回-1表示没有找到
}
