#ifndef __CHAT_H
#define __CHAT_H

/* 客户端和服务端之间通讯的消息规定 */

#define SERVER_PORT 9527 // 服务器端口号

// C/S通信数据格式
struct Message {
    int cmd;       // 命令
    int state;     // 命令执行状态（作用：服务端回送消息，客户端根据该状态来打印出用户操作是否成功）
    int name;      // 用户名
    char data[64]; // 数据
};

// 具体命令的宏
#define REGISTE 1    // 注册账号
#define LOGIN 2      // 登录
#define LOGOUT 3     // 下线并退出
#define STARTCHAT 4  // 进入聊天状态
#define BROADCAST 5  // 广播
#define CHOOSE 6     // 选择要私聊的用户
#define PRIVATE 7    // 私聊
#define ONLINEUSER 8 // 在线用户及状态

// 命令执行状态
#define OP_OK 10           // 操作成功
#define NAME_EXIST 11      // 注册时，用户名已经存在
#define PASSWORD_WRONG 12  // 登录时，输入的用户名密码不对
#define USER_LOGED 13      // 登录时，提示该用户已在线
#define USER_NOT_REGIST 14 // 登录时，提示用户没有注册
#define ONLINEUSER_OK 15   // 列出在线用户状态未结束
#define ONLINEUSER_OVER 16 // 列出在线用户状态已结束

#endif