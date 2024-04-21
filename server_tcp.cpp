#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <array>
#include <map>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>

template <int32_t Domain, int32_t Protocol = 0>
class TCPServer {
public:
    int32_t const domain = Domain;

    TCPServer(char const* ip_addr, uint16_t port_num, uint16_t listeners) noexcept
        : m_ACC_SOCK { socket(Domain, SOCK_STREAM, Protocol) }
    {
        if (m_ACC_SOCK == -1) {
            std::fprintf(stderr, "Could not create socket for TCPServer\n");
            std::fflush(stderr);
            return;
        }

        m_TCPServer_address.sin_family = Domain;
        inet_pton(Domain, ip_addr, &m_TCPServer_address.sin_addr);
        m_TCPServer_address.sin_port = htons(port_num);

        if (bind(m_ACC_SOCK, reinterpret_cast<sockaddr*>(&m_TCPServer_address), sizeof(m_TCPServer_address)) == -1) {
            std::fprintf(stderr, "Could not bind TCPServer to the given address\n");
            std::fflush(stderr);
            return;
        }

        if (listen(m_ACC_SOCK, listeners) == -1) {
            std::fprintf(stderr, "Could not listen on given PORT and IP\n");
            std::fflush(stderr);
            return;
        }

        m_pollfd_set[FD_INDEX::STDIN]  = { STDIN_FILENO, POLLIN };
        m_pollfd_set[FD_INDEX::ACCEPT] = { m_ACC_SOCK, POLLIN };

        std::fprintf(stdout, "TCPServer listening on %s:%d\n", ip_addr, port_num);
    }

    TCPServer(TCPServer const&)             = delete;
    TCPServer& operator=(TCPServer const&)  = delete;
    TCPServer(TCPServer const&&)            = delete;
    TCPServer& operator=(TCPServer const&&) = delete;

    void start(int32_t tout_in_mill = 1000) noexcept
    {
        for (int32_t poll_stat; !m_close_conn;) {
            poll_stat = poll(m_pollfd_set.data(), 2 + m_conns, tout_in_mill);
            if (poll_stat == -1) {
                std::fprintf(stderr, "Polling failed\n");
                std::fflush(stderr);
                return;
            }
            if (poll_stat > 0) {
                if (m_pollfd_set[FD_INDEX::ACCEPT].revents & POLLIN) {
                    m_accept_conn() ? m_broadcast(FD_INDEX::CLIENTS + m_conns - 1) : void();
                    continue;
                }
                if (m_pollfd_set[FD_INDEX::STDIN].revents & POLLIN) {
                    int32_t msg_len         = read(m_pollfd_set[FD_INDEX::STDIN].fd, m_write_buffer.data(), s_MAX_BUFFER_SIZE);
                    m_write_buffer[msg_len] = 0;
                    if (m_write_buffer[0] == '0') {
                        std::fputs("Shutting Down TCPServer\n", stdout);
                        std::fflush(stdout);
                        m_close_conn = true;
                        break;
                    }
                    m_broadcast(0);
                    continue;
                }
                for (auto* client_ptr = m_pollfd_set.begin() + FD_INDEX::CLIENTS; client_ptr < m_pollfd_set.begin() + FD_INDEX::CLIENTS + m_conns; ++client_ptr) {
                    if (client_ptr->revents & POLLIN) {
                        m_read_client(client_ptr - m_pollfd_set.begin()) ? m_broadcast(client_ptr - m_pollfd_set.begin()) : void();
                    }
                }
            }
        }
    }

    ~TCPServer() noexcept
    {
        std::for_each_n(m_pollfd_set.begin(), m_conns + FD_INDEX::CLIENTS, [](pollfd const& poll_fd) {
            close(poll_fd.fd);
        });
    }

private:
    bool m_accept_conn() noexcept
    {
        if (m_conns < s_MAX_CONNS) {
            int32_t client_sock = accept(m_ACC_SOCK, nullptr, nullptr);
            if (client_sock == -1) {
                std::fprintf(stderr, "Could not accept connection\n");
                std::fflush(stderr);
                return false;
            }
            int32_t uname_size = recv(client_sock, m_read_buffer.data(), s_MAX_BUFFER_SIZE, 0);
            if (uname_size == -1) {
                std::fprintf(stderr, "Could not receive username");
                std::fflush(stderr);
            }
            int32_t msg_len         = std::sprintf(m_write_buffer.data(), "[%.*s] connected\n", uname_size, m_read_buffer.data());
            m_write_buffer[msg_len] = 0;

            m_client_uname_sock_map[client_sock].assign(m_read_buffer.data(), uname_size);
            m_pollfd_set[FD_INDEX::CLIENTS + m_conns].fd     = client_sock;
            m_pollfd_set[FD_INDEX::CLIENTS + m_conns].events = POLLIN;
            m_conns++;

            std::fputs(m_write_buffer.data(), stdout);
            std::fflush(stdout);
            return true;
        }
        std::fprintf(stdout, "Maximum number of connections reached\n");
        return false;
    }

    bool m_read_client(int32_t client_index) noexcept
    {
        int32_t read_bytes = recv(m_pollfd_set[client_index].fd, m_read_buffer.data(), m_read_buffer.size(), 0);
        if (read_bytes == -1) {
            std::fprintf(stderr, "Could not receive complete message\n");
            std::fflush(stderr);
            return false;
        }
        if (read_bytes == 0) {
            int32_t msg_len         = std::sprintf(m_write_buffer.data(), "[%s] disconnected\n", m_client_uname_sock_map[m_pollfd_set[client_index].fd].c_str());
            m_write_buffer[msg_len] = 0;

            m_close_client(client_index);

            std::fputs(m_write_buffer.data(), stdout);
            std::fflush(stdout);

            return true;
        }
        int32_t msg_len = std::sprintf(
            m_write_buffer.data(),
            "Message from [%s]: %.*s", m_client_uname_sock_map[m_pollfd_set[client_index].fd].c_str(), read_bytes, m_read_buffer.data());
        m_write_buffer[msg_len] = 0;
        return true;
    }

    inline void m_close_client(int32_t client_index) noexcept
    {
        close(m_pollfd_set[client_index].fd);
        m_pollfd_set[client_index].fd = m_pollfd_set[FD_INDEX::CLIENTS + m_conns - 1].fd;
        m_conns--;
    }

    void m_broadcast(int32_t client_index) noexcept
    {
        size_t msg_len = std::strlen(m_write_buffer.data());
        std::for_each_n(m_pollfd_set.begin() + FD_INDEX::CLIENTS, m_conns, [=, index = int { FD_INDEX::CLIENTS }](pollfd const& poll_fd) mutable {
            if (index != client_index) {
                int32_t send_bytes = send(m_pollfd_set[index].fd, m_write_buffer.data(), msg_len, 0);
                if (send_bytes == -1) {
                    std::printf("client index: %d, index: %d", client_index, index);
                    std::fputs("Could not send message\n", stderr);
                    std::fflush(stderr);
                }
            }
            index += 1;
        });
    }

public:
    static constexpr uint8_t s_MAX_CONNS { 50 };
    static constexpr uint16_t s_MAX_BUFFER_SIZE { 1024 };
    static constexpr uint16_t s_EXTRA_BUFFER_SIZE { 256 };

private:
    enum FD_INDEX {
        STDIN,
        ACCEPT,
        CLIENTS
    };

    int32_t const m_ACC_SOCK;
    bool m_close_conn {};
    uint8_t m_conns {};
    sockaddr_in m_TCPServer_address;
    std::array<char, s_MAX_BUFFER_SIZE + 1 + s_EXTRA_BUFFER_SIZE> m_read_buffer;
    std::array<char, s_MAX_BUFFER_SIZE + 1 + s_EXTRA_BUFFER_SIZE> m_write_buffer;
    std::array<pollfd, 2 + s_MAX_CONNS> m_pollfd_set;

    std::map<uint16_t, std::string> m_client_uname_sock_map;
};

auto main(int32_t argc, char** argv) -> int32_t
{
    if (argc != 4) {
        std::fprintf(stderr, "Usage: %s <IP> <PORT> <LISTENERS>", argv[0]);
        std::exit(64);
    }

    TCPServer<AF_INET> server {
        argv[1],
        static_cast<uint16_t>(std::stoul(argv[2])),
        static_cast<uint16_t>(std::stoul(argv[3]))
    };

    server.start();
}
