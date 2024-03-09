# 基于socket的网络聊天室

注：该项目有两个版本，easy分支是第一个版本，easy版本的服务器模型是普通的多线程模型，主线程每连接一个客户端就创建一个线程来处理通信。master分支是第二个版本，master版本的服务器模型是基于事件驱动的Reactor主从线程版本，具体的模型介绍看master版本的项目说明文档。（两个版本的README.md和说明文档.md是不同的）

## 项目简介

一个在Linux终端上运行的基于C语言的网络聊天室，服务器采用多线程模型，采用SQLite数据库存储信息。
主要功能有注册登录、公共聊天室和用户私聊。涉及的技术有C语言、Socket编程、多线程编程、条件变量和锁、SQLite数据库、CMake。
项目说明.md那里有对技术选型、项目功能、功能实现的详细解释。

## 快速开始

### 环境要求

安装了SQLite数据库、CMake的Linux系统

### 安装步骤

1. 安装CMake和SQLite3

```
sudo apt install cmake
sudo apt install sqlite3
```

2. 复制工程，进入build目录

```
git clone git@github.com:EngineerBiao/Network-Chat-Room.git
cd <your project>/build
```

3. 编译工程

```
cmake ..
make
```

### 简单的使用示例

服务端程序和客户端程序都在工程的bin文件中，进入bin文件运行server，然后再运行多个client，多个client经过登录注册后就可以使用公聊和私聊功能。

1. 进入bin文件运行服务端程序server，如下图所示

![image-20231215223851018](https://biao-tu.oss-cn-shenzhen.aliyuncs.com/images/202312152238040.png)

2. 再创建两个终端，分别运行客户端程序client，然后进行注册登录，如下图所示

![image-20231215223723470](https://biao-tu.oss-cn-shenzhen.aliyuncs.com/images/202312152237542.png)

3. 选择公聊模式，进入公聊聊天室，可以看到互相发送的内容，如下图所示

![image-20231215224138342](https://biao-tu.oss-cn-shenzhen.aliyuncs.com/images/202312152241369.png)

4. 如果要私聊，双方都选择私聊模式并输入对方的用户名即可开始私聊，如下图

![image-20231215224553576](https://biao-tu.oss-cn-shenzhen.aliyuncs.com/images/202312152245601.png)

![image-20231215224629944](https://biao-tu.oss-cn-shenzhen.aliyuncs.com/images/202312152246965.png)