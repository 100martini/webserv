#include "Client.hpp"
#include "../http/HttpResponse.hpp"
#include "../http/Methods.hpp"
#include "CGIHelpers.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <algorithm>

Client::Client(int _fd, const ServerConfig* config) 
    : fd(_fd), 
      state(READING_REQUEST), 
      request_start_time(std::time(NULL)),
      server_config(config),
      bytes_sent(0),
      keep_alive(false),
      cgi_requested(false),
      is_chunked(false),
      chunked_state(CHUNK_SIZE),
      chunk_size(0),
      session_id(""),
      continue_pending(false) {
    last_activity = std::time(NULL);
}

Client::~Client() {
    close();
}

bool Client::isHeaderTimedOut() const {
    if (state == READING_REQUEST && !request_buffer.empty())
        return (std::time(NULL) - request_start_time) > HEADER_TIMEOUT;
    return false;
}

void Client::setContinueResponse() {
    response_buffer = "HTTP/1.1 100 Continue\r\n\r\n";
    bytes_sent = 0;
}

bool Client::readRequest() {
    char buffer[8192];
    ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);
    
    if (bytes > 0) {
        last_activity = std::time(NULL);
        
        if (request_buffer.empty())
            request_start_time = std::time(NULL);
        
        request_buffer.append(buffer, bytes);
        
        try {
            int parser_state = http_parser.parseHttpRequest(std::string(buffer, bytes));
            
            if (parser_state == COMPLETE) {
                const HttpRequest& req = http_parser.getRequest();
                std::string transfer_encoding = req.getHeader("Transfer-Encoding");
                
                std::string transfer_lower = transfer_encoding;
                for (size_t i = 0; i < transfer_lower.length(); i++)
                    transfer_lower[i] = std::tolower(transfer_lower[i]);
                    
                if (transfer_lower == "chunked") {
                    is_chunked = true;
                    
                    const std::string& parser_buffer = http_parser.getBuffer();
                    if (!parser_buffer.empty()) {
                        chunk_buffer = parser_buffer;
                        http_parser.clearBuffer();
                    }
                    
                    state = READING_CHUNKED;
                    
                    if (!chunk_buffer.empty())
                        return readChunkedBody();
                    return false;
                }
                std::string expect = req.getHeader("Expect");
                if (expect == "100-continue")
                    continue_pending = true;
                
                std::string cookie = req.getHeader("Cookie");
                if (!cookie.empty())
                    extractSessionId(cookie);
                
                state = PROCESSING_REQUEST;
                return true;
            }
            else if (parser_state == ERROR) {
                buildErrorResponse(400, "Bad Request");
                state = SENDING_RESPONSE;
                return false;
            }
            return false;
            
        } catch (const MissingContentLengthException& e) {
            const HttpRequest& req = http_parser.getRequest();
            if (req.getMethod() == "POST" && req.getVersion() == "HTTP/1.0") {
                state = PROCESSING_REQUEST;
                return true;
            }
            buildErrorResponse(411, "Length Required");
            state = SENDING_RESPONSE;
            return false;
        } catch (const InvalidContentLengthException& e) {
            buildErrorResponse(400, "Invalid Content-Length");
            state = SENDING_RESPONSE;
            return false;
        } catch (const InvalidMethodName& e) {
            buildErrorResponse(405, "Method Not Allowed");
            state = SENDING_RESPONSE;
            return false;
        } catch (const HttpRequestException& e) {
            buildErrorResponse(400, "Bad Request");
            state = SENDING_RESPONSE;
            return false;
        }
    }
    else {
        state = CLOSING;
        return false;
    }
}

bool Client::readChunkedBody() {
    char buffer[8192];
    ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);
    
    if (bytes > 0)
        chunk_buffer.append(buffer, bytes);
    else {
        state = CLOSING;
        return false;
    }

    while (!chunk_buffer.empty()) {
        if (chunked_state == CHUNK_SIZE) {
            size_t pos = chunk_buffer.find("\r\n");
            if (pos == std::string::npos)
                return false;

            std::string size_str = chunk_buffer.substr(0, pos);
            chunk_buffer.erase(0, pos + 2);

            size_t semicolon_pos = size_str.find(';');
            if (semicolon_pos != std::string::npos)
                size_str = size_str.substr(0, semicolon_pos);

            char *endptr;
            chunk_size = strtoul(size_str.c_str(), &endptr, 16);

            if (*endptr != '\0') {
                buildErrorResponse(400, "Invalid chunk size");
                state = SENDING_RESPONSE;
                return false;
            }

            if (chunk_size == 0) {
                if (chunk_buffer.length() >= 2 && 
                    chunk_buffer.substr(0, 2) == "\r\n") {
                    chunk_buffer.erase(0, 2);
                }

                http_parser.setChunkedBody(chunked_body);
                chunked_body.clear();
                chunk_buffer.clear();
                chunked_state = CHUNK_SIZE;
                state = PROCESSING_REQUEST;
                return true;
            }

            chunked_state = CHUNK_DATA;
        }
        else if (chunked_state == CHUNK_DATA) {
            if (chunk_buffer.length() < chunk_size)
                return false;

            chunked_body.append(chunk_buffer.substr(0, chunk_size));
            chunk_buffer.erase(0, chunk_size);
            chunked_state = CHUNK_TRAILER;
        }
        else if (chunked_state == CHUNK_TRAILER) {
            if (chunk_buffer.length() < 2)
                return false;

            if (chunk_buffer.substr(0, 2) != "\r\n") {
                buildErrorResponse(400, "Bad chunked encoding");
                state = SENDING_RESPONSE;
                return false;
            }

            chunk_buffer.erase(0, 2);
            chunked_state = CHUNK_SIZE;
        }
    }
    
    return false;
}

bool Client::sendResponse() {
    if (response_buffer.empty())
        return true;
    
    ssize_t bytes = send(fd, response_buffer.c_str() + bytes_sent, 
                         response_buffer.length() - bytes_sent, 0);

    if (bytes > 0) {
        bytes_sent += bytes;
        last_activity = std::time(NULL);
        
        if (bytes_sent >= response_buffer.length()) {
            response_buffer.clear();
            bytes_sent = 0;
            
            if (keep_alive) {
                http_parser.reset();
                cgi_requested = false;
                is_chunked = false;
                chunked_state = CHUNK_SIZE;
                chunk_size = 0;
                chunk_buffer.clear();
                chunked_body.clear();
                request_buffer.clear();
                continue_pending = false;
                state = READING_REQUEST;
                return false;
            }
            return true;
        }
        return false;
    }
    else {
        state = CLOSING;
        return false;
    }
}

static std::string getExtension(const std::string& filepath) {
    size_t dot_pos = filepath.rfind('.');
    if (dot_pos == std::string::npos)
        return "";
    return filepath.substr(dot_pos);
}

std::string Client::generateSessionId() {
    std::stringstream ss;
    ss << "sess_" << std::time(NULL) << "_" << rand();
    return ss.str();
}

void Client::extractSessionId(const std::string& cookie) {
    size_t pos = cookie.find("sessionid=");
    if (pos != std::string::npos) {
        pos += 10;
        size_t end = cookie.find(';', pos);
        if (end == std::string::npos)
            session_id = cookie.substr(pos);
        else
            session_id = cookie.substr(pos, end - pos);
    }
}

void Client::processRequest() {
    const HttpRequest& request = http_parser.getRequest();
    HttpResponse response;
    
    std::string method = request.getMethod();
    std::string path = request.getPath();
    std::string version = request.getVersion();
    std::string connection = request.getHeader("Connection");
    std::string host = request.getHeader("Host");
    
    if (version == "HTTP/1.1" && host.empty()) {
        response = HttpResponse::makeError(400, "Host header required for HTTP/1.1");
        response_buffer = response.toString();
        state = SENDING_RESPONSE;
        return;
    }
    
    std::string conn_lower = connection;
    for (size_t i = 0; i < conn_lower.length(); i++)
        conn_lower[i] = std::tolower(conn_lower[i]);
    
    keep_alive = (version == "HTTP/1.1" && conn_lower != "close") ||
                 (version == "HTTP/1.0" && conn_lower == "keep-alive");
    
    LocationConfig* location = findMatchingLocation(path);
    
    if (!location) {
        location = findMatchingLocation("/");
        if (!location) {
            response = HttpResponse::makeError(404);
            response_buffer = response.toString();
            state = SENDING_RESPONSE;
            return;
        }
    }
    
    if (location->redirect.first > 0) {
        response = HttpResponse::makeRedirect(location->redirect.first, 
                                             location->redirect.second);
        response_buffer = response.toString();
        state = SENDING_RESPONSE;
        return;
    }
    
    if (location->methods.find(method) == location->methods.end()) {
        response.setStatus(405);
        std::vector<std::string> allowed_methods(location->methods.begin(), 
                                                location->methods.end());
        response.setAllow(allowed_methods);
        response.setContentType("text/html; charset=utf-8");
        std::stringstream html;
        html << "<!DOCTYPE html>\n";
        html << "<html>\n";
        html << "<head><meta charset=\"UTF-8\"><title>405 Method Not Allowed</title>\n";
        html << "<style>body{background:#e74c3c;color:white;font-family:Arial;display:flex;"
            << "flex-direction:column;align-items:center;justify-content:center;height:100vh;"
            << "margin:0;font-size:24px;text-align:center;}"
            << ".code{font-size:48px;font-weight:bold;margin-bottom:10px;}"
            << ".message{font-size:20px;opacity:0.9;}</style>\n";
        html << "</head>\n";
        html << "<body>\n";
        html << "<div class=\"code\">405</div>\n";
        html << "<div class=\"message\">Method Not Allowed</div>\n";
        html << "</body>\n";
        html << "</html>\n";
        response.setBody(html.str());
        
        if (server_config->error_pages.find(405) != server_config->error_pages.end()) {
            std::string error_page_path = server_config->error_pages.find(405)->second;
            std::ifstream file(error_page_path.c_str());
            if (file) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
                response.setBody(content);
            }
        }
        
        response_buffer = response.toString();
        state = SENDING_RESPONSE;
        return;
    }

    if (method == "POST") {
        std::string content_length_str = request.getHeader("Content-Length");
        if (!content_length_str.empty()) {
            size_t content_length = std::atoi(content_length_str.c_str());
            size_t max_body_size = location->has_body_count ? 
                                  location->client_max_body_size : 
                                  server_config->client_max_body_size;
            
            if (content_length > max_body_size) {
                response = HttpResponse::makeError(413, "Payload Too Large");
                if (server_config->error_pages.find(413) != server_config->error_pages.end()) {
                    std::string error_page_path = server_config->error_pages.find(413)->second;
                    std::ifstream file(error_page_path.c_str());
                    if (file) {
                        std::string content((std::istreambuf_iterator<char>(file)),
                                          std::istreambuf_iterator<char>());
                        response.setBody(content);
                    }
                }
                response_buffer = response.toString();
                state = SENDING_RESPONSE;
                return;
            }
        }
    }
    
    if (session_id.empty() && (path == "/login" || path == "/admin")) {
        session_id = generateSessionId();
        response.setCookie("sessionid=" + session_id + "; Path=/; HttpOnly");
    }
    
    std::string full_path;
    std::string request_path = request.getPath();
    if (location->path == "/")
        full_path = location->root + request_path;
    else {
        if (request_path.find(location->path) == 0) {
            std::string relative = request_path.substr(location->path.length());
            if (relative.empty() || relative[0] != '/')
                relative = "/" + relative;
            full_path = location->root + relative;
        } 
        else
            full_path = location->root + request_path;
    }
    
    if (method == "GET")
        handleGet(request, location, response, cgi_requested);
    else if (method == "POST")
        handlePost(request, location, server_config, response, cgi_requested);
    else if (method == "DELETE")
        handleDelete(request, location, response);

    if (cgi_requested) {
        cgi_request = &request;
        cgi_location = location;
        cgi_full_path = full_path;
        cgi_extension = getExtension(cgi_full_path);
        state = CGI_IN_PROGRESS;
        response_buffer.clear();
        return;
    }
    
    if (response.isError() && server_config->error_pages.find(response.getStatusCode()) != server_config->error_pages.end()) {
        std::string error_page_path = server_config->error_pages.find(response.getStatusCode())->second;
        std::ifstream file(error_page_path.c_str());
        if (file) {
            std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            response.setBody(content);
        }
    }
    
    if (!keep_alive)
        response.setConnection("close");
    else
        response.setConnection("keep-alive");
    
    if (!session_id.empty() && response.getHeader("Set-Cookie").empty())
        response.setCookie("sessionid=" + session_id + "; Path=/; HttpOnly");
    
    response_buffer = response.toString();
    state = SENDING_RESPONSE;
    
    if (keep_alive)
        request_start_time = std::time(NULL);
}

LocationConfig* Client::findMatchingLocation(const std::string& path) {
    LocationConfig* best_match = NULL;
    size_t best_match_length = 0;
    
    for (size_t i = 0; i < server_config->locations.size(); ++i) {
        const std::string& loc_path = server_config->locations[i].path;
        
        if (loc_path == path)
            return const_cast<LocationConfig*>(&server_config->locations[i]);
        
        if (path.find(loc_path) == 0) {
            if (loc_path == "/" || 
                (path.length() > loc_path.length() && path[loc_path.length()] == '/') ||
                path.length() == loc_path.length()) {
                
                if (loc_path.length() > best_match_length) {
                    best_match = const_cast<LocationConfig*>(&server_config->locations[i]);
                    best_match_length = loc_path.length();
                }
            }
        }
    }
    return best_match;
}

void Client::buildErrorResponse(int code, const std::string& msg) {
    HttpResponse response = HttpResponse::makeError(code, msg);
    
    std::map<int, std::string>::const_iterator it = server_config->error_pages.find(code);
    if (it != server_config->error_pages.end()) {
        std::string error_page_path = it->second;
        std::ifstream file(error_page_path.c_str());
        if (file) {
            std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            response.setBody(content);
        }
    }
    response_buffer = response.toString();
    bytes_sent = 0;
}

void Client::buildSimpleResponse(const std::string& content) {
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << content;
    response_buffer = response.str();
    bytes_sent = 0;
}

bool Client::checkHeaders() {
    return request_buffer.find("\r\n\r\n") != std::string::npos ||
           request_buffer.find("\n\n") != std::string::npos;
}

size_t Client::getContentLength() const {
    size_t pos = request_buffer.find("Content-Length:");
    if (pos == std::string::npos)
        pos = request_buffer.find("content-length:");
    if (pos != std::string::npos) {
        size_t end = request_buffer.find("\r\n", pos);
        if (end == std::string::npos)
            end = request_buffer.find("\n", pos);
        
        std::string length_str = request_buffer.substr(pos + 15, end - pos - 15);
        size_t first = length_str.find_first_not_of(" \t");
        if (first != std::string::npos)
            length_str = length_str.substr(first);
        return std::atoi(length_str.c_str());
    }
    return 0;
}

size_t Client::getBodySize() const {
    size_t headers_end = request_buffer.find("\r\n\r\n");
    
    if (headers_end != std::string::npos)
        return request_buffer.length() - (headers_end + 4);
    headers_end = request_buffer.find("\n\n");
    if (headers_end != std::string::npos)
        return request_buffer.length() - (headers_end + 2);
    return 0;
}

bool Client::isTimedOut(time_t timeout_seconds) const {
    return (std::time(NULL) - last_activity) > timeout_seconds;
}

void Client::close() {
    if (fd != -1) {
        ::close(fd);
        fd = -1;
    }
}