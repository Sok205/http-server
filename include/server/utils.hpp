//
// Created by Filip Sokołowski on 25/05/2025.
//
#pragma once

#ifndef UTILS_HPP

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
    else if (version == "HTTP/1.0")
    {
        return conn == "keep-alive";
    }
    return false;
}

#define UTILS_HPP

#endif //UTILS_HPP
