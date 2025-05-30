#include "server/server.hpp"

#include <__chrono/calendar.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "server/exceptions.hpp"
#include "server/utils.hpp"
#include "tracy/Tracy.hpp"


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
    {
        std::lock_guard lock(clientQueueMutex_);
        stop_ = true;
    }
    clientQueueCond_.notify_all();
    for (auto& t : workerThreads_) {
        if (t.joinable()) t.join();
    }
    if (serverFd_ >= 0) ::close(serverFd_);
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
    //* Setting the number of dispatcher threads
    int numWorkers = 1;
    for (int i = 0; i < numWorkers; ++i) {
        workerThreads_.emplace_back([this] { this->workerLoop(); });
    }

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        int fd = accept(serverFd_, reinterpret_cast<sockaddr*>(&clientAddr), &len);
        if (fd < 0) continue;

        {
            std::lock_guard lock(clientQueueMutex_);
            clientQueue_.push(fd);
        }
        clientQueueCond_.notify_one();
    }
}

void TcpServer::cleanupFinishedThreads() {
    std::lock_guard lock(threadMutex_);
    erase_if(serverThreads_, [](const std::thread& t) {
        return !t.joinable();
    });
}

//* function to block to many requests. Prevents ddos attacks
bool TcpServer::blockTooManyRequests(const std::string& ip)
{
    std::lock_guard lock(ipMutex_);
    const auto now = std::chrono::steady_clock::now();
    if (auto it = lastIp_.find(ip); it != lastIp_.end() && now - it->second < std::chrono::milliseconds(100)) {
        return true; // too soon!
    }
    lastIp_[ip] = now;
    return false;
}

void TcpServer::workerLoop() {
    tracy::SetThreadName("Worker");
    const auto threadId = std::this_thread::get_id();
    {
        std::lock_guard lock(loggingMutex_);
        std::cout << "[DISPATCH] Worker started: Thread ID = " << threadId << '\n';
    }

    while (true) {
        int clientFd;
        {
            std::unique_lock lock(clientQueueMutex_);
            clientQueueCond_.wait(lock, [this] {
                return stop_ || !clientQueue_.empty();
            });
            if (stop_ && clientQueue_.empty()) return;
            clientFd = clientQueue_.front();
            clientQueue_.pop();
        }

        {
            std::lock_guard lock(loggingMutex_);
            std::cout << "[DISPATCH] Worker Thread " << threadId << " handling client fd = " << clientFd << '\n';
        }
        handleClient(clientFd);
    }
}

void TcpServer::handleClient(int clientFd)
{
    tracy::SetThreadName("ClientHandler");
    {
        std::lock_guard lock(loggingMutex_);
        std::cout << "[HANDLE] Thread" << std::this_thread::get_id() << " handling client fd = " << clientFd << '\n';
    }
    // Atomic clock implementation
    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    //*getpeername -> return peer address of connected socket (https://pubs.opengroup.org/onlinepubs/007904875/functions/getpeername.html)
    getpeername(clientFd, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);

    // allocating buffer space to store Ipv4 address
    std::string clientIP(INET_ADDRSTRLEN, '\0');
    // converting the ip address from binary to 'human' xd
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP.data(), INET_ADDRSTRLEN);
    clientIP.resize(std::strlen(clientIP.c_str()));

    if (blockTooManyRequests(clientIP))
    {
        const std::string response = "HTTP/1.1 429 Too Many Requests\r\n\r\n";
        send(clientFd, response.data(), response.size(), 0);
    }


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

    if (clientFd < 0) {
        std::cerr << "[ERROR] Invalid clientFd\n";
        return;
    }

    close(clientFd);
}

std::string TcpServer::extractBody(const std::string& request) {
    auto pos = request.find("\r\n\r\n");
    return pos==std::string::npos ? "" : request.substr(pos+4);
}

void::TcpServer::dispatchLoop()
{
    while (true)
    {
        int clientFd;
        {
            //* Pulling socket from the thread, then removing it from queue and handling
            //* returning from wait
            std::unique_lock lock(clientQueueMutex_);
            //* [this] capturing the current pointer to client
            clientQueueCond_.wait(lock, [this] {
                return !clientQueue_.empty();
            });

            clientFd = clientQueue_.front();
            clientQueue_.pop();
        }
        cleanupFinishedThreads();
        serverThreads_.emplace_back([this, clientFd] {
            handleClient(clientFd);
        });
    }
}


int main() {
    /*Todo:
     *add more logs to the server
     *maybe not doing everythin inside hpp file
     */
    try {
        Router routerA, routerB;
        routerA.addRoute(RequestType::GET, "/hello", [](const std::string&, const std::string&) {
            return "Hello from portA !";
        });

        routerA.addRoute(RequestType::PUT, "/goodbye", [](const std::string&, const std::string&) {
            return "Goodbye from Port A!";
        });

        routerB.addRoute(RequestType::GET, "/hello", [](const std::string&, const std::string&) {
            return "Hello from portB !";
        });
        TcpServer serverA(4222, routerA);
        TcpServer serverB(4444, routerB);
        std::cout << "Waiting for a client to connect...\n";

        std::thread serverAThread([&serverA]() {
            serverA.run();
        });

        std::thread serverBThread([&serverB]() {
            serverB.run();
        });

        if (serverAThread.joinable())
        {
            serverAThread.join();
        }

        if (serverBThread.joinable())
        {
            serverBThread.join();
        }

    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    return 0;
}


