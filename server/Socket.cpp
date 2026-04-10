#include "Socket.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <stdexcept>
#include <sstream>

Socket::Socket() : fd(-1), port(0) {
    std::memset(&addr, 0, sizeof(addr));
}

Socket::~Socket() {
    if (fd != -1)
        ::close(fd);
}

void Socket::create() {
    fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        throw std::runtime_error("failed to create socket: " + std::string(strerror(errno)));
}

void Socket::bind(const std::string& _host, int _port) {
    host = _host;
    port = _port;
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (host == "*" || host == "0.0.0.0")
        addr.sin_addr.s_addr = INADDR_ANY;
    else if (host == "localhost")
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    else {
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
            std::stringstream ss;
            ss << "invalid address: " << host;
            throw std::runtime_error(ss.str());
        }
    }
    
    if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        std::stringstream ss;
        ss << "failed to bind to " << host << ":" << port << " - " << strerror(errno);
        throw std::runtime_error(ss.str());
    }
}

void Socket::listen(int backlog) {
    if (::listen(fd, backlog) == -1)
        throw std::runtime_error("failed to listen on socket: " + std::string(strerror(errno)));
}

int Socket::accept() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = ::accept(fd, (struct sockaddr*)&client_addr, &client_len);
    
    if (client_fd == -1)
        return -1;
    
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags == -1 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        ::close(client_fd);
        return -1;
    }
    
    return client_fd;
}

void Socket::setNonBlocking() {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("failed to get socket flags");
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("failed to set socket to non-blocking");
}

void Socket::setReuseAddr() {
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        throw std::runtime_error("failed to set SO_REUSEADDR");
}

void Socket::close() {
    if (fd != -1) {
        ::close(fd);
        fd = -1;
    }
}