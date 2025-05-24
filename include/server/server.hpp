//
// Created by Filip Sokołowski on 22/05/2025.
//
#pragma once

#include <csignal>
#include <mutex>
#include <server/exceptions.hpp>
#include <thread>
#include <functional>
#include <unordered_map>

#ifndef SERVER_HPP
#define SERVER_HPP


using RouteHandler = std::function<std::string(const std::string&, const std::string&)>;

//----------------------------Requests---------------------------------
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
        throw std::invalid_argument("Invalid request type" + requestT);
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

class PUTHandler final : public RequestHandler
{
    std::string handler(const std::string &path, const std::string &body) override
    {
        return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nPUT: " + path + "\n" + body;
    }

    RequestType getType() const override
    {
        return RequestType::PUT;
    }
};

class DELETEHandler final : public RequestHandler
{
    std::string handler(const std::string &path, const std::string &body) override
    {
        return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nDELETE: " + path;
    }

    RequestType getType() const override
    {
        return RequestType::DELETE;
    }
};

class RequestHandlerFactory
{
public:
    static std::unique_ptr<RequestHandler> createHandler(RequestType const type)
    {
        switch (type)
        {
            case RequestType::GET:
                return std::make_unique<GETHandler>();
            case RequestType::POST:
                return std::make_unique<POSTHandler>();
            case RequestType::PUT:
                return std::make_unique<PUTHandler>();
            case RequestType::DELETE:
                return std::make_unique<DELETEHandler>();
            default:
                throw std::invalid_argument("Unsupported request type");
        }
    }
};
//----------------------------Requests---------------------------------

class Router
{
    public:
        void addRoute(RequestType const type,const std::string& path ,RouteHandler handler)
        {
            routes_[{type, path}] = std::move(handler);
        }

        RouteHandler getHandler(RequestType const type, const std::string& path) const
        {
            // If route was found returning RouteHandler of given route. Else return nullptr
            if (const auto it = routes_.find({type, path}); it != routes_.end())
                return it->second;
            return nullptr;
        }

    private:
        struct Hash {
            std::size_t operator()(const std::pair<RequestType, std::string>& p) const {
                const std::size_t h1 = std::hash<int>()(static_cast<int>(p.first));
                const std::size_t h2 = std::hash<std::string>()(p.second);
                return h1 ^ (h2 << 1);
            }
        };

        // Hashing the map
        std::unordered_map<std::pair<RequestType, std::string>, RouteHandler, Hash> routes_;
};

//----------------------------TCPSERVER---------------------------------
class TcpServer {
public:

    TcpServer(uint16_t port, Router& router) : serverFd_(::socket(AF_INET, SOCK_STREAM, 0)), router_(router)
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

        //TODO: free the threads after closing the server or add closing the thread
        while (running) {
            sockaddr_in client_addr{}; // stores information about the client
            socklen_t client_len = sizeof(client_addr); // length of the client address structure
            int client_fd = accept(serverFd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len); // wait for client to connect
            if (client_fd < 0)
                throw AcceptException("Accept failed"); // check if accept was successful

            serverThreads.emplace_back([this, client_fd]() {handleClient(client_fd);});//allows thread to run independently of the main thread


        }
        //Joins all the threads after the execution of the server
        for (auto& thread : serverThreads)
            if (thread.joinable())
                thread.join();
    }

private:
    // Server
    int serverFd_;

    // Check if server should run
    std::atomic<bool> running{true};

    // Router
    Router& router_;

    // Mutexes
    inline static std::mutex m_request;

    void handleClient(int client_fd) const
    {
        std::lock_guard<std::mutex> lock(m_request);
        const std::string request = read_request(client_fd); // read the request from the client
        std::cout << "Received request:\n" << request << std::endl; // print the request

            try
            {
                std::cout << request << std::endl;
                std::string method, path, version;
                std::istringstream stream(request);
                stream >> method >> path >> version;

                RequestType type = toRequestType(method);
                std::string body = extractBody(request);
                RouteHandler routeHandler = router_.getHandler(type, path);

                std::string response;
                if (routeHandler) {

                    std::string content = routeHandler(path, body);
                    std::ostringstream oss;
                    oss << "HTTP/1.1 200 OK\r\n"
                        << "Content-Type: text/plain\r\n"
                        << "Content-Length: " << content.size() << "\r\n"
                        << "Connection: close\r\n\r\n"
                        << content;

                    response = oss.str();
                    send(client_fd, response.c_str(), response.size(), 0);

                } else {
                    response = "HTTP/1.1 404 Not Found\r\n\r\nRoute not found";
                }

                send(client_fd, response.c_str(), response.size(), 0);

            } catch (const std::exception& e)
            {
                std::cout << e.what() << std::endl;
                const std::string response_500 = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                send(client_fd, response_500.c_str(), response_500.size(), 0);
            }
        close(client_fd);
        }
    //     else {
    //         const std::string response_404 = "HTTP/1.1 404 Not Found\r\n\r\n";
    //         send(client_fd, response_404.c_str(), response_404.size(), 0);
    //     }
    // }

    // extract body from request
    static std::string extractBody(const std::string& request) {
        if (const auto pos = request.find("\r\n\r\n"); pos != std::string::npos) {
            return request.substr(pos + 4);
        }
        return "";
    }

    // reads the whole request
    static std::string read_request(const int fd) {
        std::vector<char> buffer(4096);
        if (const ssize_t bytes = recv(fd, buffer.data(), buffer.size() - 1, 0); bytes > 0) {
            buffer[bytes] = '\0';
            return std::string(buffer.data());
        }
        return {};
    }

    //Useless right now but keep it just in case xD
    // //Check if path exists on the server
    // static bool check_request(std::string const& request) {
    //     const std::regex path_regex(R"(^\S+\s(\S+))");
    //
    //     if (std::smatch first_match; std::regex_search(request, first_match, path_regex))
    //     {
    //         const std::string path = first_match[1].str();
    //         std::cout << "Path: "<< path << std::endl;
    //
    //         // Removing the leading '/' from the request
    //         if (const std::filesystem::path full_path = std::filesystem::current_path() / path.substr(1);
    //             std::filesystem::exists(full_path))
    //             return true;
    //     }
    //     return false;
    // };
};

//----------------------------TCPSERVER---------------------------------

#endif //SERVER_HPP

