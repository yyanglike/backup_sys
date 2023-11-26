#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <string>

#define SERVER_PORT 8888
#define BUFFER_SIZE 4194304  // 4MB

#define NUMBERLOOPS 1000

std::queue<std::pair<char*, size_t>> dataQueue;
std::mutex dataMutex;
std::condition_variable dataCV;

void sendToServer(int clientSocket) {
    while (true) {
        std::unique_lock<std::mutex> lock(dataMutex);
        dataCV.wait(lock, [] { return !dataQueue.empty(); });

        auto dataPair = dataQueue.front();
        dataQueue.pop();
        lock.unlock();

        char *data = dataPair.first;
        size_t dataSize = dataPair.second;

        // Send data to server
        send(clientSocket, data, dataSize, 0);

        delete[] data; // Free allocated memory
    }
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <disk_partition_path> <server_ip> <server_port>" << std::endl;
        return -1;
    }

    const char *diskPath = argv[1];
    const char *serverIP = argv[2];
    int serverPort = std::atoi(argv[3]);

    // Get total sector count of the disk partition
    // FILE *diskInfo = popen(("fdisk -l " + std::string(diskPath) + " | grep 'sectors' | awk '{print $7}'").c_str(), "r");
    // if (!diskInfo) {
    //     std::cerr << "Error getting disk information" << std::endl;
    //     return -1;
    // }


    // long long int totalSectors;
    // fscanf(diskInfo, "%lld", &totalSectors);
    // pclose(diskInfo);

    //从/dev/datto0下面获取datto0

    std::string device = "sda";  // replace with your device name


    std::string devicePath(diskPath);
    std::string prefix = "/dev/";

    // Check if the devicePath starts with the prefix
    if (devicePath.substr(0, prefix.size()) == prefix) {
        // Get the part after the prefix
        std::string deviceName = devicePath.substr(prefix.size());

        std::cout << "Device name: " << deviceName << std::endl;  // Output: "datto0"
        device = deviceName;
    } else {
        std::cerr << "Device path does not start with the expected prefix." << std::endl;
    }

    std::ifstream file("/sys/block/" + device + "/size");
    if (!file.is_open()) {
        std::cerr << "Failed to open size file for device " << device << "\n";
        return 1;
    }

    long long int totalSectors;
    file >> totalSectors;
    file.close();


    std::cout << "tatal sectors:" << totalSectors << std::endl;

    int clientSocket;
    struct sockaddr_in serverAddr;

    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        std::cerr << "Error creating socket" << std::endl;
        return -1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);

    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Error connecting to server" << std::endl;
        return -1;
    }

    // Start sending thread
    std::thread senderThread(sendToServer, clientSocket);

     // Read disk and send data block by block (each block of 4MB)
    long long int sectorsPerBlock = ((totalSectors+NUMBERLOOPS -1) / NUMBERLOOPS) ;  // 4MB in sectors
    std::cout << "SectorsPerBlock is :" << sectorsPerBlock << std::endl;
    int blockCount = 0;
    while (blockCount < NUMBERLOOPS) {
        char command[100];
        sprintf(command, "dd if=%s bs=512 skip=%lld count=%lld", diskPath, blockCount * sectorsPerBlock, sectorsPerBlock);
        std::cout << "Command: " << command << std::endl;  // Print the command
        FILE *pipe = popen(command, "r");
        if (!pipe) {
            std::cerr << "Error executing dd command" << std::endl;
            return -1;
        }

        while (true) {
            char *data = new char[BUFFER_SIZE];
            size_t totalRead = 0;
            size_t bytesRead;
            while (totalRead < BUFFER_SIZE && (bytesRead = fread(data + totalRead, 1, BUFFER_SIZE - totalRead, pipe)) > 0) {
                totalRead += bytesRead;
            }

            if (totalRead == 0) {
                delete[] data;
                break;
            }

            std::unique_lock<std::mutex> lock(dataMutex);
            dataQueue.push(std::make_pair(data, totalRead));
            dataCV.notify_one();
        }

        // Close file after reading and before next iteration
        pclose(pipe);

        ++blockCount;

        // Wait until data is sent before reading next block
        while (!dataQueue.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    senderThread.join();
    close(clientSocket);

    return 0;
}
