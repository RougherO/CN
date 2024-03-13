#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Logger.hpp"

#define TCP
#include "Global.hpp"

int32_t main(int32_t argc, char** argv)
{
    if (argc != 4) {
        LogToStdErrAndTerminate(std::string("Usage: ") + argv[0] + " <IP> <PORT> <MESSAGE>");
    }

    int32_t client_socket = socket(_SOCK_ADDR_TYPE_, _SOCK_PROTO_TYPE_, 0);

    sockaddr_in server_address;
    server_address.sin_family = _SOCK_ADDR_TYPE_;
    inet_pton(_SOCK_ADDR_TYPE_, argv[1], &server_address.sin_addr);   // server_address.sin_addr.s_addr = inet_addr(argv[1]);
    server_address.sin_port = htons(std::stoul(argv[2]));

    if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == -1) {
        LogToStdErrAndTerminate(std::string("Could not connect to server with IP: ") + argv[1] + " and PORT: " + argv[2]);
    }

    size_t sizeOfMessage = std::strlen(argv[3]);
    int32_t numOfBytes   = send(client_socket, argv[3], sizeOfMessage, 0);

    if (numOfBytes == -1) {
        LogToStdErrAndTerminate("Could not send message");
    }

    LogToStdOut("Sent " + std::to_string(numOfBytes) + " bytes of data to server");
    close(client_socket);
}