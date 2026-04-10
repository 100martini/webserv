#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <ctime>
#include "../parsing/Config.hpp"
#include "../http/HttpParser.hpp"

class HttpResponse;

class Client {
public:
    enum State {
        READING_REQUEST,
        READING_CHUNKED,
        PROCESSING_REQUEST,
        SENDING_RESPONSE,
        CGI_IN_PROGRESS,
        CLOSING
    };
    
    enum ChunkedState {
        CHUNK_SIZE,
        CHUNK_DATA,
        CHUNK_TRAILER
    };
    
    static const int HEADER_TIMEOUT = 5;
    static const int BODY_TIMEOUT = 30;
    static const int IDLE_TIMEOUT = 60;
    
private:
    int fd;
    State state;
    std::string request_buffer;
    std::string response_buffer;
    time_t last_activity;
    time_t request_start_time;
    const ServerConfig* server_config;
    size_t bytes_sent;
    HttpParser http_parser;
    bool keep_alive;
    bool cgi_requested;
    
    bool is_chunked;
    ChunkedState chunked_state;
    size_t chunk_size;
    std::string chunk_buffer;
    std::string chunked_body;
    
    std::string session_id;
    bool continue_pending;
    
public:
    Client(int _fd, const ServerConfig* config);
    ~Client();
    
    bool readRequest();
    bool readChunkedBody();
    bool sendResponse();
    void setState(State _state) { state = _state; }
    State getState() const { return state; }
    void processRequest();
    void buildErrorResponse(int code, const std::string& msg);
    void buildSimpleResponse(const std::string& content);
    int getFd() const { return fd; }
    time_t getLastActivity() const { return last_activity; }
    bool isTimedOut(time_t timeout_seconds = 60) const;
    bool isHeaderTimedOut() const;
    bool hasDataToSend() const { return !response_buffer.empty(); }
    size_t getRequestBufferSize() const { return request_buffer.size(); }
    void close();
    
    std::string generateSessionId();
    void extractSessionId(const std::string& cookie);
    
    bool hasContinuePending() const { return continue_pending; }
    void clearContinuePending() { continue_pending = false; }
    void setContinueResponse();
    
private:
    bool checkHeaders();
    size_t getContentLength() const;
    size_t getBodySize() const;
    LocationConfig* findMatchingLocation(const std::string& path);

private:
    LocationConfig* cgi_location;
    const HttpRequest* cgi_request; 
    std::string cgi_full_path;
    std::string cgi_extension;

public:
    LocationConfig* getCGILocation() { return cgi_location; }
    std::string getCGIFullPath() { return cgi_full_path; }
    std::string getCGIExtension() { return cgi_extension; }
    const HttpRequest* getCGIRequest() { return cgi_request; }
    const ServerConfig* getServerConfig() { return server_config; }

    void setResponseBuffer(std::string response_) { this->response_buffer = response_; }
    std::string getResponseBuffer() { return response_buffer; }
};

#endif