#include "parsing/Config.hpp"
#include "server/Server.hpp"
#include <iostream>
int main(int argc, char** argv) {
    try {
        std::string config_file = (argc > 1) ? argv[1] : "webserv.conf";
        Config config(config_file);
        
        const std::vector<ServerConfig>& servers = config.getServers();
        
        std::cout << "Successfully parsed " << servers.size() << " servers, remove the (s) for me if its just one okay? <3\n\n";
        
        for (size_t i = 0; i < servers.size(); i++) {
            std::cout << GREEN << "server " << i + 1  << ":\n" << RESET;
            std::cout << "  listens on: " << YELLOW << servers[i].host << ":" << servers[i].port << RESET << "\n";
            std::cout << "  name of server: " << servers[i].server_name << "\n";
            std::cout << "  max_bodycount: " << servers[i].client_max_body_size << " bytes\n";
            //std::cout << "  how many " <<   RED << "error pages? " << RESET << servers[i].error_pages.size() << "\n";
            std::cout << "  locations: " << servers[i].locations.size() << "\n";
            
            for (size_t j = 0; j < servers[i].locations.size(); j++) {
                const LocationConfig& loc = servers[i].locations[j];
                std::cout << "    - " << loc.path << " (";
                for (std::set<std::string>::const_iterator it = loc.methods.begin(); 
                     it != loc.methods.end(); ++it) {
                    if (it != loc.methods.begin()) std::cout << " ";
                    std::cout << *it;
                }
                std::cout << ")\n";
                if (!loc.root.empty())
                    std::cout << "      root: " << loc.root << "\n";
                if (loc.redirect.first > 0)
                    std::cout << "      redirect: " << loc.redirect.first << " -> " << loc.redirect.second << "\n";
            }
            if(i != servers.size() -1)
                std::cout << "\n";
        }
        
        Server server(servers);
        server.start();
        server.run();
        
    } catch (const std::exception& e) {
        std::cerr << RED << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}