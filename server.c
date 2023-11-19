#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> // POSIX线程库，由于该库不是Linux默认库，所以要在编译时加-lpthread参数才能完成链接

#define SERVER_PORT 9527 // 服务器端口号

void sys_err(const char *str)
{
    perror(str);
    exit(1);
}
void *thread(void *arg) // 子线程操作程序
{

    // 处理程序
    char buf[BUFSIZ];           // 默认缓冲区，大小是8192
    int new_fd = *((int *)arg); // 先把void指针转换为int指针，然后再解引用
    free(arg);                  // 子线程获取到套接字的值之后就可以删掉堆区内存
    while (1)
    {
        int ret = read(new_fd, buf, sizeof(buf)); // read返回读到的字节大小
        if (ret == -1)                            // 读取出错
        {
            perror("read error");
            break;
        }
        if (ret == 0) // 客户端关闭连接
        {
            printf("客户端关闭连接\n");
            break;
        }
        write(1, buf, ret); // 读取到数据就打印到标准输出并写回给客户端
    }
    close(new_fd); // 出错或者关闭连接之后会跳出循环执行这句
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