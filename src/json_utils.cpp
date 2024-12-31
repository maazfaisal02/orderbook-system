#include "json_utils.hpp"
#include <sstream>
#include <utility>

std::string escapeJsonString(const std::string &input) {
    std::ostringstream ss;
    ss << "\"";
    for (char c : input) {
        if (c == '\"') {
            ss << "\\\"";
        } else if (c == '\\') {
            ss << "\\\\";
        } else {
            ss << c;
        }
    }
    ss << "\"";
    return ss.str();
}

std::string buildJsonString(const std::map<std::string, std::string> &fields) {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (auto &kv : fields) {
        if (!first) {
            oss << ",";
        }
        first = false;
        oss << escapeJsonString(kv.first) << ":" << escapeJsonString(kv.second);
    }
    oss << "}";
    return oss.str();
}

std::map<std::string, std::string> parseJsonString(const std::string &json) {
    // Very naive parser
    std::map<std::string, std::string> result;
    size_t pos = 0;
    while (true) {
        auto startKey = json.find("\"", pos);
        if (startKey == std::string::npos) break;
        auto endKey = json.find("\"", startKey + 1);
        if (endKey == std::string::npos) break;
        std::string key = json.substr(startKey + 1, endKey - (startKey + 1));

        auto colonPos = json.find(":", endKey);
        if (colonPos == std::string::npos) break;
        auto startVal = json.find("\"", colonPos);
        if (startVal == std::string::npos) break;
        auto endVal = json.find("\"", startVal + 1);
        if (endVal == std::string::npos) break;
        std::string val = json.substr(startVal + 1, endVal - (startVal + 1));

        result[key] = val;
        pos = endVal + 1;
    }

    return result;
}
