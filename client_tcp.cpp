#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <thread>
#include <string>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

template <int32_t Domain>
class Client {
public:
    int32_t const domain = Domain;

    Client(std::string const& uname) noexcept
        : m_uname { std::move(uname) }
        , m_socket { socket(Domain, SOCK_STREAM, 0) }
    {
        if (m_socket == -1) {
            std::fprintf(stderr, "Could not create socket for client : %s\n", m_uname.c_str());
            std::fflush(stderr);
            return;
        }
        m_pollfd_set[FD_INDEX::STDIN]       = { STDIN_FILENO, POLLIN };
        m_pollfd_set[FD_INDEX::SERVER_SOCK] = { m_socket, POLLIN };
    }

    Client(Client const&)             = delete;
    Client& operator=(Client const&)  = delete;
    Client(Client const&&)            = delete;
    Client& operator=(Client const&&) = delete;

    template <typename SockAddrType>
    void connect_to(SockAddrType&& server_address) noexcept
    {
        if (connect(m_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(SockAddrType)) != -1) {
            int32_t send_uname = send(m_socket, m_uname.c_str(), m_uname.size(), 0);
            if (send_uname == -1) {
                std::fputs("Could not send username\n", stderr);
                std::fflush(stderr);
            }
            std::fputs("Connection established\n", stdout);
            std::fflush(stdout);
            return;
        }
        std::fprintf(stderr,
                     "Could not connect to server IP : %s PORT : %hu\n",
                     inet_ntop(AF_INET, &server_address.sin_addr, m_read_buffer.data(), INET_ADDRSTRLEN),
                     ntohs(server_address.sin_port));
        std::fflush(stderr);
        return;
    }

    void communicate(int32_t tout_in_mill = 1000)
    {
        for (int32_t poll_stat; !m_close_conn;) {
            poll_stat = poll(m_pollfd_set.data(), m_pollfd_set.size(), tout_in_mill);
            if (poll_stat == -1) {
                std::fprintf(stderr, "Polling failed\n");
                std::fflush(stderr);
                return;
            }
            if (poll_stat > 0) {
                if (m_pollfd_set[STDIN_FILENO].revents & POLLIN) {
                    m_send_msg();
                    continue;
                }
                if (m_pollfd_set[FD_INDEX::SERVER_SOCK].revents & POLLIN) {
                    m_receive_msg();
                }
            }
        }
    }

    ~Client() noexcept
    {
        if (m_socket != -1) {
            close(m_socket);
        }
    }

private:
    void m_receive_msg() noexcept
    {
        int32_t read_bytes = recv(m_socket, m_read_buffer.data(), s_MAX_BUFFER_SIZE, 0);
        if (read_bytes == -1) {
            std::fputs("Could not receive complete message\n", stderr);
            std::fflush(stderr);
        } else if (read_bytes == 0) {
            std::fputs("Server closed connection\n", stdout);
            std::fflush(stdout);
            m_close_conn = true;
            return;
        }
        std::fwrite(m_read_buffer.data(), sizeof(char), read_bytes, stdout);
        std::fflush(stdout);
    }

    void m_send_msg() noexcept
    {
        int32_t send_bytes         = read(STDIN_FILENO, m_write_buffer.data(), s_MAX_BUFFER_SIZE);
        m_write_buffer[send_bytes] = 0;

        if (send_bytes == -1) {
            std::fputs("Could not send message\n", stderr);
            std::fflush(stderr);
            return;
        }

        if (m_write_buffer[0] == '0') {
            std::fputs("Closing connection\n", stdout);
            std::fflush(stdout);
            m_close_conn = true;
            return;
        }

        send(m_socket, m_write_buffer.data(), send_bytes, 0);
    }

public:
    static constexpr uint16_t s_MAX_BUFFER_SIZE { 1024 };
    static constexpr uint16_t s_EXTRA_BUFFER_SIZE { 256 };

private:
    enum FD_INDEX {
        STDIN,
        SERVER_SOCK
    };
    std::string const m_uname;
    int32_t const m_socket;
    bool m_close_conn {};
    std::array<pollfd, 2> m_pollfd_set;
    std::array<char, s_MAX_BUFFER_SIZE + 1 + s_EXTRA_BUFFER_SIZE> m_read_buffer {};
    std::array<char, s_MAX_BUFFER_SIZE + 1 + s_EXTRA_BUFFER_SIZE> m_write_buffer {};
};

int32_t main(int32_t argc, char** argv)
{

    if (argc != 4) {
        std::fprintf(stderr, "Usage: %s <IP> <PORT> <UNAME>\n", argv[0]);
        std::fflush(stderr);
        std::exit(64);
    }

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &server_address.sin_addr);
    server_address.sin_port = htons(std::stoul(argv[2]));

    Client<AF_INET> c { argv[3] };
    c.connect_to(std::move(server_address));
    c.communicate();
}
