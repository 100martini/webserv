#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <stdexcept>

class Utils {
private:
    static bool looksLikeIP(const std::string& host);
    
public:
    static std::string trim(const std::string& str);
    static std::string removeComment(const std::string& line);
    static std::string removeSemicolon(const std::string& str); 
    static std::vector<std::string> split(const std::string& str, char delim);
    static size_t parseSize(const std::string& str);
    static bool isNumber(const std::string& str);
    static bool isValidIPv4(const std::string& host);
    static bool isValidHostname(const std::string& host);
    static bool isValidHost(const std::string& host);
};

#endif