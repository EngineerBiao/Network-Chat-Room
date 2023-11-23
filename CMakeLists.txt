cmake_minimum_required(VERSION 3.0)
project(NetworkChatRoom)
#指定头文件路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
#指定生成的二进制可执行文件目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

#编译生成目标文件
set(SRC_SERVER ${CMAKE_CURRENT_SOURCE_DIR}/src/server.c ${CMAKE_CURRENT_SOURCE_DIR}/src/data.c)
set(SRC_CLIENT ${CMAKE_CURRENT_SOURCE_DIR}/src/client.c ${CMAKE_CURRENT_SOURCE_DIR}/src/data.c)

#添加编译参数，用到线程库
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
#生成可执行文件的目标
add_executable(server ${SRC_SERVER})
add_executable(client ${SRC_CLIENT})
#将生成的可执行文件与sqlite3库进行链接
target_link_libraries(server PRIVATE -lsqlite3)
target_link_libraries(client PRIVATE -lsqlite3)

