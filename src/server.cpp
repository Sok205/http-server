#include "server/server.hpp"
#include "server/exceptions.hpp"
#include "server/utils.hpp"
#include "tracy/Tracy.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

static router::RequestType toRequestType(const std::string& requestT) {
    using enum router::RequestType;
    if (requestT == "GET")    {return GET;}
    if (requestT == "POST")   {return POST;}
    if (requestT == "PUT")    {return PUT;}
    if (requestT == "DELETE") {return DELETE;}
    throw std::invalid_argument("Invalid request type: " + requestT);
}


std::unique_ptr<router::RequestHandler>
router::RequestHandlerFactory::createHandler(const RequestType type) {
    switch (type) {
        using enum RequestType;
        case GET:    return std::make_unique<GETHandler>();
        case POST:   return std::make_unique<POSTHandler>();
        case PUT:    return std::make_unique<PUTHandler>();
        case DELETE: return std::make_unique<DELETEHandler>();
    }
    throw std::invalid_argument("Unsupported request type");
}


std::string router::GETHandler::handler(const std::string& path,const std::string&body){
    return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nGET: " + path + "\n" + body;
}
router::RequestType router::GETHandler::getType() const { return RequestType::GET; }

std::string router::PUTHandler::handler(const std::string& path, const std::string& body) {
    return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nPUT: " + path + "\n" + body;
}

router::RequestType router::PUTHandler::getType() const {
    return RequestType::PUT;
}

std::string router::POSTHandler::handler(const std::string& path, const std::string& body) {
    return "HTTP/1.1 201 Created\r\nContent-Type: text/plain\r\n\r\nPOST Body: " + body + "\r\n" + path;
}

router::RequestType router::POSTHandler::getType() const {
    return RequestType::POST;
}

std::string router::DELETEHandler::handler(const std::string& path, const std::string& body) {
    return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nDELETE: " + path + "\r\n" + body;
}

router::RequestType router::DELETEHandler::getType() const {
    return RequestType::DELETE;
}



void router::Router::addRoute(RequestType type, const std::string& path, RouteHandler handler) {
    routes_[{type, path}] = std::move(handler);
}

router::RouteHandler router::Router::getHandler(RequestType type, const std::string& path) const {
    auto it = routes_.find({type, path});
    return it != routes_.end() ? it->second : nullptr;
}


server::TcpServer::TcpServer(uint16_t port, router::Router& router)
  : serverFd_(::socket(AF_INET, SOCK_STREAM, 0)), router_(router)
{
    if (serverFd_ < 0)
    {
        throw exceptions::SocketCreationException("Could not create socket");
    }

    if (constexpr int reuse = 1; setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        throw exceptions::SocketOptionSet("Setsockopt failed");
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(serverFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        throw exceptions::BindException("Socket bind failed");
    }

    if (listen(serverFd_, 5) != 0)
    {
        throw exceptions::ListenException("listen failed");
    }
}

server::TcpServer::~TcpServer() {
    {
        std::lock_guard const lock(clientQueueMutex_);
        stop_ = true;
    }
    clientQueueCond_.notify_all();
    for (std::thread &t : workerThreads_)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
    if (serverFd_ >= 0) { ::close(serverFd_);
}
}

server::TcpServer::TcpServer(TcpServer&& other) noexcept
  : serverFd_(other.serverFd_), router_(other.router_)
{
    other.serverFd_ = -1;
}

server::TcpServer& server::TcpServer::operator=(TcpServer&& other) noexcept {
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

auto server::TcpServer::run() -> void
{
    ZoneScoped; //NOLINT
    //* Setting the number of dispatcher threads
    constexpr int numWorkers = 4;
    for (int i = 0; i < numWorkers; ++i) {
        workerThreads_.emplace_back([this] { this->workerLoop(); });
    }

    while (true) {
        ZoneScopedN("AcceptConnection"); //NOLINT
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        int const fd = accept(serverFd_, reinterpret_cast<sockaddr*>(&clientAddr), &len);
        if (fd < 0)
        {
            continue;
        }
        {
            ZoneScopedN("QueueClient"); //NOLINT
            const std::lock_guard lock(clientQueueMutex_);
            clientQueue_.push(fd);
        }
        clientQueueCond_.notify_one();
    }
}

void server::TcpServer::cleanupFinishedThreads()
{
    const std::lock_guard lock(threadMutex_);
    erase_if(serverThreads_, [](const std::thread& t) {
        return !t.joinable();
    });
}

//* function to block to many requests. Prevents ddos attacks
bool server::TcpServer::blockTooManyRequests(const std::string& ip)
{
    const std::lock_guard lock(ipMutex_);
    const auto now = std::chrono::steady_clock::now();
    if (const auto it = lastIp_.find(ip); it != lastIp_.end() && now - it->second < std::chrono::milliseconds(1)) {
        return true;
    }
    lastIp_[ip] = now;
    return false;
}

void server::TcpServer::workerLoop()
{

    tracy::SetThreadName("WorkerThread");
    ZoneScoped; //NOLINT
    const auto threadId = std::this_thread::get_id();
    {
        const std::lock_guard lock(loggingMutex_);
        std::cout << "[DISPATCH] Worker started: Thread ID = " << threadId << '\n';
    }

    while (true) {
        int clientFd = 0;
        {
            ZoneScopedN("WaitForClient"); //NOLINT
            std::unique_lock lock(clientQueueMutex_);
            clientQueueCond_.wait(lock, [this] {
                return stop_ || !clientQueue_.empty();
            });
            if (stop_ && clientQueue_.empty())
            {
                return;
            }
            clientFd = clientQueue_.front();
            clientQueue_.pop();
        }

        {
            const std::lock_guard lock(loggingMutex_);
            std::cout << "[DISPATCH] Worker Thread " << threadId << " handling client fd = " << clientFd << '\n';
        }
        handleClient(clientFd);
    }
}

void server::TcpServer::handleClient(int clientFd)
{
    ZoneScopedN("TcpServer::handleClient"); //NOLINT
    {
        ZoneScopedN("GetClientInfo"); //NOLINT
        const std::lock_guard lock(loggingMutex_);
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
        ZoneScopedN("RateLimit"); //NOLINT
        const std::string response = "HTTP/1.1 429 Too Many Requests\r\n\r\n";
        send(clientFd, response.data(), response.size(), 0);
        close(clientFd);
        return;
    }


    while (true) {
        ZoneScopedN("ProcessRequest"); //NOLINT
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
            ZoneScopedN("HandleRoute"); //NOLINT
            const router::RequestType type = toRequestType(method);
            const std::string body = extractBody(request);
            const router::RouteHandler routeHandler = router_.getHandler(type, path);

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
        } catch (exceptions::HandlerException&) {
            ZoneScopedN("HandleError"); //NOLINT
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

auto server::TcpServer::extractBody(const std::string &request) -> std::string
{
    auto pos = request.find("\r\n\r\n");
    return pos==std::string::npos ? "" : request.substr(pos+4);
}

auto server::TcpServer::dispatchLoop() -> void
{
    tracy::SetThreadName("Dispatcher");
    ZoneScoped; //NOLINT
    while (true)
    {
        int clientFd = 0;
        {
            //* Pulling socket from the thread, then removing it from queue and handling
            //* returning from wait
            ZoneScopedN("TcpServer::dispatchLoop"); //NOLINT
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


int main()
{
    tracy::SetThreadName("MainThread");
    ZoneScoped; //NOLINT
    try {
        router::Router routerA;
        router::Router routerB;
        routerA.addRoute(router::RequestType::GET, "/hello", [](const std::string&, const std::string&) {
            ZoneScoped; //NOLINT
            return "Hello from portA !";
        });

        routerA.addRoute(router::RequestType::PUT, "/goodbye", [](const std::string&, const std::string&) {
            ZoneScoped; //NOLINT
            return "Goodbye from Port A!";
        });

        routerB.addRoute(router::RequestType::GET, "/hello", [](const std::string&, const std::string&) {
            ZoneScoped; //NOLINT
            return "Hello from portB !";
        });
        server::TcpServer serverA(4222, routerA);
        server::TcpServer serverB(4444, routerB);
        std::cout << "Waiting for a client to connect...\n";

        std::thread serverAThread([&serverA]() {
            tracy::SetThreadName("TcpServer::serverAThread");
            ZoneScoped; //NOLINT
            serverA.run();
        });

        std::thread serverBThread([&serverB]() {
            tracy::SetThreadName("TcpServer::serverBThread");
            ZoneScoped; //NOLINT
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
        std::cerr << ex.what() << '\n';
        return 1;
    }
    return 0;
}


