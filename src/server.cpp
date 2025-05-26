#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <regex>
#include <filesystem> // Used for going through file system
#include <server/server.hpp>
#include <thread>
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