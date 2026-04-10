#include "CGIHelpers.hpp"
#include <sstream>

std::string extractValueAfterColon(const std::string& line) {
    std::string value;
    size_t colon_pos = line.find(":");
    if (colon_pos == std::string::npos) return "";
    value = line.substr(colon_pos + 1);
    value = HttpRequest::trim(value);
    return value;
}

void setCGIResponseHeaders(HttpResponse& response, const std::string& headers) {
    std::string content_type;
    std::string location;
    size_t content_length = 0;
    int status_code = 200;
    
    std::istringstream iss(headers);
    std::string line;

    while (std::getline(iss, line)) {
        if (line == "\r" || line.empty())
            break;
        if (line[line.length() - 1] == '\r')
            line = line.substr(0, line.length() - 1);
            
        std::string lower_line = line;
        for (size_t i = 0; i < lower_line.length() && lower_line[i] != ':'; i++)
            lower_line[i] = std::tolower(lower_line[i]);
            
        if (lower_line.find("status:") == 0) {
            std::string status_str = extractValueAfterColon(line);
            status_code = atoi(status_str.c_str());
        }
        else if (lower_line.find("content-length:") == 0) {
            content_length = atoi(extractValueAfterColon(line).c_str());
            response.setContentLength(content_length); 
        }
        else if (lower_line.find("content-type:") == 0) {
            content_type = extractValueAfterColon(line);
            response.setContentType(content_type);
        }
        else if (lower_line.find("location:") == 0) {
            location = extractValueAfterColon(line);
            if (!location.empty()) response.setLocation(location);
        }
        else if (lower_line.find("set-cookie:") == 0) {
            response.setCookie(extractValueAfterColon(line));
        }
        else {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string name = line.substr(0, colon_pos);
                std::string value = extractValueAfterColon(line);
                response.setHeader(name, value);
            }
        }
    }

    if (status_code != 200)
        response.setStatus(status_code);
    if (response.getHeader("Content-Length").empty()) 
        response.setContentLength(response.getBody().length());
    if (response.getHeader("Content-Type").empty())
        response.setContentType("text/html; charset=utf-8");
}

HttpResponse processCGIOutput(const std::string& cgi_output) {
    HttpResponse cgi_response;
    size_t headers_end = cgi_output.find("\r\n\r\n");
    if (headers_end == std::string::npos) 
        headers_end = cgi_output.find("\n\n");

    if (headers_end == std::string::npos) {
        std::string body = cgi_output;
        cgi_response.setStatus(200);
        cgi_response.setContentType("text/html; charset=utf-8");
        cgi_response.setBody(body);
        return cgi_response;
    }

    std::string headers = cgi_output.substr(0, headers_end);
    size_t body_start = (cgi_output.substr(headers_end, 4) == "\r\n\r\n") ? headers_end + 4 : headers_end + 2;
    std::string body = cgi_output.substr(body_start);

    cgi_response.setBody(body);
    setCGIResponseHeaders(cgi_response, headers);
    return cgi_response;
}

bool setFdNonBlocking(int fd) {
    int flags;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
       return false;
    }
    return true;
} 

std::string convertHeaderName(const std::string& headerName) {
    if (headerName.empty()) return std::string("");
    std::string new_headerName = "HTTP_";
    for (size_t i = 0; i < headerName.length(); ++i) {
        char c = headerName[i];
        if (c == '-')
            new_headerName += '_';
        else
            new_headerName += std::toupper(c);
    }
    return new_headerName;
}

std::vector<std::string> prepareEnv(const HttpRequest* _request, const ServerConfig* servConfig, const std::string& _script_filename) {
    std::vector<std::string> env;
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.push_back("SCRIPT_NAME=" + _script_filename);
    env.push_back("REQUEST_METHOD=" + _request->getMethod());
    env.push_back("QUERY_STRING=" + _request->getQueryString());
    env.push_back("SERVER_NAME=" + servConfig->server_name);
    
    std::stringstream port_ss;
    port_ss << servConfig->port;
    env.push_back("SERVER_PORT=" + port_ss.str());
    
    env.push_back("SERVER_PROTOCOL=" + _request->getVersion());
    env.push_back("REMOTE_ADDR=127.0.0.1");
    env.push_back("REQUEST_URI=" + _request->getPath());
    
    if (_request->getMethod() == "POST" && _request->getContentlength() > 0) {
        env.push_back("CONTENT_TYPE=" + _request->getHeader("Content-Type"));
        std::stringstream length_ss;
        length_ss << _request->getContentlength();
        env.push_back("CONTENT_LENGTH=" + length_ss.str());
    }
    
    if (_request->hasHeaders()) {
        std::map<std::string, std::string> request_headers = _request->getHeadersMap();
        std::map<std::string, std::string>::const_iterator it;
        for (it = request_headers.begin(); it != request_headers.end(); ++it) {
            std::string lower = it->first;
            for (size_t i = 0; i < lower.length(); i++)
                lower[i] = std::tolower(lower[i]);
            if (lower != "content-type" && lower != "content-length")
                env.push_back(convertHeaderName(it->first) + "=" + it->second);
        }
    }
    return env;
}

std::vector<std::string> prepareEnvComplete(const HttpRequest* _request, const ServerConfig* servConfig, const std::string& _script_filename) {
    std::vector<std::string> env;
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_SOFTWARE=webserv/1.0");
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    
    std::string script_name = _request->getPath();
    size_t cgi_pos = script_name.find("/cgi-bin/");
    if (cgi_pos != std::string::npos)
        script_name = script_name.substr(cgi_pos);
    env.push_back("SCRIPT_NAME=" + script_name);
    env.push_back("SCRIPT_FILENAME=" + _script_filename);
    
    std::string path_info = "";
    size_t script_end = _request->getPath().find(".py");
    if (script_end == std::string::npos)
        script_end = _request->getPath().find(".php");
    if (script_end == std::string::npos)
        script_end = _request->getPath().find(".cgi");
    
    if (script_end != std::string::npos) {
        size_t path_info_start = script_end + 3;
        if (_request->getPath()[script_end + 1] == 'p' && _request->getPath()[script_end + 2] == 'h' && _request->getPath()[script_end + 3] == 'p')
            path_info_start = script_end + 4;
        if (path_info_start < _request->getPath().length())
            path_info = _request->getPath().substr(path_info_start);
    }
    
    if (!path_info.empty()) {
        env.push_back("PATH_INFO=" + path_info);
        env.push_back("PATH_TRANSLATED=" + servConfig->locations[0].root + path_info);
    }
    
    env.push_back("REQUEST_METHOD=" + _request->getMethod());
    env.push_back("QUERY_STRING=" + _request->getQueryString());
    env.push_back("REQUEST_URI=" + _request->getPath() + (_request->getQueryString().empty() ? "" : "?" + _request->getQueryString()));
    env.push_back("SERVER_NAME=" + (servConfig->server_name.empty() ? servConfig->host : servConfig->server_name));
    
    std::stringstream port_ss;
    port_ss << servConfig->port;
    env.push_back("SERVER_PORT=" + port_ss.str());
    
    env.push_back("REMOTE_ADDR=127.0.0.1");
    env.push_back("REMOTE_HOST=localhost");
    
    if (_request->getMethod() == "POST") {
        std::string content_type = _request->getHeader("Content-Type");
        if (!content_type.empty())
            env.push_back("CONTENT_TYPE=" + content_type);
        if (_request->getContentlength() > 0) {
            std::stringstream length_ss;
            length_ss << _request->getContentlength();
            env.push_back("CONTENT_LENGTH=" + length_ss.str());
        }
    }
    
    if (_request->hasHeaders()) {
        std::map<std::string, std::string> request_headers = _request->getHeadersMap();
        std::map<std::string, std::string>::const_iterator it;
        for (it = request_headers.begin(); it != request_headers.end(); ++it) {
            std::string lower = it->first;
            for (size_t i = 0; i < lower.length(); i++)
                lower[i] = std::tolower(lower[i]);
            if (lower != "content-type" && lower != "content-length") {
                env.push_back(convertHeaderName(it->first) + "=" + it->second);
            }
            if (lower == "cookie")
                env.push_back("HTTP_COOKIE=" + it->second);
        }
    }
    
    env.push_back("REDIRECT_STATUS=200");
    env.push_back("SERVER_ADMIN=webmaster@localhost");
    env.push_back("DOCUMENT_ROOT=" + servConfig->locations[0].root);
    
    return env;
}

char** vectorToCharArray(const std::vector<std::string>& vec) {
    char** arr = new char*[vec.size() + 1];
    for (size_t i = 0; i < vec.size(); ++i) {
        arr[i] = new char[vec[i].length() + 1];
        std::strcpy(arr[i], vec[i].c_str());
    }
    arr[vec.size()] = NULL;
    return arr;
}

void freeCharArray(char** arr) {
    for (int i = 0; arr[i] != NULL; ++i) {
        delete[] arr[i];
    }
    delete[] arr;
}