#ifndef CONFIG_HPP
#define CONFIG_HPP
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RED "\033[31m"
#define RESET "\033[0m"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <exception>

struct LocationConfig {
    std::string path;
    std::set<std::string> methods;
    std::string root;
    std::string index;
    bool autoindex;
    std::string upload_path;
    std::pair<int, std::string> redirect;
    std::map<std::string, std::string> cgi;
    size_t client_max_body_size;
    bool has_body_count; //👀
    
    LocationConfig();
};

struct ServerConfig {
    int port;
    std::string host;
    std::string server_name;
    std::map<int, std::string> error_pages;
    size_t client_max_body_size;
    std::vector<LocationConfig> locations;
    
    ServerConfig();
};

class Config {
private:
    std::vector<ServerConfig> _servers;
    std::string _config_file;
    
public:
    Config();
    Config(const std::string& config_file);
    ~Config();
    
    void parse();
    void parse(const std::string& config_file);
    void validate();
    
    const std::vector<ServerConfig>& getServers() const;
    std::vector<ServerConfig>& getServers();
};

class ConfigException : public std::exception {
private:
    std::string _msg;
public:
    ConfigException(const std::string& msg);
    virtual ~ConfigException() throw();
    virtual const char* what() const throw();
};

#endif