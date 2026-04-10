#ifndef _HTTP_REQUEST__
#define _HTTP_REQUEST__

#include <iostream>
#include <string>
#include <map>
#include <algorithm>
#include <cctype>
#include <set>

#include "HttpExceptions.hpp"

class HttpRequest {
    private:
    std::string method_;
    std::string path_;
    std::string version_;
    std::string query_string_;

    std::map<std::string, std::string> headers_;
    std::set<std::string> NoneDupHeaders;

    std::string body_;
    bool content_length_found_;
    size_t content_length_;

    static const int MAX_URI_SIZE = 1024;

    void setMethod(const std::string& method);
    void handleURI(const std::string& uri);
    void setPath(const std::string& path);
    void setVersion(const std::string& version);
    void setQueryString(const std::string& query);
    void addHeader(const std::string& key, const std::string& value);

    std::string percentDecode(const std::string& encoded);
    bool isDublicate(const std::string& name);
    
    public:
    HttpRequest();
    ~HttpRequest();

    friend class HttpParser;

    std::string getMethod() const;
    std::string getPath() const;
    std::string getVersion() const;
    std::string getQueryString() const;
    std::string getBody() const;

    bool hasHeaders() const;
    std::string getHeader(const std::string& headerName) const;
    void printHeaders() const;

    static std::string trim(const std::string& str);

    friend std::ostream& operator<<(std::ostream& os, const HttpRequest& request);
    const std::map<std::string, std::string>& getHeadersMap() const { return headers_; }
    size_t getContentlength() const { return content_length_; }
};

#endif