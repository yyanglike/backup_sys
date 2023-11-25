cmake_minimum_required(VERSION 3.0)
project(backup_tool)

set(CMAKE_CXX_STANDARD 11)

# 客户端程序
add_executable(client client.cpp)

# 服务器端程序
add_executable(server server.cpp)
