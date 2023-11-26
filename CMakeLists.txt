cmake_minimum_required(VERSION 3.0)
project(backup_tool)

set(CMAKE_CXX_STANDARD 11)





# 客户端程序
add_executable(client client.cpp)

# 添加链接库，包括 pthread
find_package(Threads REQUIRED)
target_link_libraries(client PRIVATE Threads::Threads)
target_compile_options(client PRIVATE -pthread)

# 服务器端程序
add_executable(server server.cpp)
