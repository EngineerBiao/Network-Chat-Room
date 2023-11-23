#ifndef __CHAT_H
#define __CHAT_H

/* 客户端和服务端之间通讯的消息规定 */

#define SERVER_PORT 9527 // 服务器端口号

// C/S通信数据格式
struct Message {
    int cmd;       // 命令
    int state;     // 存储命令返回信息（作用：服务端通过这个告诉客户端信息，然后客户端根据信息可以打印出用户的操作问题）
    int name;      // 用户名
    char data[64]; // 数据
};

// 具体命令
#define REGISTE 0X00000004    // 注册账号
#define LOGIN 0X00000008      // 登录
#define LOGOUT 0X00000020     // 下线并退出
#define BROADCAST 0X00000001  // 广播数据
#define PRIVATE 0X00000002    // 私聊
#define ONLINEUSER 0X00000010 // 显示在线用户

// 命令处理结果返回值
#define OP_OK 0X80000000           // 操作成功
#define ONLINEUSER_OK 0X80000001   // 显示在线用户，未结束
#define ONLINEUSER_OVER 0X80000002 // 显示在线用户，已经发送完毕
#define NAME_EXIST 0X80000003      // 注册信息，用户名已经存在
#define PASSWORD_WRONG 0X80000004 // 登录时，输入的用户名密码不对
#define USER_LOGED 0X80000005      // 登录时，提示该用户已经在线
#define USER_NOT_REGIST 0X80000006 // 登录时，提示用户没有注册

#endif