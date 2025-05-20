#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

   // Creating a socket
   int server_fd = socket(AF_INET, SOCK_STREAM, 0); //AF_INET -> will use IPv4 family, SOCK_STREAM -> stream-based connection,
   if (server_fd < 0) {
    std::cerr << "Failed to create server socket\n";
    return 1;
   }

   // Setting Socket Options. This allows reausing the same address multiple times
   int reuse = 1;
   if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
     std::cerr << "setsockopt failed\n";
     return 1;
   }

   struct sockaddr_in server_addr; // Creates structure that hold the server's address and port
   server_addr.sin_family = AF_INET; //
   server_addr.sin_addr.s_addr = INADDR_ANY; // Will accept connections to any available
   server_addr.sin_port = htons(4221); // Sets the port number

   // binds the socket to this address port
   if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
     std::cerr << "Failed to bind to port 4221\n";
     return 1;
   }

   int connection_backlog = 5; // Maximum number of pending connections
   if (listen(server_fd, connection_backlog) != 0) { // start listening to incoming connections
     std::cerr << "listen failed\n";
     return 1;
   }

   struct sockaddr_in client_addr;
   int client_addr_len = sizeof(client_addr); // stores information about the client

   std::cout << "Waiting for a client to connect...\n";

   int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len); // wait for client to connect
   std::cout << "Client connected\n";

   class response_serv
   {
     public:
       std::string http_version;
       std::string status;
       std::string reason_phrase;
       std::string crlf;

       //Constructor for our class
       response_serv()
       : http_version("HTTP/1.1")
       , status("200")
       , reason_phrase("OK")
       , crlf("\r\n")
       {}

   };
   response_serv response = response_serv();

   // Creating a response for succesfull connection
   std::string responseStr = response.http_version + " " + response.status + " " + response.reason_phrase + response.crlf + response.crlf;

   // Sending response back to the client -> int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
   send(client_fd, responseStr.c_str(), responseStr.length(), 0);

   close(server_fd);
   close(client_fd);

  return 0;
}
