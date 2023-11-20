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

void sys_err(const char *str)
{
    perror(str);
    exit(1);
}
void *readThread(void *arg) // 子线程接收服务端的数据
{
    char buf[BUFSIZ];       // 默认缓冲区，大小是8192
    int fd = *((int *)arg); // 先把void指针转换为int指针，然后再解引用
    free(arg);              // 子线程获取到套接字的值之后就可以删掉堆区内存
    while (1)
    {
        int ret = read(fd, buf, sizeof(buf)); // read返回读到的字节大小
        if (ret == -1)
            sys_err("read error");
        write(1, buf, ret); // 打印读取到的数据到标准输出
    }
}
int main()
{
    int fd, ret;
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
    if (pthread_create(&tid, NULL, readThread, (void *)arg_fd) != 0)
        sys_err("thread create error");
    pthread_detach(tid); // 线程分离
    
    // 循环读取键盘输入
    char buf[BUFSIZ]; // BUFSIZ是标准缓冲区大小
    char cha;         // 用来接收键盘的字符输入
    while (1)
    {
        
        int i = 0;
        printf("input message: ");
        while (scanf("%c", &cha)) // 循环读取字符
        {
            buf[i++] = cha;
            if (cha == '\n') // 遇到换行符就写入数据
                break;
        }
        if (write(fd, buf, strlen(buf)) == -1)
        {
            close(fd);
            sys_err("write error");
        }
        memset(buf, '\0', i); // 每次写完后要清空buf缓冲区
    }
    close(fd);
    return 0;
}