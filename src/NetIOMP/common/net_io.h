#ifndef EMP_NET_IO_H__
#define EMP_NET_IO_H__

#include <iostream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <cerrno>

namespace emp {

class NetIO {
public:
    int sock;
    bool is_server;
    std::string addr;
    int port;
    long long counter = 0;

    NetIO(const char* address, int port, bool quiet = false) {
        is_server = (address == nullptr);
        this->port = port;
        if (address != nullptr)
            this->addr = address;

        if (is_server) {
            int server_fd;
            struct sockaddr_in serv_addr;
            int opt = 1;

            if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
                if (!quiet) perror("socket failed");
                exit(EXIT_FAILURE);
            }
            if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
                if (!quiet) perror("setsockopt");
                exit(EXIT_FAILURE);
            }
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_addr.s_addr = INADDR_ANY;
            serv_addr.sin_port = htons(port);

            if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                if (!quiet) perror("bind failed");
                exit(EXIT_FAILURE);
            }
            if (listen(server_fd, 3) < 0) {
                if (!quiet) perror("listen");
                exit(EXIT_FAILURE);
            }
            socklen_t addrlen = sizeof(serv_addr);
            if ((sock = accept(server_fd, (struct sockaddr *)&serv_addr, &addrlen)) < 0) {
                if (!quiet) perror("accept");
                exit(EXIT_FAILURE);
            }
            close(server_fd);
        } else {
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                if (!quiet) std::cout << "\n Socket creation error \n";
                exit(EXIT_FAILURE);
            }

            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(port);

            if (inet_pton(AF_INET, address, &serv_addr.sin_addr) <= 0) {
                if (!quiet) std::cout << "\nInvalid address/ Address not supported \n";
                exit(EXIT_FAILURE);
            }

            // Retry connect a few times to tolerate startup races between parties
            const int max_retries = 50; // ~5s total with 100ms sleep
            int attempt = 0;
            while (true) {
                if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
                    break;
                }
                if (++attempt >= max_retries) {
                    if (!quiet) {
                        std::cout << "\nConnection Failed after retries (errno=" << errno << ")\n";
                    }
                    exit(EXIT_FAILURE);
                }
                usleep(100000); // 100ms
            }
        }
        if(!quiet)
            std::cout << "Connection established" << std::endl;
    }

    ~NetIO() {
        close(sock);
    }

    void set_nodelay() {
        const int enable = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
    }

    void flush() {
        // TCP sockets are stream-oriented, flush is not needed in the same way as for buffers.
    }

    void send_data(const void* data, size_t len) {
        size_t sent_len = 0;
        while(sent_len < len) {
            ssize_t res = send(sock, (const char*)data + sent_len, len - sent_len, 0);
            if (res >= 0) {
                sent_len += res;
            } else {
                perror("send failed");
                exit(EXIT_FAILURE);
            }
        }
        counter += len;
    }

    void recv_data(void* data, size_t len) {
        size_t recv_len = 0;
        while(recv_len < len) {
            ssize_t res = recv(sock, (char*)data + recv_len, len - recv_len, 0);
            if (res > 0) {
                recv_len += res;
            } else if (res == 0) {
                // Connection closed
                break;
            } else {
                perror("recv failed");
                exit(EXIT_FAILURE);
            }
        }
        counter += len;
    }
};

} // namespace emp
#endif // EMP_NET_IO_H__