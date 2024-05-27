#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define FRAME_SIZE 1024

void WaitForEvent();
void ReceiveFrame(int connfd, char* frame);
void ExtractData(char* frame, char* data);
void DeliverData(char* data);

void WaitForEvent()
{
    std::cout << "Waiting for frame..." << std::endl;
    int wait_time = 500 + std::rand() % 1500;
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
}

void ReceiveFrame(int connfd, char* frame)
{
    int n = read(connfd, frame, FRAME_SIZE);
    if (n < 0) {
        std::cerr << "Error in receiving frame" << std::endl;
        exit(EXIT_FAILURE);
    }
    frame[n] = 0;
}

void ExtractData(char* frame, char* data)
{
    strncpy(data, frame, FRAME_SIZE);
}

void DeliverData(char* data)
{
    std::cout << "Received frame: " << data << std::endl;
}

void Receiver(char const* ip, int port)
{
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        std::cerr << "Socket creation failed." << std::endl;
        exit(EXIT_FAILURE);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(port);

    if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
        std::cerr << "Invalid address / Address not supported" << std::endl;
        exit(EXIT_FAILURE);
    }

    if ((bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0) {
        std::cerr << "Socket bind failed." << std::endl;
        exit(EXIT_FAILURE);
    }

    if ((listen(sockfd, 1)) != 0) {
        std::cerr << "Listen failed." << std::endl;
        exit(EXIT_FAILURE);
    } else {
        std::cout << "Listening for connections..." << std::endl;
    }

    socklen_t len = sizeof(cli);

    connfd = accept(sockfd, (struct sockaddr*)&cli, &len);
    if (connfd < 0) {
        std::cerr << "Server accept failed." << std::endl;
        exit(EXIT_FAILURE);
    } else {
        std::cout << "Connection established." << std::endl;
    }

    char frame[FRAME_SIZE];
    char data[FRAME_SIZE];

    while (true) {
        WaitForEvent();
        ReceiveFrame(connfd, frame);
        if (!frame[0]) break;
        ExtractData(frame, data);
        DeliverData(data);
    }

    close(connfd);
    close(sockfd);
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <IP> <Port>" << std::endl;
        return EXIT_FAILURE;
    }

    char const* ip = argv[1];
    int port       = std::stoi(argv[2]);

    Receiver(ip, port);
    return 0;
}
