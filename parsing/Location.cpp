#include "Parser.hpp"
#include "Utils.hpp"
#include <sstream>
#include <cstdlib>

LocationConfig Parser::parseLocation(std::ifstream& file, const std::string& path, bool braceOnSameLine) {
    LocationConfig location;
    location.path = path;
    std::string line;
    int braceCount = 0;
    
    if (braceOnSameLine)
        braceCount = 1;
    else {
        if (std::getline(file, line)) {
            line = Utils::trim(Utils::removeComment(line));
            if (line == "{")
                braceCount = 1;
            else if (line.find('{') != std::string::npos) {
                validateBraceLine(line, '{');
                braceCount = 1;
            } else
                throw ConfigException("expected '{' after location directive, found: " + line);
        }
    }
    
    while (std::getline(file, line) && braceCount > 0) {
        line = Utils::trim(Utils::removeComment(line));
        if (line.empty()) continue;
        
        if (line.find('{') != std::string::npos) {
            validateBraceLine(line, '{');
            braceCount++;
        }
        
        if (line.find('}') != std::string::npos) {
            validateBraceLine(line, '}');
            braceCount--;
            if (braceCount == 0) break; 
            continue;
        }
        
        if (braceCount == 0) break;
        
        std::istringstream iss(line);
        std::string directive;
        iss >> directive;
        
        validateDirectiveLine(line, directive);
        
        if (directive == "methods") {
            std::string method;
            while (iss >> method) {
                method = Utils::removeSemicolon(method);
                if (!method.empty()) {
                    if (method != "GET" && method != "POST" && method != "DELETE") {
                        throw ConfigException("invalid HTTP method: " + method + " (only GET, POST, DELETE are supported)");
                    }
                    location.methods.insert(method);
                }
            }
        }
        else if (directive == "root") {
            iss >> location.root;
            location.root = Utils::removeSemicolon(location.root);
            if (location.root.empty())
                throw ConfigException("root directive requires a path");
            // ensuring that root path doesn't end with / unless it's just /
            if (location.root.length() > 1 && location.root[location.root.length() - 1] == '/')
                location.root = location.root.substr(0, location.root.length() - 1);
        }
        else if (directive == "index") {
            std::string idx;
            location.index.clear();
            bool has_index = false;
            while (iss >> idx) {
                idx = Utils::removeSemicolon(idx);
                if (!idx.empty()) {
                    if (!location.index.empty()) location.index += " ";
                    location.index += idx;
                    has_index = true;
                }
            }
            if (!has_index)
                throw ConfigException("index directive requires at least one file");
        }
        else if (directive == "autoindex") {
            std::string value;
            iss >> value;
            value = Utils::removeSemicolon(value);
            if (value != "on" && value != "off")
                throw ConfigException("autoindex must be 'on' or 'off', got: " + value);
            location.autoindex = (value == "on");
        }
        else if (directive == "upload_path") {
            iss >> location.upload_path;
            location.upload_path = Utils::removeSemicolon(location.upload_path);
            if (location.upload_path.empty())
                throw ConfigException("upload_path directive requires a path");
        }
        else if (directive == "return") {
            std::string code_str;
            iss >> code_str >> location.redirect.second;
            location.redirect.first = std::atoi(code_str.c_str());
            location.redirect.second = Utils::removeSemicolon(location.redirect.second);
            
            if (location.redirect.first < 300 || location.redirect.first > 399)
                throw ConfigException("return directive requires a 3xx status code");
            if (location.redirect.second.empty())
                throw ConfigException("return directive requires a URL");
        }
        else if (directive == "cgi") {
            std::string ext, handler;
            iss >> ext >> handler;
            handler = Utils::removeSemicolon(handler);
            if (ext.empty() || handler.empty())
                throw ConfigException("cgi directive requires extension and handler");
            if (ext[0] != '.')
                throw ConfigException("cgi extension must start with '.': " + ext);
            location.cgi[ext] = handler;
        }
        else if (directive == "client_max_body_size") {
            std::string size;
            iss >> size;
            size = Utils::removeSemicolon(size);
            if (size.empty())
                throw ConfigException("client_max_body_size requires a size");
            location.client_max_body_size = Utils::parseSize(size);
            location.has_body_count = true;
        }
        else {
            throw ConfigException("Unknown directive in location block: " + directive);
        }
    }
    
    if (location.methods.empty())
        location.methods.insert("GET");
    
    if (location.index.empty())
        location.index = "index.html"; 
    
    return location;
}