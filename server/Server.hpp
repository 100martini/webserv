#ifndef SERVER_HPP
#define SERVER_HPP
#define PURPLE "\033[0;35m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

#include "Socket.hpp"
#include "Client.hpp"
#include "EventManager.hpp"
#include "../parsing/Config.hpp"
#include "CGIHelpers.hpp"
#include <vector>
#include <map>
#include <set>
#include <time.h>

struct CGIProcess {
    pid_t pid;
    int pipeIn;
    int pipeOut;
    int client_fd;
    std::string post_body;
    size_t bytes_written;
    std::string cgi_output;
    time_t start_time;
    bool stdin_closed;
    bool error;
    int error_code;
};

class Server {
private:
    std::vector<ServerConfig> configs;
    std::vector<Socket*> listen_sockets;
    std::map<int, Client*> clients;
    std::map<int, ServerConfig*> fd_to_config;
    EventManager event_manager;
    bool running;
    
public:
    Server(const std::vector<ServerConfig>& _configs);
    ~Server();
    
    void start();
    void run();
    void stop();
    
private:
    void acceptNewClient(int listen_fd);
    void handleClientRead(Client* client);
    void handleClientChunked(Client* client);
    void handleClientWrite(Client* client);
    void removeClient(Client* client);
    
    void checkTimeouts();
    
    ServerConfig* getConfigForListenFd(int fd);

    std::map<int, CGIProcess*> active_cgis;

    void executeCGI(Client* client);
    void handleCGIEventPipe(int fd, const EventManager::Event& event);
    void readCGIOutput(CGIProcess* cgi);
    void writeCGIInput(CGIProcess* cgi);
    void completeCGI(CGIProcess* cgi);
    void checkCGITimeout();
    void cleanupCGI(CGIProcess* cgi);
};

#endif