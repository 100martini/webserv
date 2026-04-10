#ifndef __CGI_HELPER_HPP_
#define __CGI_HELPER_HPP_

#include "../http/HttpRequest.hpp"
#include "../parsing/Config.hpp"
#include "../http/HttpResponse.hpp"

#include <sstream>
#include <sys/wait.h>
#include <cstdlib>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <sched.h>
#include <vector>
#include <fcntl.h>
#include <cctype>

std::string convertHeaderName(const std::string& headerName);
std::vector<std::string> prepareEnv(const HttpRequest* _request, const ServerConfig* servConfig, const std::string& _script_filename);
std::vector<std::string> prepareEnvComplete(const HttpRequest* _request, const ServerConfig* servConfig, const std::string& _script_filename);
char** vectorToCharArray(const std::vector<std::string>& vec);
std::string extractValueAfterColon(const std::string& line);
void setCGIResponseHeaders(HttpResponse& response, const std::string& headers);
HttpResponse processCGIOutput(const std::string& cgi_output);
void freeCharArray(char** arr);
bool setFdNonBlocking(int fd);

#endif