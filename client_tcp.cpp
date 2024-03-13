#include <array>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Logger.hpp"

#define TCP
#include "Global.hpp"

void reader(int32_t client_socket)
{
    std::array<char, _BUF_SIZE_> read_buffer;
    for (int32_t numOfBytes;;) {
        numOfBytes = recv(client_socket, read_buffer.data(), _BUF_SIZE_, 0);
        if (numOfBytes == -1) {
            LogToStdErrAndTerminate("Could not receive complete message");
        } else if (numOfBytes == 0) {
            break;
        }
        LogToStdOut("Message from server: ", "");
        LogToStdOut(read_buffer.data(), numOfBytes);
    }
}

void writer(int32_t client_socket)
{
    std::array<char, _BUF_SIZE_> write_buffer;
    for (int32_t numOfBytes { -1 };;) {
        numOfBytes = LogFromStdIn(write_buffer.data(), _BUF_SIZE_);
        numOfBytes = send(client_socket, write_buffer.data(), numOfBytes, 0);
        if (numOfBytes == -1) {
            LogToStdErrAndTerminate("Could not send message");
        }
    }
}

int32_t main(int32_t argc, char** argv)
{

    if (argc != 3) {
        LogToStdErrAndTerminate(std::string("Usage: ") + argv[0] + " <IP> <PORT>");
    }

    int32_t client_socket = socket(_SOCK_ADDR_TYPE_, _SOCK_PROTO_TYPE_, 0);

    sockaddr_in server_address;
    server_address.sin_family = _SOCK_ADDR_TYPE_;
    inet_pton(_SOCK_ADDR_TYPE_, argv[1], &server_address.sin_addr);   // server_address.sin_addr.s_addr = inet_addr(argv[1]);
    server_address.sin_port = htons(std::stoul(argv[2]));

    if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == -1) {
        LogToStdErrAndTerminate(std::string("Could not connect to server with IP: ") + argv[1] + " and PORT: " + argv[2]);
    }

    std::thread recv_thread(reader, client_socket);
    std::thread send_thread(writer, client_socket);

    recv_thread.join();
    send_thread.join();

    close(client_socket);
}