#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cerrno>
#include <cstring>

class Socket {
private:
    int fd;
    int port;
    std::string host;
    struct sockaddr_in addr;
    
    Socket(const Socket&);
    Socket& operator=(const Socket&);
    
public:
    Socket();
    ~Socket();
    
    void create();
    void bind(const std::string& _host, int _port);
    void listen(int backlog = 128);
    int accept();
    void setNonBlocking();
    void setReuseAddr();
    void close();
    
    int getFd() const { return fd; }
    int getPort() const { return port; }
    const std::string& getHost() const { return host; }
    bool isValid() const { return fd != -1; }
};

#endif