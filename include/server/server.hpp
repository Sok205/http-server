//
// Created by Filip Sokołowski on 22/05/2025.
//
#pragma once

#include <thread>
#include <server/exceptions.hpp>

#ifndef SERVER_HPP
#define SERVER_HPP
//TODO: Add routing
//TODO: Add error handling (for the request that doesn't exist
enum class RequestType {GET = 0, POST = 1, PUT = 2, DELETE = 3};

//Changing string to RequestType.
static RequestType toRequestType(const std::string& requestT)
{
    if (requestT == "GET")
        return RequestType::GET;
    if (requestT == "POST")
        return RequestType::POST;
    if (requestT == "PUT")
        return RequestType::PUT;
    if (requestT == "DELETE")
        return RequestType::DELETE;
    else
        return RequestType::GET;
}

class RequestHandler
{
    public:
        // constructor
        RequestHandler() = default;

        // move, copy operators and assignments
        RequestHandler(const RequestHandler&) = default;
        RequestHandler& operator=(const RequestHandler&) = default;
        RequestHandler(RequestHandler&&) = default;
        RequestHandler& operator=(RequestHandler&&) = default;

        // handling the request
        virtual std::string handler(const std::string& path, const std::string& body) = 0;

        // destructor
        virtual ~RequestHandler() = default;

        // type of request
        virtual RequestType getType() const = 0;

};

class GETHandler final : public RequestHandler
{
    std::string handler(const std::string &path, const std::string &body) override
    {
        return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nGET: " + path;
    }

    RequestType getType() const override
    {
        return RequestType::GET;
    }
};

class POSTHandler final : public RequestHandler
{
    std::string handler(const std::string &path, const std::string &body) override
    {
        return "HTTP/1.1 201 Created\r\nContent-Type: text/plain\r\n\r\nPOST Body: " + body;
    }

    RequestType getType() const override
    {
        return RequestType::POST;
    }
};
//TODO:
// class PUTHandler final : public RequestHandler
// {
//     std::string handler(const std::string &path, const std::string &body) override
// };

//TODO:
// class DELETEHandler final : public RequestHandler
// {
//
// };

class RequestHandlerFactory
{
public:
    static std::unique_ptr<RequestHandler> createHandler(RequestType const type)
    {
        switch (type)
        {
            case RequestType::GET:
                return std::unique_ptr<GETHandler>();
            case RequestType::POST:
                return std::unique_ptr<POSTHandler>();
            default:
                return std::unique_ptr<RequestHandler>();
        }
    }
};

class TcpServer {
public:
    explicit TcpServer(uint16_t port) : serverFd_(::socket(AF_INET, SOCK_STREAM, 0))
    {
        // Creating a socket

        if (serverFd_ < 0)
            throw SocketCreationException("Socket creation failed");


        constexpr int reuse = 1;
        // Setting Socket Options. This allows reusing the same address multiple times
        if (setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
            throw SocketCreationException("Socket option set failed");


        sockaddr_in addr{};
        addr.sin_family = AF_INET; // IPv4
        addr.sin_addr.s_addr = INADDR_ANY; // Accept connections to any available address
        addr.sin_port = htons(port); // Sets the port number

        if (bind(serverFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            if (errno == EADDRINUSE) {
                throw BindException("Bind failed: Port is already in use");
            } else {
                throw BindException(std::string("Bind failed: ") + std::strerror(errno));
            }
        }


        if (listen(serverFd_, 5) != 0) // start listening to incoming connections
            throw ListenException("Listen failed");
    }

    ~TcpServer() = default;

    void run() const {

        std::vector<std::thread> serverThreads;
        while (true) {
            sockaddr_in client_addr{}; // stores information about the client
            socklen_t client_len = sizeof(client_addr); // length of the client address structure
            int client_fd = accept(serverFd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len); // wait for client to connect
            if (client_fd < 0)
                throw AcceptException("Accept failed"); // check if accept was successful

            serverThreads.emplace_back([this, client_fd]() {
                const std::string request = read_request(client_fd); // read the request from the client
                std::cout << "Received request:\n" << request << std::endl; // print the request

                if (check_request(request)){
                  try
                      {
                        std::string method, path, version;
                        std::istringstream stream(request);
                        stream >> method >> path >> version;

                        auto handler = RequestHandlerFactory::createHandler(toRequestType(method));
                        std::string body = extractBody(request);

                        std::string response = handler->handler(path, body);
                        send(client_fd, response.c_str(), response.size(), 0);

                      } catch (const std::exception& e)
                      {
                          std::cout << e.what() << std::endl;
                      }
                  }
                  else {
                      const std::string response_404 = "HTTP/1.1 404 Not Found\r\n\r\n";
                      send(client_fd, response_404.c_str(), response_404.size(), 0);
                  }
                close(client_fd);
            }); //allows thread to run independently of the main thread

        }
        // Joining all the threads
        for (auto& thread : serverThreads)
            thread.join();
    }

private:
    int serverFd_;

    static std::string extractBody(const std::string& request) {
        if (const auto pos = request.find("\r\n\r\n"); pos != std::string::npos) {
            return request.substr(pos + 4);
        }
        return "";
    }

    static std::string read_request(const int fd) {
        std::vector<char> buffer(4096);
        if (const ssize_t bytes = recv(fd, buffer.data(), buffer.size() - 1, 0); bytes > 0) {
            buffer[bytes] = '\0';
            return std::string(buffer.data());
        }
        return {};
    }

    //Check if path exists on the server
    static bool check_request(std::string const& request) {
        const std::regex path_regex(R"(^\S+\s(\S+))");

        if (std::smatch first_match; std::regex_search(request, first_match, path_regex))
        {
            const std::string path = first_match[1].str();
            std::cout << "Path: "<< path << std::endl;

            // Removing the leading '/' from the request

            if (const std::filesystem::path full_path = std::filesystem::current_path() / path.substr(1);
                std::filesystem::exists(full_path))
                return true;
        }
        return false;
    }
};
#endif //SERVER_HPP

