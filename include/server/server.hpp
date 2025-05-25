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
#include <utility>
#include "utils.hpp"

#ifndef SERVER_HPP
#define SERVER_HPP


using RouteHandler = std::function<std::string(const std::string&, const std::string&)>;

//----------------------------Requests---------------------------------
enum class RequestType : std::uint8_t {GET = 0, POST = 1, PUT = 2, DELETE = 3};

//Changing string to RequestType.
static RequestType toRequestType(const std::string& requestT)
{
    if (requestT == "GET")
    {
        return RequestType::GET;
    }
    if (requestT == "POST")
    {
        return RequestType::POST;
    }
    if (requestT == "PUT")
    {
        return RequestType::PUT;
    }
    if (requestT == "DELETE")
    {
        return RequestType::DELETE;
    }

    throw std::invalid_argument("Invalid request type" + requestT);

}

class RequestHandler
{
    public:
        //* constructor
        RequestHandler() = default;

        //* move, copy operators and assignments
        RequestHandler(const RequestHandler&) = default;
        RequestHandler& operator=(const RequestHandler&) = default;
        RequestHandler(RequestHandler&&) = default;
        RequestHandler& operator=(RequestHandler&&) = default;

        //* handling the request
        virtual std::string handler(const std::string& path, const std::string& body) = 0;

        //* destructor
        virtual ~RequestHandler() = default;

        //* type of request
        virtual RequestType getType() const = 0;

};

class GETHandler final : public RequestHandler
{
    std::string handler(const std::string &path, const std::string &body) override
    {
        return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nGET: " + path + "\n" + body;
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
        return "HTTP/1.1 201 Created\r\nContent-Type: text/plain\r\n\r\nPOST Body: " + body + "\r\n" + path;
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
        return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nDELETE: " + path + "\r\n" + body;
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
            using enum RequestType;
            case GET:
                return std::make_unique<GETHandler>();
            case POST:
                return std::make_unique<POSTHandler>();
            case PUT:
                return std::make_unique<PUTHandler>();
            case DELETE:
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
            //* If route was found returning RouteHandler of given route. Else return nullptr
            if (const auto it = routes_.find({type, path}); it != routes_.end())
                {
                return it->second;
                }
            return nullptr;
        }

    private:
        struct Hash {
            std::size_t operator()(const std::pair<RequestType, std::string>& p) const {
                const std::size_t h1 = std::hash<int>()
                    (std::to_underlying(p.first)); //! Sonar Qube: Use "std::to_underlying" to cast enums to their underlying type. Link: https://en.cppreference.com/w/cpp/types/underlying_type
                const std::size_t h2 = std::hash<std::string>()(p.second);
                return h1 ^ (h2 << 1);
            }
        };

        //* Hashing the map
        std::unordered_map<std::pair<RequestType, std::string>, RouteHandler, Hash> routes_;
};

//----------------------------TCPSERVER---------------------------------
class TcpServer {
public:

    TcpServer(uint16_t port, Router& router) : serverFd_(::socket(AF_INET, SOCK_STREAM, 0)), router_(router)
    {
        // Creating a socket

        if (serverFd_ < 0)
            {
                throw SocketCreationException("Socket creation failed");
            }


        constexpr int reuse = 1;
        //* Setting Socket Options. This allows reusing the same address multiple times
        if (setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
            {
                throw SocketCreationException("Socket option set failed");
            }


        sockaddr_in addr{};
        addr.sin_family = AF_INET; // IPv4
        addr.sin_addr.s_addr = INADDR_ANY; // Accept connections to any available address
        addr.sin_port = htons(port); // Sets the port number

        if (bind(serverFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            if (errno == EADDRINUSE) {
                throw BindException("Bind failed: Port is already in use");
            }
            throw BindException(std::string("Bind failed: ") + std::strerror(errno));
        }

        //* start listening to incoming connections
        if (listen(serverFd_, 5) != 0)
        {
            throw ListenException("Listen failed");
        }
    }
    //* Copy constructor (we cannot safely copy this, that is why we use delete)
    TcpServer(const TcpServer&) = delete;

    //* Copy assignment, also unsafe
    TcpServer& operator=(const TcpServer&) = delete;

    //* We can move it, but first we set others serverFd_ to minus one so the destructor won't close the second socket again
    TcpServer(TcpServer&& other) noexcept
    : serverFd_(other.serverFd_), router_(other.router_) {
        other.serverFd_ = -1;
    }

    //* Closing existing socket then moving TCPSERVER
    TcpServer& operator=(TcpServer&& other) noexcept {
        if (this != &other) {
            if (serverFd_ >= 0) {
                ::close(serverFd_);
            }
            serverFd_ = other.serverFd_;
            router_ = other.router_;
            other.serverFd_ = -1;
        }
        return *this;
    }

    //* Closing the socket after destruction of the class
    ~TcpServer() {
        if (serverFd_ >= 0) {
            ::close(serverFd_);
        }
    }

    void run() {

        //? Ask professor about "jthread"
        std::vector<std::thread> serverThreads;

        while (running_) {
            sockaddr_in clientAddr{}; //* stores information about the client
            socklen_t socklen = sizeof(clientAddr); //* length of the client address structure
            int const clientFd = accept(serverFd_, reinterpret_cast<sockaddr*>(&clientAddr), &socklen); // wait for client to connect
            if (clientFd< 0)
            {
                throw AcceptException("Accept failed"); //* check if accept was successful
            }


            serverThreads.emplace_back([this, clientFd]() {handleClient(clientFd);});//*allows thread to run independently of the main thread


        }
        //*Joins all the threads after the execution of the server
        for (auto& thread : serverThreads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
    }

private:
    // Server
    int serverFd_;

    // Check if server should run
    std::atomic<bool> running_{true};

    // Router
    Router& router_;

    // Mutexes
    inline static std::mutex m_request;

    void handleClient(int clientFd)
    {
        //? Ask professor about using scopelock instead of lock_guard
        std::lock_guard<std::mutex> lock(m_request);


        while (true)
        {
            const std::string request = readRequest(clientFd);

            if (request.empty())
            {
                break;
            }
            std::istringstream iss(request);
            std::string method;
            std::string path;
            std::string version;

            //* Skipping the whitespace and getting the next character after the white space
            iss >> method >> path >> version;

            //* Parsing the headers
            std::unordered_map<std::string, std::string> headers;
            std::string line;
            std::getline(iss, line);  //* Reading characters from input stream and placing them into string:
            //https://en.cppreference.com/w/cpp/string/basic_string/getline

            //* "Connection: keep-alive" -> headers["connection"] = "keep-alive"
            while (std::getline(iss, line) && line != "\r" && !line.empty())
            {
                if (const auto currentPos = line.find(':'); currentPos != std::string::npos)
                {
                    std::string key = trim(line.substr(0, currentPos));
                    const std::string value = trim(line.substr(currentPos + 1));

                    std::ranges::transform(key, key.begin(), [](const unsigned char c) { return std::tolower(c); });

                    headers[key] = value;
                }
            }

            try
            {
                const RequestType type = toRequestType(method);
                const std::string body = extractBody(request);
                const RouteHandler routeHandler = router_.getHandler(type, path);

                std::string response;
                if (routeHandler)
                {
                    const std::string content = routeHandler(path, body);
                    std::ostringstream oss;
                    oss << "HTTP/1.1 200 OK\r\n"
                        << "Content-Type: text/plain\r\n"
                        << "Content-Length: " << content.size() << "\r\n"
                        << "Connection: " << (shouldKeepAlive(version, headers) ? "keep-alive" : "close") << "\r\n\r\n"
                        << content;

                    response = oss.str();
                    send(clientFd, response.c_str(), response.size(), 0);
                    std::cout << "Connection header: " << headers["connection"] << '\n';
                    std::cout << "Will keep alive: " << std::boolalpha << shouldKeepAlive(version, headers) << '\n';
                }
                else
                {
                    response = "HTTP/1.1 404 Not Found\r\n\r\nRoute not found";
                }

                send(clientFd, response.c_str(), response.size(), 0);
            }
            catch (HandlerException &)
            {
                const std::string response500 = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                send(clientFd, response500.c_str(), response500.size(), 0);
            }
            close(clientFd);
        }
    }
    //* extract body from request
    //! SonarQube: Replace this const reference to "std::string" by a "std::string_view": https://en.cppreference.com/w/cpp/string/basic_string_view
    static std::string extractBody(const std::string& request) {
        if (const auto pos = request.find("\r\n\r\n"); pos != std::string::npos) {
            return request.substr(pos + 4);
        }
        return "";
    }


    // reads the whole request

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

