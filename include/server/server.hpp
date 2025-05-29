#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#


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

class Router {
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

class TcpServer {
public:
    TcpServer(uint16_t port, Router& router);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) noexcept;
    TcpServer& operator=(TcpServer&&) noexcept;

    void run();

private:
    int serverFd_;
    Router& router_;

    std::atomic<bool> running_{true};
    std::mutex threadMutex_;
    //? jthread implementation
    std::vector<std::thread> serverThreads_;

    void cleanupFinishedThreads();
    void handleClient(int clientFd);
    std::string extractBody(const std::string& request);
};