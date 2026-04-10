#ifndef EVENTMANAGER_HPP
#define EVENTMANAGER_HPP

#include <vector>
#include <map>
#include <sys/epoll.h>
#include <cerrno>
#include <cstring>
class EventManager {
public:
    struct Event {
        int fd;
        bool readable;
        bool writable;
        bool error;
    };
    
private:
    int epoll_fd;
    std::vector<struct epoll_event> epoll_events;
    std::map<int, uint32_t> fd_events;
    std::vector<Event> events;
    static const int MAX_EVENTS = 1024;
    
public:
    EventManager();
    ~EventManager();
    
    void addFd(int fd, bool monitor_read = true, bool monitor_write = false);
    void removeFd(int fd);
    void setWriteMonitoring(int fd, bool enable);
    void setReadMonitoring(int fd, bool enable);
    
    int wait(int timeout_ms = -1);
    
    const std::vector<Event>& getEvents() const { return events; }
    bool isMonitored(int fd) const;
    
private:
    void modifyFd(int fd, uint32_t events);
};

#endif