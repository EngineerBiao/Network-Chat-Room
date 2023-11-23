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

int registe(); // 注册
int login(); // 登录
void logout(); // 下线  
void broadcast(); // 广播
void private(); // 私聊
void list_online_user(); // 列出在线用户


int fd; // 由于其他函数也要用该文件描述符，所以直接定义为全局变量
int login_f = -1; // -1表示未登录，1表示已登录
// 由于只有一个线程，所以msg直接设为文件作用域
struct Message msg, msg_back; // msg是message的简写，msg_back用来获取服务端发送回的消息

void sys_err(const char *str)
{
    perror(str);
    exit(1);
}
void *readThread(void *arg) // 子线程接收服务端的数据
{
    char buf[BUFSIZ];       // 默认缓冲区，大小是8192
    while (1)
    {
        if (login_f == -1) // 只有登录后才接收数据，这里使用轮询的方法（看后期能不能改进效率）
            continue;
        int ret = read(fd, buf, sizeof(buf)); // read返回读到的字节大小
        if (ret == -1)
            sys_err("read error");
        write(1, buf, ret); // 打印读取到的数据到标准输出
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
    
    // 创建子线程
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
        scanf("%d", &num);
        if (num == 0)
        {
            if (login_f == -1) // 非登录状态就下直接退出程序
                exit(0);
            else // 登录状态下先下线然后再退出
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
                ret = login();
                break;
            case 3:
                broadcast();
                break;
            case 4:
                private();
                break;
            case 5:
                list_online_user();
                break;
            default:    
                break;
        }
    }
    
    // 循环读取键盘输入
    // char buf[BUFSIZ]; // BUFSIZ是标准缓冲区大小
    // char cha;         // 用来接收键盘的字符输入
    // while (1)
    // {
        
    //     int i = 0;
    //     printf("input message: ");
    //     while (scanf("%c", &cha)) // 循环读取字符
    //     {
    //         buf[i++] = cha;
    //         if (cha == '\n') // 遇到换行符就写入数据
    //             break;
    //     }
    //     if (write(fd, buf, strlen(buf)) == -1)
    //     {
    //         close(fd);
    //         sys_err("write error");
    //     }
    //     memset(buf, '\0', i); // 每次写完后要清空buf缓冲区
    // }
    close(fd);
    return 0;
}
// 注册请求函数
int registe()
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
int login()
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
// 账号下线，基本都会成功，所以无需返回值或者检查
void logout()
{
    msg.cmd = LOGOUT;
    write(fd, &msg, sizeof(msg));
    close(fd);
}
void broadcast()
{
    
}
void private()
{
    
}
void list_online_user()
{
    
}
