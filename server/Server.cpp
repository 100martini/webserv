#include "Server.hpp"
#include <iostream>
#include <cstring>
#include <signal.h>

static bool g_server_running = true;

static void signalHandler(int sig) {
    (void)sig;
    g_server_running = false;
    std::cout << "\nshutting down server." << std::endl;
}

Server::Server(const std::vector<ServerConfig>& _configs) 
    : configs(_configs), running(false) {
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);
}

Server::~Server() {
    stop();
    
    std::set<CGIProcess*> unique_cgis;
    for (std::map<int, CGIProcess*>::iterator it = active_cgis.begin();
         it != active_cgis.end(); ++it) {
        unique_cgis.insert(it->second);
    }
    
    for (std::set<CGIProcess*>::iterator it = unique_cgis.begin();
         it != unique_cgis.end(); ++it) {
        CGIProcess* cgi = *it;
        
        kill(cgi->pid, SIGTERM);
        usleep(100000);
        
        int status;
        if (waitpid(cgi->pid, &status, WNOHANG) == 0) {
            kill(cgi->pid, SIGKILL);
            waitpid(cgi->pid, &status, 0);
        }
        
        if (cgi->pipeIn != -1) {
            close(cgi->pipeIn);
            cgi->pipeIn = -1;
        }
        if (cgi->pipeOut != -1) {
            close(cgi->pipeOut);
            cgi->pipeOut = -1;
        }
        delete cgi;
    }
    active_cgis.clear();
    
    for (size_t i = 0; i < listen_sockets.size(); i++)
        delete listen_sockets[i];
    
    for (std::map<int, Client*>::iterator it = clients.begin(); 
         it != clients.end(); ++it)
        delete it->second;
}

void Server::start() {
    std::cout << PURPLE << "\nstarting ircerv..." << RESET << std::endl;
    
    for (size_t i = 0; i < configs.size(); i++) {
        Socket* sock = new Socket();
        
        try {
            sock->create();
            sock->setReuseAddr();
            sock->setNonBlocking(); 
            sock->bind(configs[i].host, configs[i].port);
            sock->listen();
            
            listen_sockets.push_back(sock);
            fd_to_config[sock->getFd()] = &configs[i];
            event_manager.addFd(sock->getFd(), true, false);
            
            std::cout << "listening on " << YELLOW << configs[i].host 
                      << ":" << configs[i].port 
                      << RESET << " (fd=" << sock->getFd() << ")" << std::endl;
        }
        catch (const std::exception& e) {
            delete sock;
            throw std::runtime_error("failed to start server: " + std::string(e.what()));
        }
    }
    
    running = true;
    std::cout << GREEN << "server started successfully! <3" << RESET << std::endl;
}

void Server::run() {
    std::cout << "server is running. Ctrl+C if you wanna stop." << std::endl;
    
    while (running && g_server_running) {
        int num_events = event_manager.wait(100);
        
        if (num_events > 0) {
            const std::vector<EventManager::Event>& events = event_manager.getEvents();
            
            for (size_t i = 0; i < events.size(); i++) {
                const EventManager::Event& event = events[i];
                
                if (fd_to_config.find(event.fd) != fd_to_config.end()) {
                    if (event.readable)
                        acceptNewClient(event.fd);
                }
                else if (clients.find(event.fd) != clients.end()) {
                    Client* client = clients[event.fd];

                    if (event.error) {
                        removeClient(client);
                        continue;
                    }
                    
                    if (event.readable) {
                        if (client->getState() == Client::READING_REQUEST) {
                            handleClientRead(client);
                            if (clients.find(event.fd) == clients.end())
                                continue;
                        }
                        else if (client->getState() == Client::READING_CHUNKED) {
                            handleClientChunked(client);
                            if (clients.find(event.fd) == clients.end())
                                continue;
                        }
                    }
                    
                    if (event.writable && client->getState() == Client::SENDING_RESPONSE) {
                        handleClientWrite(client);
                        if (clients.find(event.fd) == clients.end())
                            continue;
                    }
                    
                    if (clients.find(event.fd) != clients.end() && 
                        client->getState() == Client::PROCESSING_REQUEST) {
                        if (client->hasContinuePending()) {
                            client->setContinueResponse();
                            client->clearContinuePending();
                            client->setState(Client::SENDING_RESPONSE);
                            event_manager.setReadMonitoring(client->getFd(), true);
                            event_manager.setWriteMonitoring(client->getFd(), true);
                        } else {
                            client->processRequest();
                            if (client->getState() == Client::CGI_IN_PROGRESS) {
                                executeCGI(client);
                                event_manager.setReadMonitoring(client->getFd(), true);
                                event_manager.setWriteMonitoring(client->getFd(), false);
                            } else {
                                event_manager.setReadMonitoring(client->getFd(), true);
                                event_manager.setWriteMonitoring(client->getFd(), true);
                            }
                        }
                    }
                }
                else if (active_cgis.find(event.fd) != active_cgis.end()) {
                    handleCGIEventPipe(event.fd, event);
                }
            }
        }
        
        checkTimeouts();
        checkCGITimeout();
    }
}

void Server::stop() {
    running = false;
    
    while (!clients.empty())
        removeClient(clients.begin()->second);
}

void Server::acceptNewClient(int listen_fd) {
    Socket* listen_socket = NULL;
    for (size_t i = 0; i < listen_sockets.size(); i++) {
        if (listen_sockets[i]->getFd() == listen_fd) {
            listen_socket = listen_sockets[i];
            break;
        }
    }
    
    if (!listen_socket)
        return;
    
    int accepted_count = 0;
    const int MAX_ACCEPT_PER_CYCLE = 10;
    
    while (accepted_count < MAX_ACCEPT_PER_CYCLE) {
        int client_fd = listen_socket->accept();
        if (client_fd == -1)
            break;
        
        if (clients.size() >= 1000) {
            ::close(client_fd);
            std::cout << "max clients reached, rejecting connection" << std::endl;
            break;
        }
        
        ServerConfig* config = fd_to_config[listen_fd];
        Client* client = new Client(client_fd, config);
        clients[client_fd] = client;
        
        event_manager.addFd(client_fd, true, true);
        
        accepted_count++;
    }
}

void Server::handleClientChunked(Client* client) {
    if (!client) return;
    
    bool complete = client->readChunkedBody();
    if (client->getState() == Client::CLOSING) {
        removeClient(client);
        return;
    }
    if (complete && client->getState() == Client::PROCESSING_REQUEST) {
        client->processRequest();
        if (client->getState() == Client::CGI_IN_PROGRESS) {
            executeCGI(client);
            event_manager.setReadMonitoring(client->getFd(), true);
            event_manager.setWriteMonitoring(client->getFd(), false);
            return;
        }
        event_manager.setReadMonitoring(client->getFd(), true);
        event_manager.setWriteMonitoring(client->getFd(), true);
    }
    else if (client->getState() == Client::SENDING_RESPONSE) {
        event_manager.setReadMonitoring(client->getFd(), true);
        event_manager.setWriteMonitoring(client->getFd(), true);
    }
}

void Server::handleClientRead(Client* client) {
    if (!client) return;

    bool request_complete = client->readRequest();
    if (client->getState() == Client::CLOSING) {
        removeClient(client);
        return;
    }
    if (request_complete && client->getState() == Client::PROCESSING_REQUEST) {
        if (client->hasContinuePending()) {
            client->setContinueResponse();
            client->clearContinuePending();
            client->setState(Client::SENDING_RESPONSE);
            event_manager.setReadMonitoring(client->getFd(), true);
            event_manager.setWriteMonitoring(client->getFd(), true);
            return;
        }
        client->processRequest();

        if (client->getState() == Client::CGI_IN_PROGRESS) {
            executeCGI(client);
            event_manager.setReadMonitoring(client->getFd(), true);
            event_manager.setWriteMonitoring(client->getFd(), false);
            return ;
        }

        event_manager.setReadMonitoring(client->getFd(), true);
        event_manager.setWriteMonitoring(client->getFd(), true);
    }
    else if (client->getState() == Client::SENDING_RESPONSE) {
        event_manager.setReadMonitoring(client->getFd(), true);
        event_manager.setWriteMonitoring(client->getFd(), true);
    }
}

void Server::handleClientWrite(Client* client) {
    if (!client) return;
    
    bool response_complete = client->sendResponse();
    
    if (client->getState() == Client::CLOSING) {
        removeClient(client);
        return;
    }
    if (response_complete)
        removeClient(client);
    else if (client->getState() == Client::READING_REQUEST) {
        event_manager.setWriteMonitoring(client->getFd(), true);
        event_manager.setReadMonitoring(client->getFd(), true);
    }
}

void Server::removeClient(Client* client) {
    if (!client) return;
    
    int fd = client->getFd();
    if (fd < 0) {
        delete client;
        return;
    }
    event_manager.removeFd(fd);
    std::map<int, Client*>::iterator it = clients.find(fd);
    if (it != clients.end())
        clients.erase(it);
    delete client;
}

void Server::checkTimeouts() {
    std::vector<Client*> timed_out;
    
    for (std::map<int, Client*>::iterator it = clients.begin(); 
         it != clients.end(); ++it) {
        Client* client = it->second;
        
        if (client->getState() == Client::READING_REQUEST) {
            if (client->getRequestBufferSize() > 0 && client->isHeaderTimedOut()) {
                client->buildErrorResponse(408, "Request Timeout");
                client->setState(Client::SENDING_RESPONSE);
                event_manager.setWriteMonitoring(client->getFd(), true);
                event_manager.setReadMonitoring(client->getFd(), true);
                continue;
            }
        }
        else if (client->getState() == Client::READING_CHUNKED &&
                 client->isTimedOut(10))
            timed_out.push_back(client);
        else if (client->isTimedOut(60))
            timed_out.push_back(client);
    }
    
    for (size_t i = 0; i < timed_out.size(); i++) {
        std::cout << "client timed out: fd=" << timed_out[i]->getFd() << std::endl;
        removeClient(timed_out[i]);
    }
}

ServerConfig* Server::getConfigForListenFd(int fd) {
    std::map<int, ServerConfig*>::iterator it = fd_to_config.find(fd);
    if (it != fd_to_config.end())
        return it->second;
    return NULL;
}

void Server::cleanupCGI(CGIProcess* cgi) {
    if (cgi->pipeOut != -1) {
        event_manager.removeFd(cgi->pipeOut);
        close(cgi->pipeOut);
        active_cgis.erase(cgi->pipeOut);
        cgi->pipeOut = -1;
    }
    if (cgi->pipeIn != -1 && !cgi->stdin_closed) {
        event_manager.removeFd(cgi->pipeIn);
        close(cgi->pipeIn);
        active_cgis.erase(cgi->pipeIn);
        cgi->pipeIn = -1;
        cgi->stdin_closed = true;
    }
}

void Server::executeCGI(Client* client) {
    int pipeIn[2];
    int pipeOut[2];
    pid_t pid;

    pipeIn[0] = -1;
    pipeIn[1] = -1;
    pipeOut[0] = -1;
    pipeOut[1] = -1;

    if (pipe(pipeIn) == -1) {
        client->buildErrorResponse(500, "Internal Server Error");
        client->setState(Client::SENDING_RESPONSE);
        event_manager.setWriteMonitoring(client->getFd(), true);
        return;
    }
    
    if (pipe(pipeOut) == -1) {
        close(pipeIn[0]);
        close(pipeIn[1]);
        client->buildErrorResponse(500, "Internal Server Error");
        client->setState(Client::SENDING_RESPONSE);
        event_manager.setWriteMonitoring(client->getFd(), true);
        return;
    }

    if (!setFdNonBlocking(pipeOut[0]) || !setFdNonBlocking(pipeIn[1])) {
        close(pipeIn[0]);
        close(pipeIn[1]);
        close(pipeOut[0]);
        close(pipeOut[1]);
        client->buildErrorResponse(500, "Internal Server Error");
        client->setState(Client::SENDING_RESPONSE);
        event_manager.setWriteMonitoring(client->getFd(), true);
        return;
    }

    pid = fork();
    if (pid == -1) {
        close(pipeIn[0]);
        close(pipeIn[1]);
        close(pipeOut[0]);
        close(pipeOut[1]);
        client->buildErrorResponse(500, "Internal Server Error");
        client->setState(Client::SENDING_RESPONSE);
        event_manager.setWriteMonitoring(client->getFd(), true);
        return;
    }
    if (pid == 0) {
        close(pipeIn[1]);
        close(pipeOut[0]);

        dup2(pipeIn[0], STDIN_FILENO);
        dup2(pipeOut[1], STDOUT_FILENO);
        dup2(pipeOut[1], STDERR_FILENO);

        close(pipeIn[0]);
        close(pipeOut[1]);

        for (int fd = 3; fd < 1024; fd++)
            close(fd);

        const std::string& fullPath = client->getCGIFullPath();
        std::vector<std::string> env_vect = prepareEnvComplete(client->getCGIRequest(), client->getServerConfig(), fullPath);
        char **envp = vectorToCharArray(env_vect);
        std::string interpreter = client->getCGILocation()->cgi[client->getCGIExtension()];
        if (interpreter.empty()) {
            char* argv[] = { const_cast<char*>(fullPath.c_str()), NULL };
            execve(fullPath.c_str(), argv, envp);
        } else {
            char* argv[] = {
                const_cast<char*>(interpreter.c_str()),
                const_cast<char*>(fullPath.c_str()),
                NULL
            };
            execve(interpreter.c_str(), argv, envp);
        }
        freeCharArray(envp);
        _exit(127);
    }
    close(pipeIn[0]);
    close(pipeOut[1]);

    CGIProcess *cgi = new CGIProcess();
    cgi->pid = pid;
    cgi->pipeIn = pipeIn[1];
    cgi->pipeOut = pipeOut[0];
    cgi->client_fd = client->getFd();
    cgi->post_body = client->getCGIRequest()->getBody();
    cgi->bytes_written = 0;
    cgi->cgi_output = "";
    cgi->start_time = time(NULL);
    cgi->stdin_closed = false;
    cgi->error = false;
    cgi->error_code = 0;
    
    active_cgis[pipeOut[0]] = cgi;
    event_manager.addFd(pipeOut[0], true, false);
    if (!cgi->post_body.empty()) {
        active_cgis[pipeIn[1]] = cgi;
        event_manager.addFd(pipeIn[1], false, true);
    }
    else {
        close(pipeIn[1]);
        cgi->pipeIn = -1;
        cgi->stdin_closed = true;
    }
}

void Server::handleCGIEventPipe(int fd, const EventManager::Event& event) {
    CGIProcess* cgi_ptr = NULL;
    for (std::map<int, CGIProcess*>::iterator it = active_cgis.begin();
        it != active_cgis.end(); ++it) {
        if (it->second->pipeOut == fd || it->second->pipeIn == fd) {
            cgi_ptr = it->second;
            break;
        }
    }
    if (!cgi_ptr) return;

    if (event.readable && fd == cgi_ptr->pipeOut)
        readCGIOutput(cgi_ptr);

    if (event.writable && fd == cgi_ptr->pipeIn)
        writeCGIInput(cgi_ptr);

    int status;
    pid_t result = waitpid(cgi_ptr->pid, &status, WNOHANG);
    if (result > 0) {
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code != 0) {
                cgi_ptr->error = true;
                cgi_ptr->error_code = exit_code;
            }
        }
        else if (WIFSIGNALED(status)) {
            cgi_ptr->error = true;
            cgi_ptr->error_code = 128 + WTERMSIG(status);
        }
        completeCGI(cgi_ptr);
    }
}

void Server::readCGIOutput(CGIProcess* cgi) {
    static const size_t MAX_CGI_OUTPUT = 10 * 1024 * 1024;
    
    if (cgi->cgi_output.size() >= MAX_CGI_OUTPUT) {
        cgi->error = true;
        cgi->error_code = 1;
        return;
    }
    
    char buffer[8192];
    ssize_t bytes = read(cgi->pipeOut, buffer, sizeof(buffer));
    if (bytes > 0) {
        size_t space_left = MAX_CGI_OUTPUT - cgi->cgi_output.size();
        size_t to_append = (static_cast<size_t>(bytes) < space_left) ? bytes : space_left;
        cgi->cgi_output.append(buffer, to_append);
        
        if (cgi->cgi_output.size() >= MAX_CGI_OUTPUT) {
            cgi->error = true;
            cgi->error_code = 1;
        }
    }
    else if (bytes == 0) {
        event_manager.removeFd(cgi->pipeOut);
        close(cgi->pipeOut);
        active_cgis.erase(cgi->pipeOut);
        cgi->pipeOut = -1;
    }
    else {
        event_manager.removeFd(cgi->pipeOut);
        close(cgi->pipeOut);
        active_cgis.erase(cgi->pipeOut);
        cgi->pipeOut = -1;
    }
}

void Server::writeCGIInput(CGIProcess* cgi) {
    if (cgi->stdin_closed || cgi->bytes_written >= cgi->post_body.length()) {
        if (!cgi->stdin_closed && cgi->pipeIn != -1) {
            event_manager.removeFd(cgi->pipeIn);
            close(cgi->pipeIn);
            active_cgis.erase(cgi->pipeIn);
            cgi->pipeIn = -1;
            cgi->stdin_closed = true;
        }
        return ;
    }

    size_t remaining = cgi->post_body.length() - cgi->bytes_written;
    const char* data = cgi->post_body.c_str() + cgi->bytes_written;
    ssize_t written = write(cgi->pipeIn, data, remaining);
    
    if (written > 0) {
        cgi->bytes_written += written;
        if (cgi->bytes_written >= cgi->post_body.length()) {
            event_manager.removeFd(cgi->pipeIn);
            close(cgi->pipeIn);
            active_cgis.erase(cgi->pipeIn);
            cgi->pipeIn = -1;
            cgi->stdin_closed = true;
        }
    }
    else if (written == 0) {
        return;
    }
    else {
        event_manager.removeFd(cgi->pipeIn);
        close(cgi->pipeIn);
        active_cgis.erase(cgi->pipeIn);
        cgi->pipeIn = -1;
        cgi->stdin_closed = true;
    }
}

void Server::completeCGI(CGIProcess* cgi) {
    std::map<int, Client*>::iterator client_it = clients.find(cgi->client_fd);
    if (client_it == clients.end()) {
        cleanupCGI(cgi);
        delete cgi;
        return;
    }

    Client* client = client_it->second;
    HttpResponse response;
    
    cleanupCGI(cgi);

    if (cgi->error) {
        if (cgi->error_code == 126)
            response = HttpResponse::makeError(403, "CGI Permission denied");
        else if (cgi->error_code == 127)
            response = HttpResponse::makeError(404, "CGI Script not found");
        else
            response = HttpResponse::makeError(500, "CGI execution failed");
    }
    else if (cgi->cgi_output.empty()) {
        response = HttpResponse::makeError(500, "Empty CGI output");
    }
    else {
        response = processCGIOutput(cgi->cgi_output);
    }
    
    client->setResponseBuffer(response.toString());
    client->setState(Client::SENDING_RESPONSE);
    event_manager.setWriteMonitoring(client->getFd(), true);
    event_manager.setReadMonitoring(client->getFd(), true);

    delete cgi;
}

void Server::checkCGITimeout() {
    time_t timeout_sec = 5;
    time_t current_time = time(NULL);

    std::set<CGIProcess*> unique_cgis;
    for (std::map<int, CGIProcess*>::iterator it = active_cgis.begin();
         it != active_cgis.end(); ++it) {
        if ((current_time - it->second->start_time) >= timeout_sec)
            unique_cgis.insert(it->second);
    }

    for (std::set<CGIProcess*>::iterator it = unique_cgis.begin();
         it != unique_cgis.end(); ++it) {
        CGIProcess* cgi = *it;
        
        kill(cgi->pid, SIGKILL);
        waitpid(cgi->pid, NULL, 0);

        cleanupCGI(cgi);

        std::map<int, Client*>::iterator client_it = clients.find(cgi->client_fd);
        if (client_it != clients.end()) {
            Client* client = client_it->second;
            HttpResponse response = HttpResponse::makeError(504, "CGI timeout");
            client->setResponseBuffer(response.toString());
            client->setState(Client::SENDING_RESPONSE);
            event_manager.setWriteMonitoring(client->getFd(), true);
            event_manager.setReadMonitoring(client->getFd(), true);
        }

        delete cgi;
    }
}