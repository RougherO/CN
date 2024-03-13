#include <array>

#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "Logger.hpp"
#define UDP
#include "Global.hpp"

int32_t main(int32_t argc, char** argv)
{
    std::array<char, _BUF_SIZE_> buffer;
    buffer.fill(0);

    if (argc != 3) {
        LogToStdErrAndTerminate(std::string("Usage: ") + argv[0] + " <IP> <PORT>");
    }

    int32_t server_socket = socket(_SOCK_ADDR_TYPE_, _SOCK_PROTO_TYPE_, 0);

    sockaddr_in server_address;
    server_address.sin_family = _SOCK_ADDR_TYPE_;
    inet_pton(_SOCK_ADDR_TYPE_, argv[1], &server_address.sin_addr);
    server_address.sin_port = htons(std::stoul(argv[2]));

    if (bind(server_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == -1) {
        LogToStdErrAndTerminate("Could not bind server to the given address");
    }

    for (size_t numOfBytes;;) {
        numOfBytes = recvfrom(server_socket, buffer.data(), _BUF_SIZE_, 0, nullptr, nullptr);
        if (numOfBytes == -1) {
            LogToStdErrAndTerminate("Could not receive complete message");
        } else if (numOfBytes == 0) {
            break;
        }
        LogToStdOut("Received " + std::to_string(numOfBytes) + " bytes from peer");
        LogToStdOut(buffer.data(), numOfBytes);
    }

    close(server_socket);
}