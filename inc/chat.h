#ifndef __CHAT_H
#define __CHAT_H

#define SERVER_PORT 9527 // 服务器端口号

// 在线用户信息
struct ONLINE {
    int fd;          // -1：该用户下线  >0：该用户已登录，值为对应的套接字
    int flage;       // -1：该条目没有用户信息  1：该条目有用户注册信息
    char name[32];   // 注册的用户名字
    char passwd[32]; // 用户名密码
};
struct ONLINE online[64]; // 用一个全局数组保存用户信息，后面再用数据库

// C/S通信格式
struct protocol {
    int cmd;       // 命令
    int state;     // 存储命令返回信息
    char name[32]; // 用户名
    char data[64]; // 数据
};

// 具体命令
#define BROADCAST 0X00000001  // 广播数据
#define PRIVATE 0X00000002    // 私聊
#define REGISTE 0X00000004    // 注册账号
#define LOGIN 0X00000008      // 登录
#define ONLINEUSER 0X00000010 // 显示在线用户
#define LOGOUT 0X00000020     // 退出
// 命令处理结果返回值
#define OP_OK 0X80000000           // 操作成功
#define ONLINEUSER_OK 0X80000001   // 显示在线用户，未结束
#define ONLINEUSER_OVER 0X80000002 // 显示在线用户，已经发送完毕
#define NAME_EXIST 0X80000003      // 注册信息，用户名已经存在
#define NAME_PWD_NMATCH 0X80000004 // 登录时，输入的用户名密码不对
#define USER_LOGED 0X80000005      // 登录时，提示该用户已经在线
#define USER_NOT_REGIST 0X80000006 // 登录时，提示用户没有注册

#endif