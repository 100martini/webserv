#include "EventManager.hpp"
#include <stdexcept>
#include <unistd.h>
#include <sstream>

EventManager::EventManager() : epoll_fd(-1) {
    epoll_fd = epoll_create(1);
    if (epoll_fd == -1)
        throw std::runtime_error("epoll_create1() failed: " + std::string(strerror(errno)));
    epoll_events.resize(MAX_EVENTS);
}

EventManager::~EventManager() {
    if (epoll_fd != -1)
        ::close(epoll_fd);
}

//just encaps bcus the server doesn't know or care whether EventManager uses poll or epoll, kisayn wait()
void EventManager::addFd(int fd, bool monitor_read, bool monitor_write) {
    if (fd_events.find(fd) != fd_events.end())
        return;
    
    struct epoll_event ev;
    ev.events = 0;
    
    if (monitor_read)
        ev.events |= EPOLLIN;
    if (monitor_write)
        ev.events |= EPOLLOUT;
    
    ev.data.fd = fd;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        std::stringstream ss;
        ss << "epoll_ctl(ADD) failed for fd " << fd << ": " << strerror(errno);
        throw std::runtime_error(ss.str());
    }
    
    fd_events[fd] = ev.events;
}

void EventManager::removeFd(int fd) {
    std::map<int, uint32_t>::iterator it = fd_events.find(fd);
    if (it == fd_events.end())
        return;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) {
        if (errno != ENOENT && errno != EBADF) {
            std::stringstream ss;
            ss << "epoll_ctl(DEL) failed for fd " << fd << ": " << strerror(errno);
        }
    }

    fd_events.erase(it);
}

void EventManager::setWriteMonitoring(int fd, bool enable) {
    std::map<int, uint32_t>::iterator it = fd_events.find(fd);
    if (it == fd_events.end())
        return;
    
    uint32_t new_events = it->second;
    
    if (enable)
        new_events |= EPOLLOUT;
    else
        new_events &= ~EPOLLOUT;
    
    if (new_events != it->second) {
        modifyFd(fd, new_events);
        it->second = new_events;
    }
}

void EventManager::setReadMonitoring(int fd, bool enable) {
    std::map<int, uint32_t>::iterator it = fd_events.find(fd);
    if (it == fd_events.end())
        return;
    
    uint32_t new_events = it->second;
    
    if (enable)
        new_events |= EPOLLIN;
    else
        new_events &= ~EPOLLIN;
    
    if (new_events != it->second) {
        modifyFd(fd, new_events);
        it->second = new_events;
    }
}

void EventManager::modifyFd(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        std::stringstream ss;
        ss << "epoll_ctl(MOD) failed for fd " << fd << ": " << strerror(errno);
        throw std::runtime_error(ss.str());
    }
}

int EventManager::wait(int timeout_ms) {
    events.clear();
    
    if (fd_events.empty())
        return 0;
    
    int nfds = epoll_wait(epoll_fd, &epoll_events[0], MAX_EVENTS, timeout_ms);
    
    if (nfds == -1) {
        if (errno == EINTR)
            return 0;
        throw std::runtime_error("epoll_wait() failed: " + std::string(strerror(errno)));
    }
    
    for (int i = 0; i < nfds; i++) {
        Event e;
        e.fd = epoll_events[i].data.fd;
        e.readable = (epoll_events[i].events & EPOLLIN) != 0;
        e.writable = (epoll_events[i].events & EPOLLOUT) != 0;
        e.error = (epoll_events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0;
        events.push_back(e);
    }
    
    return events.size();
}

//was testing with it, to ignore
bool EventManager::isMonitored(int fd) const {
    return fd_events.find(fd) != fd_events.end();
}