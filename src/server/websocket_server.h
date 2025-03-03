#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/sha.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypto")

class SimpleSocketServer {
public:
    using MessageHandler = std::function<std::string(const std::string&)>;
    using BinaryMessageHandler = std::function<void(SOCKET, const std::vector<uint8_t>&)>;
    
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
    
    // Set binary message handler
    void setBinaryMessageHandler(BinaryMessageHandler handler) { _binaryMessageHandler = std::move(handler); }
    
    // Send binary message to all clients
    bool broadcastBinaryMessage(const std::vector<uint8_t>& data);
    
    // Send binary message to specific client
    bool sendBinaryMessage(SOCKET client, const std::vector<uint8_t>& data);
    
    // Send text message to all clients
    bool broadcastMessage(const std::string& message);
    
    // Send text message to specific client
    bool sendMessage(SOCKET client, const std::string& message);
    
    // Get client count
    size_t getClientCount() const { return _clients.size(); }
    
private:
    // Server implementation methods
    void runServer();
    bool handleWebSocketHandshake(SOCKET clientSocket);
    std::string computeAcceptKey(const std::string& key);
    std::string generateHandshakeResponse(const std::string& key);
    void processWebSocketFrame(SOCKET clientSocket, const std::vector<uint8_t>& frame);
    std::vector<uint8_t> encodeWebSocketFrame(const std::string& message);
    std::vector<uint8_t> encodeBinaryWebSocketFrame(const std::vector<uint8_t>& data);
    
    // Helper method to use the raw Winsock send
    int rawSend(SOCKET client, const char* data, int length, int flags);
    
    // Server state
    int _port;
    std::string _host;
    SOCKET _listenSocket;
    std::atomic<bool> _running;
    std::thread _serverThread;
    MessageHandler _messageHandler;
    BinaryMessageHandler _binaryMessageHandler;
    std::map<SOCKET, bool> _clients;
    std::mutex _clientsMutex;
};