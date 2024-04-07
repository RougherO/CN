#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <array>
#include <thread>
#include <unordered_map>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

static const int32_t stdin_fd  = fileno(stdin);
static const int32_t stdout_fd = fileno(stdout);
static const int32_t stderr_fd = fileno(stderr);

template <int32_t Domain, int32_t Type, int32_t Proto = 0>
class Server {
public:
    int32_t const domain   = Domain;
    int32_t const type     = Type;
    int32_t const protocol = Proto;

    Server(char const* ip_addr, uint16_t port_num, uint16_t listeners) noexcept
        : m_accept_socket { socket(Domain, Type, Proto) }
    {
        if (m_accept_socket == -1) {
            std::fprintf(stderr, "Could not create socket for server\n");
            std::fflush(stderr);
            // Hanlde error and return
            return;
        }

        int32_t flags = fcntl(m_accept_socket, F_GETFL);
        if (flags == -1) {
            std::fprintf(stderr, "Could not get socket flags for server\n");
            std::fflush(stderr);
            // Handle error and return
            return;
        }
        if (fcntl(m_accept_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
            std::fprintf(stderr, "Could not set socket flags for server\n");
            std::fflush(stderr);
            // Handle error and return
            return;
        }

        m_server_address.sin_family = Domain;
        inet_pton(Domain, ip_addr, &m_server_address.sin_addr);
        m_server_address.sin_port = htons(port_num);

        if (bind(m_accept_socket, reinterpret_cast<sockaddr*>(&m_server_address), sizeof(m_server_address)) == -1) {
            std::fprintf(stderr, "Could not bind server to the given address\n");
            std::fflush(stderr);
            // Handle error and return
            return;
        }

        if (listen(m_accept_socket, listeners) == -1) {
            std::fprintf(stderr, "Could not listen on given PORT and IP\n");
            std::fflush(stderr);
            // Handle error and return
            return;
        }

        std::fprintf(stdout, "Server listening on %s:%d\n", ip_addr, port_num);
    }

    Server(Server const&)             = delete;
    Server& operator=(Server const&)  = delete;
    Server(Server const&&)            = delete;
    Server& operator=(Server const&&) = delete;

    void accept_conn() noexcept
    {
        for (int32_t client_sock; m_conns < s_max_conns;) {
            client_sock = accept(m_accept_socket, nullptr, nullptr);
            if (client_sock == -1) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    break;
                } else {
                    std::fprintf(stderr, "Could not accept connection\n");
                    std::fflush(stderr);
                    return;
                }
            }
            for (int32_t uname_size;;) {
                uname_size = recv(client_sock, m_read_buffer.data(), s_max_buffer_size, 0);
                if (uname_size == -1) {
                    if (errno != EWOULDBLOCK && errno != EAGAIN) {
                        std::fprintf(stderr, "Could not receive username");
                        std::fflush(stderr);
                    }
                    continue;
                }
                std::fprintf(stdout, "[%.*s] connected\n", uname_size, m_read_buffer.data());
                m_client_uname_sock_map[client_sock].assign(m_read_buffer.data(), uname_size);
                m_client_pollfds[m_conns].fd     = client_sock;
                m_client_pollfds[m_conns].events = POLLIN;
                m_conns++;
                break;
            }
        }
    }

    void start(int32_t tout_in_mill = 1000) noexcept
    {
        m_read_thread                   = std::thread(&Server::m_check_conns, this, tout_in_mill);
        acc_and_stdin_pollfds[0].fd     = m_accept_socket;
        acc_and_stdin_pollfds[0].events = POLLIN;
        acc_and_stdin_pollfds[1].fd     = stdin_fd;
        acc_and_stdin_pollfds[1].events = POLLIN;

        for (int32_t poll_stat; !m_close_conn;) {
            poll_stat = poll(acc_and_stdin_pollfds.data(), acc_and_stdin_pollfds.size(), tout_in_mill);
            if (poll_stat == -1) {
                std::fprintf(stderr, "Polling failed\n");
                std::fflush(stderr);
                // Handle error and return;
                return;
            } else if (poll_stat > 0) {
                // Check if server socket has any new connection)
                if (acc_and_stdin_pollfds[0].revents & POLLIN) {
                    accept_conn();
                    continue;   // Repolling to get new events for all sockets
                }
                // Check if server has any message to broadcast
                if (acc_and_stdin_pollfds[1].revents & POLLIN) {
                    int32_t msg_len = read(stdin_fd, m_write_buffer.data(), s_max_buffer_size);
                    if (m_write_buffer[0] == '0') {
                        std::fputs("Shutting Down Server\n", stdout);
                        std::fflush(stdout);
                        m_close_conn = true;
                        break;
                    }
                    for (uint8_t client_index {}; client_index < m_conns; ++client_index) {
                        m_write_client(client_index, msg_len);
                    }
                }
            }
        }
    }

    ~Server() noexcept
    {
        if (m_read_thread.joinable()) {
            m_read_thread.join();
        }
        if (m_accept_socket != -1) {
            close(m_accept_socket);
        }
    }

private:
    void m_check_conns(int32_t tout_in_mill) noexcept
    {
        for (int32_t poll_stat; !m_close_conn;) {
            poll_stat = poll(m_client_pollfds.data(), m_conns, tout_in_mill);
            if (poll_stat == -1) {
                std::fprintf(stderr, "Polling failed\n");
                std::fflush(stderr);
                // Handle error and return;
                return;
            } else if (poll_stat > 0) {
                for (int32_t client_index = 0; client_index < m_conns; client_index++) {
                    if (m_client_pollfds[client_index].revents == 0) {   // Skip clients which don't have any events
                        continue;
                    }
                    if (m_client_pollfds[client_index].revents & POLLIN) {
                        m_read_client(client_index);
                    }
                }
            }
        }
        for (uint8_t client_index {}; client_index < m_conns; ++m_conns) {
            close(m_client_pollfds[client_index].fd);
        }
    }

    void m_read_client(int32_t client_index) noexcept
    {
        int32_t read_bytes = recv(m_client_pollfds[client_index].fd, m_read_buffer.data(), m_read_buffer.size(), 0);
        if (read_bytes == -1) {
            std::fprintf(stderr, "Could not receive complete message\n");
            std::fflush(stderr);
            // Handle error and return
            return;
        }
        if (read_bytes == 0) {
            std::fprintf(stdout, "%s disconnected\n", m_client_uname_sock_map[m_client_pollfds[client_index].fd].data());
            std::fflush(stdout);
            m_close_client(client_index);
            return;
            // Close connection and swap it with an active connection to
            // compress the number of active connections
        }
        std::fprintf(stdout,
                     "Message from [%s]: %.*s",
                     m_client_uname_sock_map[m_client_pollfds[client_index].fd].data(), read_bytes, m_read_buffer.data());
        std::fflush(stdout);
    }

    inline void m_close_client(int32_t client_index) noexcept
    {
        close(m_client_pollfds[client_index].fd);
        std::swap(m_client_pollfds[client_index], m_client_pollfds[m_conns - 1]);
        m_conns--;
    }

    void m_write_client(int32_t client_index, uint16_t msg_len) noexcept
    {
        int32_t send_bytes = send(m_client_pollfds[client_index].fd, m_write_buffer.data(), msg_len, 0);
        if (send_bytes == -1) {
            std::fputs("Could not send message\n", stderr);
            std::fflush(stderr);
        }
    }

public:
    static constexpr uint8_t s_max_conns { 50 };
    static constexpr uint16_t s_max_buffer_size { 1024 };

private:
    int32_t const m_accept_socket;
    uint8_t m_conns {};
    sockaddr_in m_server_address;
    std::atomic<bool> m_close_conn {};
    std::array<char, s_max_buffer_size> m_read_buffer;
    std::array<char, s_max_buffer_size> m_write_buffer;
    std::array<pollfd, 2> acc_and_stdin_pollfds;
    std::array<pollfd, s_max_conns> m_client_pollfds;
    std::unordered_map<uint16_t, std::string> m_client_uname_sock_map;
    std::thread m_read_thread;
};

auto main(int32_t argc, char** argv) -> int32_t
{
    if (argc != 4) {
        std::fprintf(stderr, "Usage: %s <IP> <PORT> <LISTENERS>", argv[0]);
        std::exit(64);
    }

    Server<AF_INET, SOCK_STREAM> server {
        argv[1],
        static_cast<uint16_t>(std::stoul(argv[2])),
        static_cast<uint16_t>(std::stoul(argv[3]))
    };

    server.start();
}
