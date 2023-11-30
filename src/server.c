#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> // POSIX线程库，由于该库不是Linux默认库，所以要在编译时加-lpthread参数才能完成链接
#include "chat.h"
#include "data.h"

static void registe(int fd, struct Message *msg); // 注册
static void login(int fd, struct Message *msg); // 登录
static void logout(int fd, struct Message *msg); // 用户下线
static void global_chat(); // 公聊

void sys_err(const char *str)
{
    perror(str);
    exit(1);
}
void *thread(void *arg) // 子线程操作程序
{
    char buf[BUFSIZ]; // 默认缓冲区，大小是8192
    struct Message msg;
    int fd = *((int *)arg); // 先把void指针转换为int指针，然后再解引用
    free(arg); // 子线程获取到套接字的值之后就可以删掉堆区内存
    
    while (1)
    {
        int ret = read(fd, &msg, sizeof(msg)); // 用msg来接收客户端给服务器发送的消息
        if (ret == -1) // 读取出错
        {
            perror("read error");
            close(fd);
            return;
        }
        if (ret == 0) // 客户端关闭连接
        {
            printf("客户端关闭连接\n");
            close(fd);
            return;
        }
        switch (msg.cmd) // 根据客户端发送过来的消息命令进行处理
        {
            case REGISTE:
                registe(fd, &msg); break;
            case LOGIN:
                login(fd, &msg); break;
            case LOGOUT:
                logout(fd, &msg); return; // 直接结束该通信的线程
            case BROADCAST:
                global_chat(fd, &msg); break;
            default:
                break; 
        }
    }
    
}
int main()
{
    int fd, ret;
    struct sockaddr_in server_addr;
    /* 服务器地址结构 */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);       // 本地转网络字节序
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 该宏对应于0.0.0.0，表示服务器将倾听来自任何网络接口的连接

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        sys_err("socket error");
    if (bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        sys_err("bind error");
    if (listen(fd, 128) == -1)
        sys_err("listen error");
    // 初始化数据库
    database_init();
    
    while (1)
    {
        int new_fd;                     // 接收accept返回的用来通信的套接字句柄
        struct sockaddr_in client_addr; // 用来接收accept函数传出的客户端地址结构
        socklen_t clit_addr_len = sizeof(client_addr);
        if ((new_fd = accept(fd, (struct sockaddr *)&client_addr, &clit_addr_len)) == -1)
            sys_err("accept error");

        // 打印成功连接的客户端地址信息，要使用inet_ntop网络转本地函数来获取的字符串地址结构
        char ip_buf[INET_ADDRSTRLEN]; // INET_ADDRSTRLEN是IPv4地址字符串的最大长度
        const char *ip_str = inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, ip_buf, INET_ADDRSTRLEN);
        printf("client's IP is %s, and port is %d\n", ip_str, ntohs(client_addr.sin_port));

        // 把新套接字的值赋值给堆区内存来传入子线程
        int *arg_fd = (int *)malloc(sizeof(int));
        *arg_fd = new_fd;
        // 创建子线程
        pthread_t tid;
        ret = pthread_create(&tid, NULL, thread, (void *)arg_fd);
        if (ret != 0)
            sys_err("thread create error");
        pthread_detach(tid);
    }
    close(fd); // 主线程结束循环后，就可以关闭用来监听的套接字了（用来通讯的套接字由子线程关闭）
    return 0;
}
// 注册
static void registe(int fd, struct Message *msg)
{
    struct Message msg_back; // 发送回客户端的消息
    msg_back.cmd = REGISTE;
    if (is_user_registered(msg->name) == 1) // 如果注册过则不进行注册
    {
        printf("name has exist\n");
        msg_back.state = NAME_EXIST;
    }
    else
    {
        add_user(msg->name, msg->data);
        printf("register success\n");
        msg_back.state = OP_OK;
    }
    write(fd, &msg_back, sizeof(msg_back)); // 发送操作结果给客户端
}
// 登录
static void login(int fd, struct Message *msg)
{
    struct Message msg_back; // 发送回客户端的消息
    msg_back.cmd = LOGIN;
    // 查询用户是否注册
    if (is_user_registered(msg->name) != 1)
    {
        printf("Username not register\n");
        msg_back.state = USER_NOT_REGIST;
        write(fd, &msg_back, sizeof(msg_back));
        return;
    }
    // 检查密码是否正确
    if (password_check(msg->name, msg->data) == 0)
        printf("密码正确\n");
    else
    {
        printf("密码错误\n");
        msg_back.state = PASSWORD_WRONG;
        write(fd, &msg_back, sizeof(msg_back));
        return;
    }
    // 判断用户是否已上线（一个账号只能有一个客户端同时使用）
    if (user_if_online(msg->name) == 1)
    {
        printf("用户已上线\n");
        msg_back.state = USER_LOGED;
        write(fd, &msg_back, sizeof(msg_back));
        return;
    }
    // 注册且密码正确就登录成功，把数据库中的fd字段改成该套接字文件的值
    if (user_on_off(fd, msg->name, 1) == 0) 
    {
        msg_back.state = OP_OK;
        write(fd, &msg_back, sizeof(msg_back));
    }
    else
        printf("用户上线失败"); // 基本不会上线失败，这条消息是打印给服务器端的（以防万一）
}
// 用户下线
static void logout(int fd, struct Message *msg)
{
    user_on_off(fd, msg->name, 0); // 基本不会失败
    printf("用户%d下线\n", msg->name);
    close(fd);
}
// 发送公聊消息
static void global_chat(int fd, struct Message *msg)
{
    // 消息是"-1"表示退出聊天
    if (strcmp(msg->data, "-1") == 0)
    {
        leave_chat(fd);
        return;
    }
    broadcast(msg->name, fd, msg->data);
}