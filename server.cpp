#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define SERVER_PORT 8888
#define BUFFER_SIZE 4096

int main() {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    char buffer[BUFFER_SIZE];

    // 创建 TCP 服务器套接字
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        std::cerr << "Error creating socket" << std::endl;
        return -1;
    }

    // 设置服务器地址和端口
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // 绑定套接字
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Error binding" << std::endl;
        return -1;
    }

    // 监听连接
    if (listen(serverSocket, 5) == -1) {
        std::cerr << "Error listening" << std::endl;
        return -1;
    }

    std::cout << "Waiting for incoming connections..." << std::endl;

    // 接受客户端连接
    if ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen)) == -1) {
        std::cerr << "Error accepting connection" << std::endl;
        return -1;
    }

    std::cout << "Client connected" << std::endl;

    // 创建一个合并后的分区文件
    std::ofstream outputFile("full_partition_backup.dd", std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "Error creating output file" << std::endl;
        return -1;
    }

    // 持续接收并写入文件，直到客户端结束发送
    while (true) {
        ssize_t bytesRead = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesRead <= 0) {
            break;
        }
        outputFile.write(buffer, bytesRead);
    }

    // 关闭连接和文件
    close(clientSocket);
    outputFile.close();
    close(serverSocket);

    std::cout << "Received and merged backup files into full_partition_backup.dd" << std::endl;

    return 0;
}
