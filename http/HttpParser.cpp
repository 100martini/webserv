#include "HttpParser.hpp"

HttpParser::HttpParser() 
    : buffer_(""), state_(PARSING_REQUEST_LINE), bytes_read_(0), content_length_(0), content_length_found_(false) {}
HttpParser::~HttpParser() {}

void HttpParser::reset() {
    buffer_.clear();
    state_ = PARSING_REQUEST_LINE;
    bytes_read_ = 0;
    content_length_ = 0;
    content_length_found_ = false;
    httpRequest_ = HttpRequest();
}

void HttpParser::setChunkedBody(const std::string& body) {
    httpRequest_.body_ = body;
    httpRequest_.content_length_ = body.length(); 
    httpRequest_.content_length_found_ = true;
    state_ = COMPLETE;
}

int HttpParser::parseHttpRequest(const std::string& RequestData) {
    buffer_ += RequestData;
    while (state_ != COMPLETE && state_ != ERROR && HttpParser::hasEnoughData()) {
        switch (state_)
        {
            case PARSING_REQUEST_LINE:
                if (HttpParser::parseRequestLine())
                    state_ = PARSING_HEADERS;
                else 
                    break;
                break;
            case PARSING_HEADERS:
                if (HttpParser::parseHeaders()) {
                    if (httpRequest_.method_ == "POST") {
                        std::string transfer_encoding = httpRequest_.getHeader("Transfer-Encoding");
                        if (transfer_encoding == "chunked") {
                            state_ = COMPLETE;
                        }
                        else if (!content_length_found_) {
                            throw MissingContentLengthException("POST requires Content-Length");
                        }
                        else if (content_length_ > 0)
                            state_ = PARSING_BODY;
                        else
                            state_ = COMPLETE;
                    } else
                        state_ = COMPLETE;
                }
                else
                    break;
                break;
            case PARSING_BODY:
                if (HttpParser::parseBody())
                    state_ = COMPLETE;
                else 
                    break;
                break;
            case COMPLETE:
            case ERROR:
                break;
        }
    }
    return state_;
}

bool HttpParser::parseRequestLine() {
    size_t pos = buffer_.find("\r\n");
    if (pos == std::string::npos)
        return false;

    std::string line = buffer_.substr(0, pos);
    buffer_ = buffer_.substr(pos + 2);

    if (std::count(line.begin(), line.end(), ' ') != 2)
       throw MalformedRequestLineException("");

    size_t firstSpace = line.find(' ');
    if (firstSpace == std::string::npos)
        throw MalformedRequestLineException("");
    size_t secondSpace = line.find(' ', firstSpace + 1);
    if (secondSpace == std::string::npos)
        throw MalformedRequestLineException("");

    httpRequest_.setMethod(line.substr(0, firstSpace));
    std::string uri = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    
    if (uri.find("http://") == 0 || uri.find("https://") == 0) {
        size_t path_start = uri.find('/', 7);
        if (path_start != std::string::npos)
            uri = uri.substr(path_start);
        else
            uri = "/";
    }
    
    httpRequest_.handleURI(uri);
    httpRequest_.setVersion(line.substr(secondSpace + 1));
    return true;
}

bool HttpParser::parseHeaders() {
    while (true) {
        size_t pos = buffer_.find("\r\n");
        if (pos == std::string::npos)
            return false;

        std::string line = buffer_.substr(0, pos);
        buffer_ = buffer_.substr(pos + 2);

        if (line.empty())
            return true;

        if (line[0] == ' ' || line[0] == '\t')
            throw MalformedHeaderException("Header line starts with whitespace");

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos)
            throw MalformedHeaderException("");
        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        
        std::string lowerKey = key;
        for (size_t i = 0; i < lowerKey.length(); i++)
            lowerKey[i] = std::tolower(lowerKey[i]);
        
        if (lowerKey == "content-length") {
            content_length_found_ = true;
            std::string trimmed_value = value;
            size_t start = trimmed_value.find_first_not_of(" \t");
            if (start != std::string::npos)
                trimmed_value = trimmed_value.substr(start);
            char* endptr;
            long len = std::strtol(trimmed_value.c_str(), &endptr, 10);
            if (*endptr != '\0' || len < 0)
                throw InvalidContentLengthException("");
            content_length_ = static_cast<size_t>(len);
            httpRequest_.content_length_ = content_length_;
            httpRequest_.content_length_found_ = true;
        }
        
        httpRequest_.addHeader(key, value);
    }
}

bool HttpParser::parseBody() {
    if (bytes_read_ >= content_length_)
        return true;
    
    size_t needed = content_length_ - bytes_read_;
    size_t available = buffer_.length();
    size_t to_read = (available < needed) ? available : needed;
    
    httpRequest_.body_.append(buffer_.substr(0, to_read));
    buffer_.erase(0, to_read);
    bytes_read_ += to_read;
    return bytes_read_ >= content_length_;
}

bool HttpParser::hasEnoughData() {
    if (state_ == PARSING_BODY)
        return !buffer_.empty();
    size_t newLineEx = buffer_.find("\r\n");
    return newLineEx != std::string::npos;
}