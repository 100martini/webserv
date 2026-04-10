#ifndef _HTTP_PARSER__
#define _HTTP_PARSER__

#include "HttpRequest.hpp"
#include <algorithm>

enum ParserState {
    PARSING_REQUEST_LINE,
    PARSING_HEADERS,
    PARSING_BODY,
    COMPLETE,
    ERROR
};

class HttpParser {
    private:
    std::string buffer_;
    ParserState state_;
    HttpRequest httpRequest_;
    size_t bytes_read_;
    size_t content_length_;
    bool content_length_found_;
    
    bool parseRequestLine();
    bool parseHeaders();
    bool parseBody();
    bool hasEnoughData();
    
    public:
    HttpParser();
    ~HttpParser();
    
    int parseHttpRequest(const std::string& RequestData);
    void reset();
    void setChunkedBody(const std::string& body);

    const HttpRequest& getRequest() const { return httpRequest_; }
    ParserState getState() const { return state_; }
    const std::string& getBuffer() const { return buffer_; }
    void clearBuffer() { buffer_.clear(); }
};

#endif