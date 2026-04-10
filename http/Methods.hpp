#ifndef METHODS_HPP
#define METHODS_HPP

#include <string>
#include "../parsing/Config.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

void handleGet(const HttpRequest& request, LocationConfig* location, HttpResponse& response, bool & cgi_requested);
void handlePost(const HttpRequest& request, LocationConfig* location, const ServerConfig* server_config, HttpResponse& response, bool & cgi_requested);
void handleDelete(const HttpRequest& request, LocationConfig* location, HttpResponse& response);

#endif