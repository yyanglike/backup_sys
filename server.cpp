#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#define SERVER_PORT 8888
#define BUFFER_SIZE 30*1024*1024


std::queue<std::pair<char*, size_t>> dataQueue;
std::mutex dataMutex;
std::condition_variable dataCV;

void writeToFile() {
    const size_t GB = 1024 * 1024 * 1024;
    const size_t MAX_SIZE = 100 * GB;
    size_t currentFileSize = 0;
    int fileIndex = 0;
    std::ofstream outputFile("full_partition_backup" + std::to_string(fileIndex) + ".dd", std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "Error creating output file" << std::endl;
        exit(-1);
    }

    while (true) {
        std::unique_lock<std::mutex> lock(dataMutex);
        dataCV.wait(lock, [] { return !dataQueue.empty(); });

        auto dataPair = dataQueue.front();
        dataQueue.pop();
        lock.unlock();
        
        char* data = dataPair.first;
        size_t dataSize = dataPair.second;

        // Check if the data size exceeds the limit of the current file
        if (currentFileSize + dataSize > MAX_SIZE) {
            // Close the current file and open a new one
            outputFile.close();
            outputFile.open("full_partition_backup" + std::to_string(++fileIndex) + ".dd", std::ios::binary);
            if (!outputFile.is_open()) {
                std::cerr << "Error creating output file" << std::endl;
                exit(-1);
            }
            currentFileSize = 0;
        }

        outputFile.write(data, dataSize);
        currentFileSize += dataSize;
        delete[] data; // Free allocated memory
    }

    outputFile.close();
}


int main() {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    char* buffer = new char[BUFFER_SIZE];

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

    // Start writer thread
    std::thread writerThread(writeToFile);

    // ... skipping to the part where we receive data ...

    while (true) {
        char *buffer = new char[BUFFER_SIZE];
        ssize_t bytesRead = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesRead <= 0) {
            delete[] buffer;
            break;
        }

        std::unique_lock<std::mutex> lock(dataMutex);
        dataQueue.push(std::make_pair(buffer, bytesRead));
        dataCV.notify_one();
    }

    writerThread.join();

    // 关闭连接和文件
    close(clientSocket);
    close(serverSocket);
    std::cout << "Received and merged backup files into full_partition_backup.dd" << std::endl;

    return 0;
}
