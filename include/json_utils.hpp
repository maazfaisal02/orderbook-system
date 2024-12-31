#ifndef JSON_UTILS_HPP
#define JSON_UTILS_HPP

#include <map>
#include <string>

std::string buildJsonString(const std::map<std::string, std::string>& fields);
std::map<std::string, std::string> parseJsonString(const std::string &json);

// Helper for escaping quotes in JSON
std::string escapeJsonString(const std::string &input);

#endif // JSON_UTILS_HPP
