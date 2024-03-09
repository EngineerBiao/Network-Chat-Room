#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <pthread.h> // POSIX线程库，由于该库不是Linux默认库，所以要在编译时加-lpthread参数才能完成链接
#include "chat.h"
#include "data.h"

#define MAX_EVENTS 1024 // epoll红黑树监听的最大事件数量，1024是进程默认的文件描述符表大小
#define REACTOR_NUM 4   // 4核CPU，所以创建4个从Reactor线程

static void registe(int fd, struct Message *msg);      // 注册
static void login(int fd, struct Message *msg);        // 登录
static void logout(int fd, struct Message *msg);       // 用户下线
static void global_chat(int fd, struct Message *msg);  // 公聊
static void online_user_status(int fd);                // 列出在线用户及其聊天状态
static int choose_user(struct Message *msg);           // 返回用户端选择私聊的用户名的套接字
static void private_chat(int fd, int chat_fd, struct Message *msg); // 私聊
int new_accept(int listen_fd); // 处理连接请求。返回新套接字，并打印连接信息

void sys_err(const char *str) // 错误处理函数
{
    perror(str);
    exit(1);
}
void *sub_thread(void *arg) // 子线
{
    int chat_fd; // 私聊用的套接字
    char buf[BUFSIZ]; // 默认缓冲区，大小是8192
    struct Message msg;
    int epfd = (int)arg;
    struct epoll_event events[MAX_EVENTS];
    while (1)
    {
        int num_events = epoll_wait(epfd, events, MAX_EVENTS, -1); // 阻塞监听
        if (num_events == -1)
            sys_err("epoll_wait error");
        for (int i = 0; i < num_events; i++) // 循环处理事件（优化版是通过线程池来处理）
        {
            int fd = events[i].data.fd; // 获取发生读事件的描述符
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
                logout(fd, &msg); // 关闭连接后就对该用户下线
                close(fd);
                return;
            }
            switch (msg.cmd) // 根据客户端发送过来的消息命令进行处理
            {
                case REGISTE:
                    registe(fd, &msg);
                    break;
                case LOGIN:
                    login(fd, &msg);
                    break;
                case LOGOUT:
                    logout(fd, &msg);
                    return;     // 直接结束该通信的线程
                case STARTCHAT: // 进入聊天状态
                    set_chat(fd, 1);
                    break;
                case BROADCAST:
                    global_chat(fd, &msg);
                    break;
                case ONLINEUSER:
                    online_user_status(fd);
                    break;
                case CHOOSE:
                    set_chat(fd, 1); // 更新数据库中的聊天状态
                    chat_fd = choose_user(&msg);
                    break; // 获取客户端选择的聊天用户的套接字
                case PRIVATE:
                    private_chat(fd, chat_fd, &msg);
                    break;
                default:
                    break;
            }
        }
    }
}
int main()
{
    int listen_fd, ret;
    struct sockaddr_in server_addr;
    struct epoll_event event;              // event是单个文件描述符事件的结构体
    struct epoll_event events[MAX_EVENTS]; // events是wait函数传出的满足事件的结构体
    /* 服务器地址结构 */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);       // 本地转网络字节序
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 该宏对应于0.0.0.0，表示服务器将倾听来自任何网络接口的连接
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        sys_err("socket error");
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        sys_err("bind error");
    if (listen(listen_fd, 128) == -1)
        sys_err("listen error");
    // 初始化数据库
    database_init();

    // 创建4个从Reactor线程并为它们分配epoll句柄
    pthread_t sub_threads[REACTOR_NUM];
    int epoll_fds[REACTOR_NUM];
    for (int i = 0; i < REACTOR_NUM; i++)
    {
        // 为每个从线程创建epoll句柄，并传入从线程以便从线程进行监听
        if ((epoll_fds[i] = epoll_create1(0)) == -1)
            sys_err("epoll create错误\n");
        if (pthread_create(&sub_threads[i], NULL, sub_thread, (void *)epoll_fds[i]) != 0)
            sys_err("thread create错误\n");
    }
    
    // 设置主线程的epoll多路IO复用
    int epfd = epoll_create1(0);
    event.events = EPOLLIN;                          // 监听读事件
    event.data.fd = listen_fd;                             // 指定要监听的文件描述符
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &event)) // listen_fd添加到监听红黑树上
        sys_err("epoll_ctl error\n");
    int index = 0; // 轮询子线程的索引
    
    while (1) // 实施监听
    {
        if (epoll_wait(epfd, events, 1, -1) != 1) // 阻塞监听，只有一个文件描述符，只会返回1
            sys_err("epoll wait error\n");
        int client_fd = new_accept(listen_fd); // 处理连接
        // 将新的套接字轮询分给从线程（添加节点到从线程的epoll句柄上）
        event.events = EPOLLIN;
        event.data.fd = client_fd;
        if (epoll_ctl(epoll_fds[index], EPOLL_CTL_ADD, client_fd, &event) == -1)
            sys_err("epoll_ctl error\n");
        // 更新index（这个更新方法需要很熟悉）
        index = (index + 1) % REACTOR_NUM;
    }
    return 0;
}
// 处理连接请求，返回新套接字，并打印连接信息
int new_accept(int listen_fd)
{
    int client_fd;
    struct sockaddr_in client_addr; // 用来接收accept函数传出的客户端地址结构
    socklen_t clit_addr_len = sizeof(client_addr);
    if ((client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &clit_addr_len)) == -1)
        sys_err("accept error");
    // 打印成功连接的客户端地址信息，要使用inet_ntop网络转本地函数来获取的字符串地址结构
    char ip_buf[INET_ADDRSTRLEN]; // INET_ADDRSTRLEN是IPv4地址字符串的最大长度
    const char *ip_str = inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, ip_buf, INET_ADDRSTRLEN);
    printf("client's IP is %s, and port is %d\n", ip_str, ntohs(client_addr.sin_port));
    return client_fd;
}

// 注册
static void registe(int fd, struct Message *msg)
{
    struct Message msg_back; // 发送回客户端的消息
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
    // 已注册且密码正确，把数据库中的fd字段改成该套接字文件的值
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
        set_chat(fd, 0); // 更新数据库中的聊天状态
        char data[64];
        snprintf(data, sizeof(data), "用户%d退出聊天室", msg->name);
        broadcast(msg->name, fd, data); // 发送消息告知其他用户
        return;
    }
    broadcast(msg->name, fd, msg->data);
}
// 列出在线用户及其聊天状态
static void online_user_status(int fd)
{
    struct Message msg_back;
    user_status(fd);
    // 执行完后发送ONLINEUSER_OVER告诉客户端已列举结束
    msg_back.state = ONLINEUSER_OVER;
    if (write(fd, &msg_back, sizeof(msg_back)) == -1)
        printf("发送消息失败\n");
}
// 返回用户端选择私聊的用户名的套接字
static int choose_user(struct Message *msg)
{
    int fd = find_fd(atoi(msg->data));
    printf("用户%d选择私聊用户%s，套接字为%d\n", msg->name, msg->data, fd);
    return fd;
}
// 私聊
static void private_chat(int fd, int chat_fd, struct Message *msg)
{
    // 消息是"-1"表示退出聊天
    if (strcmp(msg->data, "-1") == 0)
    {
        // 发送消息给自己，防止客户端子线程阻塞
        char *data = "退出聊天室";
        write(fd, data, strlen(data));
        set_chat(fd, 0); // 更新数据库中的聊天状态
        return;
    }
    // 对fd和chat_fd发送消息（对自己也发送的原因是让聊天消息在双方画面上相同）
    char message[1024];
    snprintf(message, sizeof(message), "[Message] User %d send message: %s\n", msg->name, msg->data);
    if (write(chat_fd, message, strlen(message)) == -1)
        printf("write error");
    if (write(fd, message, strlen(message)) == -1)
        printf("write error");
}
