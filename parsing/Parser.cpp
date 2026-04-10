#include "Parser.hpp"
#include "Utils.hpp"
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>

void Parser::validateBraceLine(const std::string& line, char brace) {
    size_t brace_pos = line.find(brace);
    if (brace_pos != std::string::npos) {
        std::string after_brace = line.substr(brace_pos + 1);
        after_brace = Utils::trim(after_brace);
        if (!after_brace.empty())
            throw ConfigException("unexpected tokens after '" + std::string(1, brace) + "': " + after_brace);
        if (brace == '}') {
            std::string before_brace = line.substr(0, brace_pos);
            before_brace = Utils::trim(before_brace);
            if (!before_brace.empty())
                throw ConfigException("unexpected tokens before '}': " + before_brace);
        }
    }
}

void Parser::validateDirectiveLine(const std::string& line, const std::string& directive) {
    if (directive == "location" || directive == "server" || directive == "}" || directive == "{")
        return;
    
    size_t semicolon_pos = line.find(';');
    if (semicolon_pos == std::string::npos)
        throw ConfigException("missing semicolon after directive: " + directive);
    
    std::string after_semicolon = line.substr(semicolon_pos + 1);
    after_semicolon = Utils::trim(after_semicolon);
    if (!after_semicolon.empty())
        throw ConfigException("unexpected tokens after semicolon: " + after_semicolon);
}

void Parser::validateRootPath(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        throw ConfigException("root path does not exist: " + path);
    if (!S_ISDIR(st.st_mode))
        throw ConfigException("root path is not a directory: " + path);
}

void Parser::validateCGIInterpreter(const std::string& interpreter) {
    struct stat st;
    if (stat(interpreter.c_str(), &st) != 0)
        throw ConfigException("CGI interpreter not found: " + interpreter);
    if (access(interpreter.c_str(), X_OK) != 0)
        throw ConfigException("CGI interpreter is not executable: " + interpreter);
}

ServerConfig Parser::parseServer(std::ifstream& file) {
    ServerConfig server;
    std::string line;
    int braceCount = 1;
    
    while (std::getline(file, line) && braceCount > 0) {
        line = Utils::trim(Utils::removeComment(line));
        if (line.empty()) continue;
        
        if (line.find('{') != std::string::npos)
            validateBraceLine(line, '{');
        if (line.find('}') != std::string::npos)
            validateBraceLine(line, '}');
        
        if (line.find("location") == std::string::npos) {
            size_t open_pos = line.find('{');
            while (open_pos != std::string::npos) {
                braceCount++;
                open_pos = line.find('{', open_pos + 1);
            }
        }
        
        size_t close_pos = line.find('}');
        while (close_pos != std::string::npos) {
            braceCount--;
            if (braceCount == 0) break;
            close_pos = line.find('}', close_pos + 1);
        }
        
        if (braceCount == 0) break;
        
        std::istringstream iss(line);
        std::string directive;
        iss >> directive;
        
        validateDirectiveLine(line, directive);
        
        if (directive == "listen") {
            std::string listen_value;
            iss >> listen_value;
            listen_value = Utils::removeSemicolon(listen_value);
            
            size_t colon_pos = listen_value.find(':');
            if (colon_pos != std::string::npos) {
                std::string host_part = listen_value.substr(0, colon_pos);
                std::string port_part = listen_value.substr(colon_pos + 1);
                
                if (!Utils::isValidHost(host_part)) {
                    throw ConfigException("invalid host in listen directive: '" + host_part + 
                        "' (must be a valid IPv4 address like 127.0.0.1 or domain like example.com)");
                }
                server.host = host_part;
                
                if (!Utils::isNumber(port_part)) {
                    throw ConfigException("invalid port in listen directive: '" + port_part + 
                        "' (must be a number)");
                }
                server.port = std::atoi(port_part.c_str());
            } else {
                if (!Utils::isNumber(listen_value))
                    throw ConfigException("invalid port number: '" + listen_value + "'");
                server.port = std::atoi(listen_value.c_str());
            }
        }
        else if (directive == "host") {
            iss >> server.host;
            server.host = Utils::removeSemicolon(server.host);
            
            std::string extra_token;
            if (iss >> extra_token) {
                extra_token = Utils::removeSemicolon(extra_token);
                if (!extra_token.empty())
                    throw ConfigException("unexpected token after host directive: '" + extra_token + "'");
            }
            
            if (!Utils::isValidHost(server.host)) {
                throw ConfigException("invalid host format: '" + server.host + 
                    "' (must be a valid IPv4 address like 127.0.0.1 or domain like example.com)");
            }
        }
        else if (directive == "server_name") {
            iss >> server.server_name;
            server.server_name = Utils::removeSemicolon(server.server_name);
        }
        else if (directive == "error_page") {
            std::vector<int> codes;
            std::string token, page;
            
            while (iss >> token) {
                token = Utils::removeSemicolon(token);
                if (Utils::isNumber(token)) {
                    codes.push_back(std::atoi(token.c_str()));
                } else {
                    page = token;
                    break;
                }
            }
            
            if (page.empty())
                throw ConfigException("error_page requires a path");
            if (codes.empty())
                throw ConfigException("error_page requires at least one error code");
            
            for (size_t i = 0; i < codes.size(); i++)
                server.error_pages[codes[i]] = page;
        }
        else if (directive == "client_max_body_size") {
            std::string size;
            iss >> size;
            size = Utils::removeSemicolon(size);
            server.client_max_body_size = Utils::parseSize(size);
        }
        else if (directive == "location") {
            std::string path;
            iss >> path;
            
            if (path.empty() || path[0] != '/')
                throw ConfigException("location path must start with '/': " + path);
            
            size_t brace_pos = line.find('{');
            bool braceOnSameLine = (brace_pos != std::string::npos);
            
            if (braceOnSameLine) {
                std::string between = line.substr(line.find(path) + path.length(), brace_pos - line.find(path) - path.length());
                between = Utils::trim(between);
                if (!between.empty())
                    throw ConfigException("unexpected tokens between path and '{': " + between);
                validateBraceLine(line, '{');
            }
            
            server.locations.push_back(parseLocation(file, path, braceOnSameLine));
        }
        else {
            throw ConfigException("Unknown directive in server block: " + directive);
        }
    }
    
    for (size_t i = 0; i < server.locations.size(); i++) {
        if (!server.locations[i].root.empty())
            validateRootPath(server.locations[i].root);
        
        for (std::map<std::string, std::string>::const_iterator it = server.locations[i].cgi.begin();
             it != server.locations[i].cgi.end(); ++it) {
            validateCGIInterpreter(it->second);
        }
    }
    
    return server;
}

std::vector<ServerConfig> Parser::parseConfigFile(const std::string& filename) {
    std::vector<ServerConfig> servers;
    std::ifstream file(filename.c_str());
    
    if (!file.is_open())
        throw ConfigException("cannot open config file: " + filename);

    std::string line;
    while (std::getline(file, line)) {
        line = Utils::trim(Utils::removeComment(line));
        if (line.empty()) continue;
        
        if (line.find("server") == 0) {
            size_t brace_pos = line.find('{');
            if (brace_pos != std::string::npos) {
                std::string between = line.substr(6, brace_pos - 6);
                between = Utils::trim(between);
                if (!between.empty())
                    throw ConfigException("unexpected tokens between 'server' and '{': " + between);
                validateBraceLine(line, '{');
                servers.push_back(parseServer(file));
            } else if (line == "server") {
                std::string next_line;
                if (std::getline(file, next_line)) {
                    next_line = Utils::trim(Utils::removeComment(next_line));
                    if (next_line == "{")
                        servers.push_back(parseServer(file));
                    else
                        throw ConfigException("expected '{' after 'server', found: " + next_line);
                } else
                    throw ConfigException("unexpected end of file after 'server'");
            } else
                throw ConfigException("invalid server directive: " + line);
        } else
            throw ConfigException("unexpected token outside server block: " + line);
    }
    
    file.close();
    
    if (servers.empty())
        throw ConfigException("no valid server blocks found in configuration file");
    
    return servers;
}