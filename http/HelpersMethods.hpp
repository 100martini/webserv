#ifndef HELPERS_HPP
#define HELPERS_HPP

#include <string>
#include "HttpResponse.hpp"
// #include "../cgi/CGIHandler.hpp"

void serveFile(const std::string& filepath, HttpResponse& response);
std::string getFileExtension(const std::string& filepath);
std::string generateDirectoryListing(const std::string& dir_path, const std::string& uri_path);
bool ensureUploadDirectory(const std::string& path, HttpResponse& response);
bool parseMultipartFormData(const std::string& body, const std::string& boundary, const std::string& upload_path, HttpResponse& response);
bool saveUploadedFile(const std::string& filepath, const std::string& content);
bool saveUploadedBinaryFile(const std::string& filepath, const char* data, size_t length);
std::string extractBoundary(const std::string& content_type);
bool isPathSafe(const std::string& full_path, const std::string& root);

#endif