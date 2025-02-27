#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

class SimpleSocketServer {
public:
    using MessageHandler = std::function<void(const std::string&)>;
    
    SimpleSocketServer(int port);
    ~SimpleSocketServer();
    
    // Returns success/failure and error message
    std::pair<bool, std::string> start();
    void stop();
    int getPort() const { return _port; }
    void setMessageHandler(MessageHandler handler) { _messageHandler = std::move(handler); }
    
private:
    void runServer();
    bool handleWebSocketHandshake(SOCKET clientSocket);
    std::string generateHandshakeResponse(const std::string& key);
    std::string computeAcceptKey(const std::string& key);
    
    int _port;
    SOCKET _listenSocket = INVALID_SOCKET;
    std::atomic<bool> _running{false};
    std::thread _serverThread;
    MessageHandler _messageHandler;
    std::map<SOCKET, bool> _clients;  // Track clients and handshake status
};