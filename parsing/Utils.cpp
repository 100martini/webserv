#include "Utils.hpp"
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <stdexcept>

std::string Utils::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

std::string Utils::removeComment(const std::string& line) {
    size_t pos = line.find('#');
    if (pos != std::string::npos)
        return line.substr(0, pos);
    return line;
}

std::string Utils::removeSemicolon(const std::string& str) {
    size_t pos = str.find(';');
    if (pos != std::string::npos)
        return str.substr(0, pos);
    return str;
}

std::vector<std::string> Utils::split(const std::string& str, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
        token = trim(token);
        if (!token.empty())
            tokens.push_back(token);
    }
    return tokens;
}

size_t Utils::parseSize(const std::string& str) {
    if (str.empty()) {
        throw std::runtime_error("empty size string");
    }
    
    size_t value = 0;
    std::string num_str;
    char unit = 0;
    
    size_t i = 0;
    while (i < str.length() && (::isdigit(str[i]) || str[i] == '.')) {
        num_str += str[i];
        i++;
    }
    
    if (i < str.length())
        unit = str[i];
    
    std::stringstream ss(num_str);
    ss >> value;
    
    if (ss.fail())
        throw std::runtime_error("invalid size format: " + str);
    
    switch (::toupper(unit)) {
        case 'G': value *= 1024UL * 1024UL * 1024UL; break;
        case 'M': value *= 1024UL * 1024UL; break;
        case 'K': value *= 1024UL; break;
        case '\0':
        case 'B':
            break;
        default:
            throw std::runtime_error("invalid size unit: " + std::string(1, unit));
    }
    
    return value;
}

bool Utils::isNumber(const std::string& str) {
    if (str.empty()) return false;
    for (size_t i = 0; i < str.length(); i++)
        if (!::isdigit(str[i])) return false;
    return true;
}

bool Utils::looksLikeIP(const std::string& host) {
    std::vector<std::string> parts = split(host, '.');
    
    if (parts.empty() || parts.size() == 1) return false;
    
    if (parts.size() == 4) return true;
    
    bool allNumericLooking = true;
    for (size_t i = 0; i < parts.size(); i++) {
        if (!isNumber(parts[i])) {
            allNumericLooking = false;
            break;
        }
    }
    
    return allNumericLooking;
}

bool Utils::isValidIPv4(const std::string& host) {
    if (host.empty()) return false;
    
    if (host[0] == '.' || host[host.length() - 1] == '.') return false;
    
    if (host.find("..") != std::string::npos) return false;
    
    std::vector<std::string> parts;
    std::stringstream ss(host);
    std::string token;
    while (std::getline(ss, token, '.'))
        parts.push_back(token);
    
    if (parts.size() != 4) return false;
    
    for (size_t i = 0; i < parts.size(); i++) {
        if (parts[i].empty()) return false;
        
        if (!isNumber(parts[i])) return false;
        
        if (parts[i].length() > 1 && parts[i][0] == '0') return false;
        
        int num = std::atoi(parts[i].c_str());
        if (num < 0 || num > 255) return false;
    }
    
    return true;
}

bool Utils::isValidHostname(const std::string& host) {
    if (host.empty() || host.length() > 253) return false;
    
    if (host == "localhost" || host == "*") return true;
    
    if (host[0] == '.' || host[host.length() - 1] == '.')
        return false;
    
    if (host.find("..") != std::string::npos)
        return false;

    if (looksLikeIP(host)) return false;
    
    if (host.find('.') == std::string::npos) return false;
    
    std::vector<std::string> labels = split(host, '.');
    if (labels.empty()) return false;
    
    for (size_t i = 0; i < labels.size(); i++) {
        const std::string& label = labels[i];
        
        if (label.empty() || label.length() > 63) return false;
        
        if (!std::isalnum(label[0]) || !std::isalnum(label[label.length() - 1]))
            return false;
        
        for (size_t j = 0; j < label.length(); j++)
            if (!std::isalnum(label[j]) && label[j] != '-')
                return false;
        if (label.find("--") != std::string::npos) return false;
    }
    
    return true;
}

bool Utils::isValidHost(const std::string& host) {
    return isValidIPv4(host) || isValidHostname(host);
}