//
// Created by Filip Sokołowski on 21/05/2025.
//

#ifndef SERVER_HPP
#define SERVER_HPP

class TcpServer {
public:
    explicit TcpServer(uint16_t port) : serverFd_(::socket(AF_INET, SOCK_STREAM, 0)) {
        // Creating a socket

        if (serverFd_ < 0) throw std::runtime_error("Failed to create socket");

        int reuse = 1;
        // Setting Socket Options. This allows reusing the same address multiple times
        if (setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
            throw std::runtime_error("setsockopt failed");

        sockaddr_in addr{};
        addr.sin_family = AF_INET; // IPv4
        addr.sin_addr.s_addr = INADDR_ANY; // Accept connections to any available address
        addr.sin_port = htons(port); // Sets the port number

        if (bind(serverFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) // bind the socket to this address and port
            throw std::runtime_error("Failed to bind");

        if (listen(serverFd_, 5) != 0) // start listening to incoming connections
            throw std::runtime_error("Listen failed");
    }

    ~TcpServer() = default;

    void run() {
        while (true) {
            sockaddr_in client_addr{}; // stores information about the client
            socklen_t client_len = sizeof(client_addr); // length of the client address structure
            int client_fd = accept(serverFd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len); // wait for client to connect
            if (client_fd < 0) throw std::runtime_error("Accept failed"); // check if accept was successful

            std::string request = read_request(client_fd); // read the request from the client
            std::cout << "Received request:\n" << request << std::endl; // print the request

            if (check_request(request)){
                std::string response = "HTTP/1.1 200 OK\r\n\r\n";
                send(client_fd, response.c_str(), response.size(), 0);
            }
            else {
                std::string response_404 = "HTTP/1.1 404 Not Found\r\n\r\n";
                send(client_fd, response_404.c_str(), response_404.size(), 0);
            }

            close(client_fd);
        }
    }

private:
    int serverFd_;

    std::string read_request(int fd) {
        std::vector<char> buffer(4096);
        ssize_t bytes = recv(fd, buffer.data(), buffer.size() - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            return std::string(buffer.data());
        }
        return {};
    }

    //Check if path exists on the server
    static bool check_request(std::string const& request) {
        std::regex path_regex(R"(^\S+\s(\S+))");

        if (std::smatch first_match; std::regex_search(request, first_match, path_regex))
        {
            std::string path = first_match[1].str();
            std::cout << "Path: "<< path << std::endl;

            //Removing the leading '/' from the request
            std::filesystem::path full_path = std::filesystem::current_path() / path.substr(1);

            if (std::filesystem::exists(full_path))
                return true;
        }
        return false;
    }
};
#endif //SERVER_HPP
