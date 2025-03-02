#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <map>
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/sha.h>
#include <nlohmann/json.hpp>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypto")

class SimpleSocketServer {
public:
    using MessageHandler = std::function<std::string(const std::string&)>;
    
    // Constructor
    explicit SimpleSocketServer(int port, const std::string& host = "127.0.0.1");
    
    // Destructor
    ~SimpleSocketServer();
    
    // Prevent copying
    SimpleSocketServer(const SimpleSocketServer&) = delete;
    SimpleSocketServer& operator=(const SimpleSocketServer&) = delete;
    
    // Start the server
    std::pair<bool, std::string> start();
    
    // Stop the server
    void stop();
    
    // Get server port
    int getPort() const { return _port; }
    
    // Set message handler
    void setMessageHandler(MessageHandler handler) { _messageHandler = std::move(handler); }
    
private:
    // Server implementation methods
    void runServer();
    bool handleWebSocketHandshake(SOCKET clientSocket);
    std::string computeAcceptKey(const std::string& key);
    std::string generateHandshakeResponse(const std::string& key);
    std::string decodeWebSocketFrame(const std::vector<uint8_t>& frame);
    std::vector<uint8_t> encodeWebSocketFrame(const std::string& message);
    
    // Server state
    int _port;
    std::string _host;
    SOCKET _listenSocket;
    std::atomic<bool> _running;
    std::thread _serverThread;
    MessageHandler _messageHandler;
    std::map<SOCKET, bool> _clients;
};