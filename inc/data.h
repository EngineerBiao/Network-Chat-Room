#ifndef __DATA_H
#define __DATA_H

/**
 * 数据库名是user.db，表名是user，存储用户的用户名、密码、在线文件描述符、是否注册过
 * 数据库的表结构如下
 * Usrname(int)  Password(text)  fd(int)  chat(int)
 * 
 * 数据库相关函数
 * 各数据的作用：用户名和密码用来登录，然后在线文件描述符用于判断该用户是否在线并对它进行操作，是否注册过用来判断用户注册时是否重名
 * 补充说明
 *  文件描述符fd：-1表示用户不在线，>0表示用户在线且值为其套接字的文件描述符
 *  聊天状态chat：0表示没进入聊天状态，1表示进入聊天状态
 */

int database_init(); // 数据库初始化（创建数据库、打开数据表，数据库将被创建在bin目录中）
int is_user_registered(int name); // 判断用户是否注册过，返回0表示没注册过，1表示注册过，-1则是错误
int add_user(int name, char *password); // 添加用户，成功返回0，失败返回-1
int password_check(int name, char *password); // 检查密码是否正确，正确返回0，错误返回-1
int user_on_off(int fd, int name, int on_off); // 更新用户上线或下线状态，成功返回0，失败返回-1
int user_if_online(int name); // 判断用户是否在线，在线返回1，不在线返回-1，未注册返回-2
int broadcast(int name, int fd, char *message); // 向所有用户发送公聊消息，name和fd是发送消息的客户端用户名和fd，message是消息

#define DATABASE_NAME "user.db"
#define TABLE_NAME "user"
// 在宏定义中，要用\来连接字符串常量（如果不是宏定义就不需要）
#define SQL_TABLE_CREATE "create table if not exists user"      \
                         "(name int primary key not null, "     \
                         "password text not null, "             \
                         "fd int not null, "                    \
                         "chat int not null);" 


#endif 
