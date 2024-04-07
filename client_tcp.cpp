#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <thread>
#include <string>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

static const int32_t stdin_fd  = fileno(stdin);
static const int32_t stdout_fd = fileno(stdout);
static const int32_t stderr_fd = fileno(stderr);

template <int32_t Domain, int32_t Type, int32_t Proto = 0>
class Client {
public:
    int32_t const domain   = Domain;
    int32_t const type     = Type;
    int32_t const protocol = Proto;

    Client(std::string const& uname) noexcept
        : m_uname { std::move(uname) }
        , m_socket { socket(Domain, Type, Proto) }
    {
        if (m_socket == -1) {
            std::fprintf(stderr, "Could not create socket for client : %s\n", m_uname.c_str());
            std::fflush(stderr);
            // Hanlde error and return
            return;
        }
        int32_t flags = fcntl(m_socket, F_GETFL);
        if (flags == -1) {
            std::fprintf(stderr, "Could not get socket flags for client : %s\n", m_uname.c_str());
            std::fflush(stderr);
            // Handle error and return
            return;
        }
        if (fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
            std::fprintf(stderr, "Could not set socket flags for client : %s\n", m_uname.c_str());
            std::fflush(stderr);
            // Handle error and return
            return;
        }
    }

    Client(Client const&)             = delete;
    Client& operator=(Client const&)  = delete;
    Client(Client const&&)            = delete;
    Client& operator=(Client const&&) = delete;

    template <typename SockAddrType>
    void connect_to(SockAddrType&& server_address) noexcept
    {
        while (1) {
            if (connect(m_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(SockAddrType)) == -1) {
                if (errno == EINPROGRESS) {
                    std::fputs("Connecting...\n", stdout);
                    std::fflush(stdout);
                    continue;
                }
                if (errno == EISCONN) {
                    int32_t send_uname = send(m_socket, m_uname.c_str(), m_uname.size(), 0);
                    if (send_uname == -1) {
                        std::fputs("Could not send username\n", stderr);
                    }
                    m_is_conn = true;
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
        }
    }

    void communicate()
    {
        if (!m_is_conn) {
            return;
        }
        m_read_thread = std::thread(&Client::m_receive_msg, this);
        m_send_msg();
    }

    ~Client() noexcept
    {
        if (m_read_thread.joinable()) {
            m_read_thread.join();
        }
        if (m_socket != -1) {
            close(m_socket);
        }
    }

private:
    void m_receive_msg() noexcept
    {
        for (int32_t read_bytes {}; !m_close_conn;) {
            read_bytes = recv(m_socket, m_read_buffer.data(), s_max_buffer_size, 0);
            if (read_bytes == -1) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    continue;
                }
                std::fputs("Could not receive complete message\n", stderr);
                std::fflush(stderr);
            } else if (read_bytes == 0) {
                std::fputs("Server closed connection\n", stdout);
                std::fflush(stdout);
                m_close_conn = true;
                continue;
            }
            std::fputs("Message from [SERVER]: ", stdout);
            std::fwrite(m_read_buffer.data(), sizeof(char), read_bytes, stdout);
            std::fflush(stdout);
        }
    }

    void m_send_msg() noexcept
    {
        for (int32_t send_bytes {}; !m_close_conn;) {
            send_bytes = read(stdin_fd, m_write_buffer.data(), s_max_buffer_size);

            if (send_bytes == -1) {
                if (errno == EWOULDBLOCK) {
                    continue;
                }
                std::fputs("Could not send message\n", stderr);
                std::fflush(stderr);
            }

            if (m_write_buffer[0] == '0') {
                std::fputs("Closing connection\n", stdout);
                std::fflush(stdout);
                m_close_conn = true;
                break;
            }
            send_bytes = send(m_socket, m_write_buffer.data(), send_bytes, 0);
        }
    }

public:
    static constexpr uint16_t s_max_buffer_size { 1024 };

private:
    std::string const m_uname;
    int32_t const m_socket;
    bool m_is_conn {};
    std::atomic<bool> m_close_conn {};
    std::thread m_read_thread;
    std::array<char, s_max_buffer_size> m_read_buffer {};
    std::array<char, s_max_buffer_size> m_write_buffer {};
};

int32_t main(int32_t argc, char** argv)
{

    if (argc != 4) {
        std::fprintf(stderr, "Usage: %s <IP> <PORT> <UNAME>\n", argv[0]);
        std::fflush(stderr);
        std::exit(64);
    }

    int32_t flag = fcntl(stdin_fd, F_GETFL);
    if (flag == -1) {
        std::fputs("Could not get stdin flags\n", stderr);
        std::fflush(stderr);
        std::exit(1);
    }

    if (fcntl(stdin_fd, F_SETFL, flag | O_NONBLOCK) == -1) {
        std::fputs("Could not set stdin flags\n", stderr);
        std::fflush(stderr);
        std::exit(1);
    }

    // Server configuration
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &server_address.sin_addr);   // server_address.sin_addr.s_addr = inet_addr(argv[1]);
    server_address.sin_port = htons(std::stoul(argv[2]));

    Client<AF_INET, SOCK_STREAM> c { argv[3] };
    c.connect_to(std::move(server_address));
    c.communicate();
}
