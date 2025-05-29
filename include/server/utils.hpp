//
// Created by Filip Sokołowski on 25/05/2025.
//
#pragma once
#include <sys/socket.h>


namespace utils
{
    //* Function totally taken from stack overflow. But it works :>
    inline std::string trim(const std::string& s) {
        const auto start = s.find_first_not_of(" \t\r\n");
        const auto end = s.find_last_not_of(" \t\r\n");
        return (start == std::string::npos || end == std::string::npos)
            ? ""
            : s.substr(start, end - start + 1);
    }

    static std::string readRequest(const int fd) {
        std::vector<char> buffer(4096);
        // Checking if length of the request isn't too long
        if (const ssize_t bytes = recv(fd, buffer.data(), buffer.size() - 1, 0); bytes > 0) {
            buffer[bytes] = '\0';
            return {buffer.data()};
        }
        return {};
    }

    bool inline shouldKeepAlive(const std::string& version, const std::unordered_map<std::string, std::string>& headers) {
        auto it = headers.find("connection");
        const std::string conn = (it != headers.end()) ? headers.at("connection") : "";

        if (version == "HTTP/1.1")
        {
            return conn != "close";
        }
        //* else statys here for the feature
        if (version == "HTTP/1.0")
        {
            return conn == "keep-alive";
        }
        return false;
    };
    //These are used for transparent lookup. This will allow us
    //to skip the conversion inside the unordered map
    //we will not use as much memory
    //sonarqube is the GOAT
    //Example
    /*
        ipLastSeen_.find("192.168.0.1");
        ipLastSeen_.find(std::string_view("192.168.0.1"));
        ipLastSeen_.find(std::string("192.168.0.1"));
     */
    //They all work without copying right now :>
    struct TransparentHash {
        using IsTransparent = void;

        size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }

        size_t operator()(const std::string& s) const noexcept {
            return std::hash<std::string>{}(s);
        }

        size_t operator()(const char* s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    struct TransparentEqual {
        using IsTransparent = void;
        bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
            return lhs == rhs;
        }
    };
    } // namespace utils
