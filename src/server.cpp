#include "server/server.hpp"
#include "server/exceptions.hpp"
#include "server/utils.hpp"
#include <arpa/inet.h>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <mutex>

// ———  toRequestType  —————————————
RequestType toRequestType(const std::string& requestT) {
    using enum RequestType;
    if (requestT == "GET")    {return GET;}
    if (requestT == "POST")   {return POST;}
    if (requestT == "PUT")    {return PUT;}
    if (requestT == "DELETE") {return DELETE;}
    throw std::invalid_argument("Invalid request type: " + requestT);
}

// ———  RequestHandlerFactory  —————————————
std::unique_ptr<RequestHandler>
RequestHandlerFactory::createHandler(const RequestType type) {
    switch (type) {
        using enum RequestType;
        case GET:    return std::make_unique<GETHandler>();
        case POST:   return std::make_unique<POSTHandler>();
        case PUT:    return std::make_unique<PUTHandler>();
        case DELETE: return std::make_unique<DELETEHandler>();
    }
    throw std::invalid_argument("Unsupported request type");
}

// ———  GET/POST/PUT/DELETE handlers  —————————————
std::string GETHandler::handler(const std::string& path,const std::string&body){
    return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nGET: " + path + "\n" + body;
}
RequestType GETHandler::getType() const { return RequestType::GET; }

std::string PUTHandler::handler(const std::string& path, const std::string& body) {
    return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nPUT: " + path + "\n" + body;
}

RequestType PUTHandler::getType() const {
    return RequestType::PUT;
}

std::string POSTHandler::handler(const std::string& path, const std::string& body) {
    return "HTTP/1.1 201 Created\r\nContent-Type: text/plain\r\n\r\nPOST Body: " + body + "\r\n" + path;
}

RequestType POSTHandler::getType() const {
    return RequestType::POST;
}

std::string DELETEHandler::handler(const std::string& path, const std::string& body) {
    return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nDELETE: " + path + "\r\n" + body;
}

RequestType DELETEHandler::getType() const {
    return RequestType::DELETE;
}


// ———  Router  —————————————
void Router::addRoute(RequestType type, const std::string& path, RouteHandler handler) {
    routes_[{type, path}] = std::move(handler);
}

RouteHandler Router::getHandler(RequestType type, const std::string& path) const {
    auto it = routes_.find({type, path});
    return it != routes_.end() ? it->second : nullptr;
}


// ———  TcpServer implementation  —————————————
TcpServer::TcpServer(uint16_t port, Router& router)
  : serverFd_(::socket(AF_INET, SOCK_STREAM, 0)), router_(router)
{
    if (serverFd_ < 0)
    {
        throw SocketCreationException("Could not create socket");
    }

    if (constexpr int reuse = 1; setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        throw SocketOptionSet("Setsockopt failed");
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(serverFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        throw BindException("Socket bind failed");
    }

    if (listen(serverFd_, 5) != 0)
    {
        throw ListenException("listen failed");
    }
}

TcpServer::~TcpServer() {
    if (serverFd_ >= 0)
    {
        ::close(serverFd_);
    }
}

TcpServer::TcpServer(TcpServer&& other) noexcept
  : serverFd_(other.serverFd_), router_(other.router_)
{
    other.serverFd_ = -1;
}

TcpServer& TcpServer::operator=(TcpServer&& other) noexcept {
    if (this != &other) {
        if (serverFd_ >= 0)
        {
            ::close(serverFd_);
        }

        serverFd_      = other.serverFd_;
        router_        = other.router_;
        other.serverFd_= -1;
    }
    return *this;
}

void TcpServer::run() {
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        int const fd  = accept(serverFd_, reinterpret_cast<sockaddr*>(&clientAddr), &len);
        if (fd < 0)
        {
            break;
        }
        cleanupFinishedThreads();
        serverThreads_.emplace_back([this,fd]{ handleClient(fd); });
    }
    for (auto& t : serverThreads_)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
}

void TcpServer::cleanupFinishedThreads() {
    std::lock_guard lock(threadMutex_);
    erase_if(serverThreads_, [](const std::thread& t) {
        return !t.joinable();
    });
}

void TcpServer::handleClient(int clientFd)
{
    while (true) {
        const std::string request = utils::readRequest(clientFd);

        if (request.empty()) {
            break;
        }

        std::istringstream iss(request);
        std::string method;
        std::string path;
        std::string version;
        iss >> method >> path >> version;

        std::unordered_map<std::string, std::string> headers;
        std::string line;
        std::getline(iss, line);

        while (std::getline(iss, line) && line != "\r" && !line.empty()) {
            if (const auto currentPos = line.find(':'); currentPos != std::string::npos) {
                std::string key = utils::trim(line.substr(0, currentPos));
                const std::string value = utils::trim(line.substr(currentPos + 1));
                std::ranges::transform(key, key.begin(), [](const unsigned char c) { return std::tolower(c); });
                headers[key] = value;
            }
        }

        try {
            const RequestType type = toRequestType(method);
            const std::string body = extractBody(request);
            const RouteHandler routeHandler = router_.getHandler(type, path);

            std::string response;
            if (routeHandler) {
                const std::string content = routeHandler(path, body);
                std::ostringstream oss;
                oss << "HTTP/1.1 200 OK\r\n"
                    << "Content-Type: text/plain\r\n"
                    << "Content-Length: " << content.size() << "\r\n"
                    << "Connection: " << (utils::shouldKeepAlive(version, headers) ? "keep-alive" : "close") << "\r\n\r\n"
                    << content;

                response = oss.str();
                send(clientFd, response.c_str(), response.size(), 0);
                std::cout << "Connection header: " << headers["connection"] << '\n';
                std::cout << "Will keep alive: " << std::boolalpha << utils::shouldKeepAlive(version, headers) << '\n';
            } else {
                response = "HTTP/1.1 404 Not Found\r\n\r\nRoute not found";
                send(clientFd, response.c_str(), response.size(), 0);
            }
        } catch (HandlerException&) {
            const std::string response500 = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            send(clientFd, response500.c_str(), response500.size(), 0);
        }
    }

    close(clientFd);
}

std::string TcpServer::extractBody(const std::string& request) {
    auto pos = request.find("\r\n\r\n");
    return pos==std::string::npos ? "" : request.substr(pos+4);
}



int main() {
    /*Todo:
     *add more logs to the server
     *maybe not doing everythin inside hpp file
     */
    try {
        Router router;
        router.addRoute(RequestType::GET, "/hello", [](const std::string&, const std::string&) {
            return "Hello from /hello!";
        });

        router.addRoute(RequestType::PUT, "/goodbye", [](const std::string&, const std::string&) {
            return "Goodbye from /goodbye!";
        });

        TcpServer server(4222, router);
        std::cout << "Waiting for a client to connect...\n";

        std::thread serverThread([&server]() {
            server.run();
        });


        serverThread.join();

    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    return 0;
}


