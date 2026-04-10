#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <ctime>

class HttpResponse {
public:
    enum StatusCode {
        OK = 200,
        CREATED = 201,
        NO_CONTENT = 204,
        MOVED_PERMANENTLY = 301,
        FOUND = 302,
        SEE_OTHER = 303,
        NOT_MODIFIED = 304,
        TEMPORARY_REDIRECT = 307,
        PERMANENT_REDIRECT = 308,
        BAD_REQUEST = 400,
        UNAUTHORIZED = 401,
        FORBIDDEN = 403,
        NOT_FOUND = 404,
        METHOD_NOT_ALLOWED = 405,
        NOT_ACCEPTABLE = 406,
        REQUEST_TIMEOUT = 408,
        CONFLICT = 409,
        LENGTH_REQUIRED = 411,
        PAYLOAD_TOO_LARGE = 413,
        URI_TOO_LONG = 414,
        UNSUPPORTED_MEDIA_TYPE = 415,
        INTERNAL_SERVER_ERROR = 500,
        NOT_IMPLEMENTED = 501,
        BAD_GATEWAY = 502,
        SERVICE_UNAVAILABLE = 503,
        GATEWAY_TIMEOUT = 504,
        HTTP_VERSION_NOT_SUPPORTED = 505
    };

private:
    int status_code;
    std::string status_message;
    std::map<std::string, std::string> headers_;
    std::string body_;
    std::string version_;
    bool chunked;
    static std::map<int, std::string> status_messages;
    static void initStatusMessages();
    static bool messages_initialized;
    
public:
    HttpResponse();
    ~HttpResponse();

    void setStatus(int code);
    void setStatus(int code, const std::string& message);
    void setHeader(const std::string& name, const std::string& value);
    void setHeader(const std::string& name, size_t value);
    void removeHeader(const std::string& name);
    bool hasHeader(const std::string& name) const;
    std::string getHeader(const std::string& name) const;
    void setBody(const std::string& content);
    void appendBody(const std::string& content);
    const std::string& getBody() const { return body_; }
    size_t getBodySize() const { return body_.size(); }
    int getStatusCode() const { return status_code; }
    void setContentType(const std::string& type);
    void setContentLength(size_t length);
    void setConnection(const std::string& value);
    void setLocation(const std::string& url);
    void setCookie(const std::string& cookie);
    void setLastModified(time_t time);
    void setExpires(time_t time);
    void setCacheControl(const std::string& directives);
    void setAllow(const std::vector<std::string>& methods);
    std::string toString() const;
    std::string buildHeaders() const;
    void enableChunked() { chunked = true; }
    bool isChunked() const { return chunked; }
    bool isError() const { return status_code >= 400; }
    bool isRedirect() const { return status_code >= 300 && status_code < 400; }
    bool isSuccess() const { return status_code >= 200 && status_code < 300; }
    static std::string getStatusMessage(int code);
    static std::string getMimeType(const std::string& extension);
    static std::string formatHttpDate(time_t time);
    static HttpResponse makeError(int code, const std::string& message = "");
    static HttpResponse makeRedirect(int code, const std::string& location);
    static HttpResponse makeFile(const std::string& content, const std::string& mime_type);
    
private:
    std::string normalizeHeaderName(const std::string& name) const;
};

#endif