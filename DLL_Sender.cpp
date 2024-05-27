#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <arpa/inet.h>
#include <unistd.h>

#define FRAME_SIZE   1024
#define TOTAL_FRAMES 10

void WaitForEvent(int frame_number);
void GetData(char* data, int frame_number);
void MakeFrame(char* data, char* frame);
void SendFrame(int sockfd, char* frame);

void WaitForEvent(int frame_number)
{
    int wait_time = 500 + std::rand() % 1000;
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
}

void GetData(char* data, int frame_number)
{
    snprintf(data, FRAME_SIZE, "Frame %d", frame_number);
}

void MakeFrame(char* data, char* frame)
{
    strncpy(frame, data, FRAME_SIZE);
    frame[FRAME_SIZE - 1] = '\0';   // Ensure null-termination
}

void SendFrame(int sockfd, char* frame)
{
    send(sockfd, frame, FRAME_SIZE, 0);
    std::cout << "Sending frame: " << frame << std::endl;
}

void Sender(char const* ip, int port)
{
    int sockfd;
    struct sockaddr_in servaddr;

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

    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        std::cerr << "Connection to the server failed." << std::endl;
        exit(EXIT_FAILURE);
    }

    char data[FRAME_SIZE];
    char frame[FRAME_SIZE];

    for (int i = 0; i < TOTAL_FRAMES; ++i) {
        GetData(data, i);
        MakeFrame(data, frame);
        SendFrame(sockfd, frame);
        WaitForEvent(i);
    }

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

    Sender(ip, port);
    return 0;
}
