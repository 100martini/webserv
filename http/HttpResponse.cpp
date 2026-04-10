#include "HttpResponse.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <cstdio>

std::map<int, std::string> HttpResponse::status_messages;
bool HttpResponse::messages_initialized = false;

HttpResponse::HttpResponse() 
    : status_code(200), status_message("OK"), version_("HTTP/1.0"), chunked(false) {
    if (!messages_initialized) {
        initStatusMessages();
        messages_initialized = true;
    }
    setHeader("Server", "webserv/1.0");
    setHeader("Date", formatHttpDate(std::time(NULL)));
}

HttpResponse::~HttpResponse() {}

void HttpResponse::initStatusMessages() {
    status_messages[200] = "OK";
    status_messages[201] = "Created";
    status_messages[204] = "No Content";
    status_messages[301] = "Moved Permanently";
    status_messages[302] = "Found";
    status_messages[303] = "See Other";
    status_messages[304] = "Not Modified";
    status_messages[307] = "Temporary Redirect";
    status_messages[308] = "Permanent Redirect";
    status_messages[400] = "Bad Request";
    status_messages[401] = "Unauthorized";
    status_messages[403] = "Forbidden";
    status_messages[404] = "Not Found";
    status_messages[405] = "Method Not Allowed";
    status_messages[406] = "Not Acceptable";
    status_messages[408] = "Request Timeout";
    status_messages[409] = "Conflict";
    status_messages[411] = "Length Required";
    status_messages[413] = "Payload Too Large";
    status_messages[414] = "URI Too Long";
    status_messages[415] = "Unsupported Media Type";
    status_messages[500] = "Internal Server Error";
    status_messages[501] = "Not Implemented";
    status_messages[502] = "Bad Gateway";
    status_messages[503] = "Service Unavailable";
    status_messages[504] = "Gateway Timeout";
    status_messages[505] = "HTTP Version Not Supported";
}

void HttpResponse::setStatus(int code) {
    status_code = code;
    status_message = getStatusMessage(code);
}

void HttpResponse::setStatus(int code, const std::string& message) {
    status_code = code;
    status_message = message.empty() ? getStatusMessage(code) : message;
}

void HttpResponse::setHeader(const std::string& name, const std::string& value) {
    std::string normalized = normalizeHeaderName(name);
    headers_[normalized] = value;
}

void HttpResponse::setHeader(const std::string& name, size_t value) {
    std::stringstream ss;
    ss << value;
    setHeader(name, ss.str());
}

void HttpResponse::removeHeader(const std::string& name) {
    headers_.erase(normalizeHeaderName(name));
}

bool HttpResponse::hasHeader(const std::string& name) const {
    return headers_.find(normalizeHeaderName(name)) != headers_.end();
}

std::string HttpResponse::getHeader(const std::string& name) const {
    std::map<std::string, std::string>::const_iterator it = headers_.find(normalizeHeaderName(name));
    return (it != headers_.end()) ? it->second : "";
}

void HttpResponse::setBody(const std::string& content) {
    body_ = content;
    if (!chunked && !hasHeader("Content-Length"))
        setContentLength(body_.size());
}

void HttpResponse::appendBody(const std::string& content) {
    body_ += content;
    if (!chunked && !hasHeader("Content-Length"))
        setContentLength(body_.size());
}

void HttpResponse::setContentType(const std::string& type) {
    setHeader("Content-Type", type);
}

void HttpResponse::setContentLength(size_t length) {
    if (!chunked)
        setHeader("Content-Length", length);
}

void HttpResponse::setConnection(const std::string& value) {
    setHeader("Connection", value);
}

void HttpResponse::setLocation(const std::string& url) {
    setHeader("Location", url);
}

void HttpResponse::setCookie(const std::string& cookie) {
    setHeader("Set-Cookie", cookie);
}

void HttpResponse::setLastModified(time_t time) {
    setHeader("Last-Modified", formatHttpDate(time));
}

void HttpResponse::setExpires(time_t time) {
    setHeader("Expires", formatHttpDate(time));
}

void HttpResponse::setCacheControl(const std::string& directives) {
    setHeader("Cache-Control", directives);
}

void HttpResponse::setAllow(const std::vector<std::string>& methods) {
    std::string allow_header;
    for (size_t i = 0; i < methods.size(); ++i) {
        if (i > 0) allow_header += ", ";
        allow_header += methods[i];
    }
    setHeader("Allow", allow_header);
}

std::string HttpResponse::toString() const {
    std::stringstream response;

    response << version_ << " " << status_code << " " << status_message << "\r\n";
    response << buildHeaders();
    response << "\r\n";

    if (status_code != 204 && status_code != 304)
        response << body_;
    return response.str();
}

std::string HttpResponse::buildHeaders() const {
    std::stringstream headers;
    
    for (std::map<std::string, std::string>::const_iterator it = headers_.begin();
         it != headers_.end(); ++it)
        headers << it->first << ": " << it->second << "\r\n";
    return headers.str();
}

std::string HttpResponse::normalizeHeaderName(const std::string& name) const {
    std::string normalized = name;
    bool capitalize_next = true;
    
    for (size_t i = 0; i < normalized.length(); ++i) {
        if (normalized[i] == '-')
            capitalize_next = true;
        else if (capitalize_next) {
            normalized[i] = std::toupper(normalized[i]);
            capitalize_next = false;
        } else
            normalized[i] = std::tolower(normalized[i]);
    }
    
    return normalized;
}

std::string HttpResponse::getStatusMessage(int code) {
    if (!messages_initialized) {
        initStatusMessages();
        messages_initialized = true;
    }
    
    std::map<int, std::string>::const_iterator it = status_messages.find(code);
    if (it != status_messages.end())
        return it->second;
    
    if (code >= 100 && code < 200) return "Informational";
    if (code >= 200 && code < 300) return "Success";
    if (code >= 300 && code < 400) return "Redirection";
    if (code >= 400 && code < 500) return "Client Error";
    if (code >= 500 && code < 600) return "Server Error";
    
    return "Unknown";
}

std::string HttpResponse::getMimeType(const std::string& extension) {
    std::string ext = extension;
    if (!ext.empty() && ext[0] == '.')
        ext = ext.substr(1);

    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
    if (ext == "css") return "text/css; charset=utf-8";
    if (ext == "js") return "application/javascript; charset=utf-8";
    if (ext == "json") return "application/json; charset=utf-8";
    if (ext == "xml") return "application/xml; charset=utf-8";
    if (ext == "txt") return "text/plain; charset=utf-8";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png") return "image/png";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "ico") return "image/x-icon";
    if (ext == "pdf") return "application/pdf";
    if (ext == "zip") return "application/zip";
    if (ext == "tar") return "application/x-tar";
    if (ext == "gz") return "application/gzip";

    return "application/octet-stream";
}

std::string HttpResponse::formatHttpDate(time_t time) {
    struct tm* gmt = std::gmtime(&time);
    char buffer[80];
    
    const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    sprintf(buffer, "%s, %02d %s %04d %02d:%02d:%02d GMT",
                days[gmt->tm_wday],
                gmt->tm_mday,
                months[gmt->tm_mon],
                gmt->tm_year + 1900,
                gmt->tm_hour,
                gmt->tm_min,
                gmt->tm_sec);
    return std::string(buffer);
}

HttpResponse HttpResponse::makeError(int code, const std::string& message) {
    HttpResponse response;
    response.setStatus(code);
    std::string msg = message.empty() ? getStatusMessage(code) : message;
    std::stringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html>\n";
    html << "<head><meta charset=\"UTF-8\"><title>" << code << " " << msg << "</title>\n";
    html << "<style>body{background:#e74c3c;color:white;font-family:Arial;display:flex;"
         << "flex-direction:column;align-items:center;justify-content:center;height:100vh;"
         << "margin:0;font-size:24px;text-align:center;}"
         << ".code{font-size:48px;font-weight:bold;margin-bottom:10px;}"
         << ".message{font-size:20px;opacity:0.9;}</style>\n";
    html << "</head>\n";
    html << "<body>\n";
    html << "<div class=\"code\">" << code << "</div>\n";
    html << "<div class=\"message\">" << msg << "</div>\n";
    html << "</body>\n";
    html << "</html>\n";
    response.setContentType("text/html; charset=utf-8");
    response.setBody(html.str());
    response.setConnection("close");
    return response;
}

HttpResponse HttpResponse::makeRedirect(int code, const std::string& location) {
    HttpResponse response;
    response.setStatus(code);
    response.setLocation(location);
    std::stringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html>\n";
    html << "<head><title>Redirect</title></head>\n";
    html << "<body>\n";
    html << "<h1>Redirecting...</h1>\n";
    html << "<p>You are being redirected to <a href=\"" << location << "\">" << location << "</a></p>\n";
    html << "</body>\n";
    html << "</html>\n";
    response.setContentType("text/html; charset=utf-8");
    response.setBody(html.str());
    response.setConnection("close");
    return response;
}

HttpResponse HttpResponse::makeFile(const std::string& content, const std::string& mime_type) {
    HttpResponse response;
    response.setStatus(200);
    response.setContentType(mime_type);
    response.setBody(content);
    return response;
}