#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h> // POSIX线程库，由于该库不是Linux默认库，所以要在编译时加-lpthread参数才能完成链接
#include "chat.h"

static int registe();      // 注册
static int login();        // 登录
static void logout();      // 下线
static void set_flag();    // 更新服务端的聊天状态
static void global_chat(); // 公聊
static void private();     // 私聊
static void online_user_status(); // 列出在线用户

int fd; // 其他函数也要用该文件描述符，所以直接定义为全局变量
int login_f = -1; // 用来控制菜单显示，-1表示未登录，1表示已登录，
int chat_flag = 0; // 0表示未进入聊天状态，1表示聊天状态
int message_line = 2; // 聊天消息的行号
pthread_mutex_t mutex;
pthread_cond_t cond;

// 只有一个线程，所以msg直接设为文件作用域
struct Message msg, msg_back; // msg是message的简写，msg_back用来获取服务端发送回的消息

void sys_err(const char *str) // 错误封装处理函数
{
    perror(str);
    exit(1);
}
void *readThread(void *arg) // 子线程实时接收服务端发送的数据（处于聊天模式后）
{
    char buf[BUFSIZ]; // 默认缓冲区，大小是8192
    while (1) // 循环实时接收服务器发来的聊天消息
    {
        /* if (chat_flag == 0) // 处于聊天模式才接收数据
            continue;*/
        // 使用轮询的方法不利于程序效率，下面使用条件变量来确保进入聊天模式时才唤醒该线程
        pthread_mutex_lock(&mutex);
        while (chat_flag == 0) // 不是聊天状态时，进入循环然后阻塞等待唤醒
            pthread_cond_wait(&cond, &mutex); // 先解锁并阻塞等待唤醒，唤醒后会重新申请锁然后再进行判断语句
        pthread_mutex_unlock(&mutex);
        
        if (read(fd, buf, sizeof(buf)) == -1) // 读取内容
            sys_err("read error");
        
        // 用户退出了聊天室，此时需要重新等待唤醒
        if (chat_flag == 0)
        {
            memset(buf, '\0', sizeof(buf)); // 清空缓冲区
            continue;
        }
        if (message_line >= 15) // 当聊天消息显示到第16行就清屏
        {
            system("clear");
            if (chat_flag == 1)
                printf("\t\t公共聊天室（输入-1退出）");
            else
                printf("\t\t私人聊天室（输入-1退出）");
            message_line = 2;
        }
        printf("\e[%d;1H", message_line++); // 需要先移动光标，具体移动方法看GPT
        printf("%s", buf);
        memset(buf, '\0', sizeof(buf)); // 输出之后要清空缓冲区
    }
}
int main()
{
    int ret;
    // 客户端连接服务器只需调用socket和connect函数，后者需要传入服务器端的地址结构
    struct sockaddr_in server_addr; // 服务器的地址结构
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT); // 本地转网络（字符串转二进制）
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 这个宏是回环地址，代表127.0.0.1
    // 建立连接
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        sys_err("socket error");
    if (connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        sys_err("connect error");
    // 把套接字的值赋值给堆区内存来传入子线程
    int *arg_fd = (int *)malloc(sizeof(int));
    *arg_fd = fd;
    
    // 创建子线程前先初始化锁和条件变量
    if (pthread_mutex_init(&mutex, NULL) != 0)
        sys_err("mutex init error");
    if (pthread_cond_init(&cond, NULL) != 0)
        sys_err("cond init error");

    // 创建用来实时接收消息的子线程
    pthread_t tid;
    if (pthread_create(&tid, NULL, readThread, NULL) != 0)
        sys_err("thread create error");
    pthread_detach(tid); // 线程分离

    // 开始显示菜单
    int num, minNum, maxNum; // 用于菜单选择
    while (1)
    {
        if (login_f == -1)
        {
            printf("\t 1、注册\n");
            printf("\t 2、登录\n");
        }
        else
        {
            printf("\t 3、公聊\n");
            printf("\t 4、私聊\n");
            printf("\t 5、在线列表\n");
        }
        printf("\t 0、退出\n");
        printf("请选择功能：");
        scanf("%d", &num);
        if (num == 0)
        {
            if (login_f == -1) // 非登录状态就直接退出程序
                exit(0);
            else // 登录状态就先下线然后再退出
            {
                logout();
                exit(0);
            }
        }
        if (login_f == -1)
        {
            minNum = 1;
            maxNum = 2;
        }
        else
        {
            minNum = 3;
            maxNum = 5;
        }
        if (num < minNum || num > maxNum)
        {
            printf("wrong num! input again\n");
            continue;
        }
        switch (num)
        {
            case 1:
                registe();
                break;
            case 2:
                login();
                break;
            case 3:
                set_flag(); // 更新服务端的聊天状态
                chat_flag = 1;
                global_chat();
                break;
            case 4:
                private();
                break;
            case 5:
                online_user_status();
                break;
            default:
                break;
        }
    }
    // 释放相关资源
    close(fd);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
    return 0;
}
// 注册请求函数
static int registe()
{
    msg.cmd = REGISTE;
    printf("input Username: ");
    scanf("%d", &msg.name);
    printf("input Password(without spaces): ");
    scanf("%s", msg.data);
    
    write(fd, &msg, sizeof(msg)); // 向服务器发送该注册消息
    read(fd, &msg_back, sizeof(msg_back)); // 服务器接收到注册消息后会进行注册操作，并发送回注册状态
    if (msg_back.state != OP_OK)
    {
        printf("name has exist, try again!\n");
        return -1;
    }
    else
    {
        printf("register success\n");
        return 0;
    }
}
// 登录请求函数，成功返回0，失败返回-1
static int login()
{
    msg.cmd = LOGIN;
    printf("input your name: ");
    scanf("%d", &msg.name);
    printf("input your password: ");
    scanf("%s", msg.data);
    write(fd, &msg, sizeof(msg));
    read(fd, &msg_back, sizeof(msg_back));
    if (msg_back.state == USER_NOT_REGIST) // 用户未注册
    {
        printf("Username not register\n");
        return -1;
    }
    else if (msg_back.state == PASSWORD_WRONG) // 密码错误
    {
        printf("password is wrong\n");
        return -1;
    }
    else if (msg_back.state == USER_LOGED) // 用户已上线
    {
        printf("User has logined\n");
        return -1;
    }
    else
    {
        printf("login success\n");
        login_f = 1;
        return 0;
    }
}
// 账号下线（基本都会成功，所以无需返回值和检查）
static void logout()
{
    msg.cmd = LOGOUT;
    write(fd, &msg, sizeof(msg));
    close(fd);
}
// 更新服务端的聊天状态
static void set_flag()
{
    msg.cmd = STARTCHAT;
    write(fd, &msg, sizeof(msg)); // 更新服务端的聊天状态
}
// 公聊
static void global_chat()
{
    msg.cmd = BROADCAST;
    system("clear"); // 清屏
    printf("\t\t公共聊天室（输入-1退出）\n");
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    while (1)
    {
        scanf("%s", msg.data);
        write(fd, &msg, sizeof(msg)); // 如果发送-1，服务器会更新聊天状态并发送用户退出消息给其他用户
        if (strcmp(msg.data, "-1") == 0)
            break;
    }
    // 退出聊天室需要更新聊天状态、重置聊天消息的行数并清屏
    chat_flag = 0;
    message_line = 2;
    system("clear");
}
// 列出在线用户及其聊天状态
static void online_user_status()
{
    // 发送请求
    msg.cmd = ONLINEUSER;
    write(fd, &msg, sizeof(msg));
    // 注意：这里要使用msg_back来接收服务端的数据，因为不能更改msg的内容
    // 服务端每次msg是一个在线用户，客户端循环接收，直到cmd为ONLINEUSER_OVER表示没有在线用户了
    while (read(fd, &msg_back, sizeof(msg_back)) != -1 && msg_back.state != ONLINEUSER_OVER)
        printf("%s\n", msg_back.data);
    printf("列举结束\n");
}
static void private()
{
    // 先列出在线用户及其聊天状态
    online_user_status();
    // 用户选择要聊天的对象
    printf("请选择要私聊的用户名(输入自己表示返回)：");
    scanf("%s", msg.data);
    if (msg.name == atoi(msg.data))
        return;
    msg.cmd = CHOOSE;
    write(fd, &msg, sizeof(msg));
    
    // 聊天状态
    chat_flag = 2;
    msg.cmd = PRIVATE;
    system("clear"); // 清屏
    printf("\t\t私人聊天室（输入-1退出）\n");
    // 唤醒实时接收信息的子线程
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    while (1)
    {
        scanf("%s", msg.data);
        write(fd, &msg, sizeof(msg)); // 如果发送-1，服务器接收到后会更新聊天状态
        if (strcmp(msg.data, "-1") == 0)
            break;
    }
    // 退出聊天室需要更新聊天状态、重置聊天消息的行数并清屏
    chat_flag = 0;
    message_line = 2;
    system("clear");
}
