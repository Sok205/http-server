#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <server/utils.hpp>
#include <tracy/Tracy.hpp>

namespace router
{
class Router;

using RouteHandler = std::function<std::string(const std::string&, const std::string&)>;

enum class RequestType : std::uint8_t { GET = 0, POST, PUT, DELETE };


RequestType toRequestType(const std::string& requestT);



class RequestHandler {
public:
    RequestHandler() = default;

    virtual ~RequestHandler() = default;
    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;
    RequestHandler(RequestHandler&&) = delete;
    RequestHandler& operator=(RequestHandler&&) = delete;

    virtual std::string handler(const std::string& path, const std::string& body) = 0;
    virtual RequestType getType() const = 0;
};

class GETHandler final : public RequestHandler {
public:
    std::string handler(const std::string& path, const std::string& body) override;
    RequestType getType() const override;
};
class POSTHandler final : public RequestHandler {
public:
    std::string handler(const std::string& path, const std::string& body) override;
    RequestType getType() const override;
};

class PUTHandler final : public RequestHandler {
public:
    std::string handler(const std::string& path, const std::string& body) override;
    RequestType getType() const override;
};

class DELETEHandler final : public RequestHandler {
public:
    std::string handler(const std::string& path, const std::string& body) override;
    RequestType getType() const override;
};

class RequestHandlerFactory {
public:
    static std::unique_ptr<RequestHandler> createHandler(RequestType type);
};

class Router
{
public:
    void addRoute(RequestType type, const std::string& path, RouteHandler handler);
    RouteHandler getHandler(RequestType type, const std::string& path) const;

private:
    struct Hash {
        std::size_t operator()(const std::pair<RequestType, std::string>& p) const {
            const auto h1 = std::hash<int>()(std::to_underlying(p.first));
            const auto h2 = std::hash<std::string>()(p.second);
            return h1 ^ (h2 << 1);
        }
    };
    std::unordered_map<std::pair<RequestType, std::string>, RouteHandler, Hash> routes_;
};
};//namespace router

namespace server
{
class TcpServer {
public:
    TcpServer(uint16_t port, router::Router& router);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) noexcept;
    TcpServer& operator=(TcpServer&&) noexcept;

    void run();

private:
    int serverFd_;
    router::Router& router_;
    // ! Don't know how to make it work xd
    std::unordered_map<
        std::string,
        std::chrono::steady_clock::time_point,
        utils::TransparentHash,
        utils::TransparentEqual
    > lastIp_;

    std::mutex ipMutex_;

    std::atomic<bool> running_{true};
    std::mutex threadMutex_;
    //? jthread implementation
    std::vector<std::thread> serverThreads_;

    //*Dispatcher implementiation
    std::queue<int> clientQueue_;
    std::mutex clientQueueMutex_;
    std::condition_variable clientQueueCond_; //? https://en.cppreference.com/w/cpp/thread/condition_variable.html
    std::thread dispatcherThread_;
    std::vector<std::thread> workerThreads_;
    void workerLoop();
    bool stop_ = false;
    void dispatchLoop();

    //*Loggin
    std::mutex loggingMutex_;


    void cleanupFinishedThreads();
    void handleClient(int clientFd);
    bool blockTooManyRequests(const std::string& ip);
    static std::string extractBody(const std::string& request);

};
}
