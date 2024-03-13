#include <algorithm>
#include <cstring>
#include <string>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "Logger.hpp"
#define UDP
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

    size_t sizeOfMessage = std::strlen(argv[3]);
    for (size_t toSend {}; toSend < sizeOfMessage; toSend += _BUF_SIZE_) {
        int32_t numOfBytes = sendto(client_socket, argv[3] + toSend, std::min(_BUF_SIZE_, sizeOfMessage - toSend), 0, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address));
        if (numOfBytes == -1) {
            LogToStdErrAndTerminate("Could not send message");
        }
        LogToStdOut("Sent " + std::to_string(numOfBytes) + " bytes of data to server");
    }
    close(client_socket);
}