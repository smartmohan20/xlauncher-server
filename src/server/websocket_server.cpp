#include "websocket_server.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <vector>
#include <windows.h>
#include <wincrypt.h>
#include <openssl/sha.h>

// Base64 encoding function
std::string base64Encode(const unsigned char* data, size_t length) {
    static const char base64Chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string encoded;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (length--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                encoded += base64Chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int j = 0; j < i + 1; j++)
            encoded += base64Chars[char_array_4[j]];

        while (i++ < 3)
            encoded += '=';
    }

    return encoded;
}

// Constructor
SimpleSocketServer::SimpleSocketServer(int port) : _port(port), _running(false), _listenSocket(INVALID_SOCKET) {
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        throw std::runtime_error("WSAStartup failed");
    }
}

// Destructor
SimpleSocketServer::~SimpleSocketServer() {
    // Stop the server if it's running
    stop();

    // Cleanup Winsock
    WSACleanup();
}

std::pair<bool, std::string> SimpleSocketServer::start() {
    if (_running) return {true, "Server is already running"};
    
    // Create a socket
    _listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_listenSocket == INVALID_SOCKET) {
        return {false, "Socket creation failed: " + std::to_string(WSAGetLastError())};
    }
    
    // Set up the sockaddr structure
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("127.0.0.1");
    service.sin_port = htons(_port);
    
    // Bind the socket
    if (bind(_listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        closesocket(_listenSocket);
        return {false, "Bind failed with error: " + std::to_string(WSAGetLastError())};
    }
    
    // Start listening for connections
    if (listen(_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(_listenSocket);
        return {false, "Listen failed with error: " + std::to_string(WSAGetLastError())};
    }
    
    std::cout << "Server listening on port " << _port << std::endl;
    
    _running = true;
    _serverThread = std::thread(&SimpleSocketServer::runServer, this);
    
    return {true, "Server started successfully"};
}

void SimpleSocketServer::stop() {
    if (!_running) return;
    
    _running = false;
    
    // Close all client sockets
    for (const auto& client : _clients) {
        closesocket(client.first);
    }
    _clients.clear();
    
    // Join the server thread
    if (_serverThread.joinable()) {
        _serverThread.join();
    }
    
    // Close listen socket
    if (_listenSocket != INVALID_SOCKET) {
        closesocket(_listenSocket);
        _listenSocket = INVALID_SOCKET;
    }
}

void SimpleSocketServer::runServer() {
    fd_set readfds;
    struct timeval tv;
    
    while (_running) {
        // Reset the socket set
        FD_ZERO(&readfds);
        FD_SET(_listenSocket, &readfds);
        
        // Set timeout
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        // Wait for activity on socket
        int activity = select(0, &readfds, NULL, NULL, &tv);
        
        if (activity == SOCKET_ERROR) {
            std::cerr << "Select error: " << WSAGetLastError() << std::endl;
            break;
        }
        
        // If something happened on the listening socket
        if (FD_ISSET(_listenSocket, &readfds)) {
            SOCKET clientSocket = accept(_listenSocket, NULL, NULL);
            
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
                continue;
            }
            
            std::cout << "Client connected" << std::endl;
            
            // Handle WebSocket handshake
            if (handleWebSocketHandshake(clientSocket)) {
                _clients[clientSocket] = true;
                
                // Start receiving messages
                char buffer[4096];
                int bytesReceived;
                
                while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
                    std::vector<uint8_t> frameData(buffer, buffer + bytesReceived);
                    std::string decodedMessage = decodeWebSocketFrame(frameData);
                    
                    if (!decodedMessage.empty() && _messageHandler) {
                        // Process message and get response
                        std::string response = _messageHandler(decodedMessage);
                        
                        if (!response.empty()) {
                            // Encode and send response
                            std::vector<uint8_t> responseFrame = encodeWebSocketFrame(response);
                            send(clientSocket, 
                                 reinterpret_cast<const char*>(responseFrame.data()), 
                                 responseFrame.size(), 
                                 0);
                        }
                    }
                }
                
                // Client disconnected
                closesocket(clientSocket);
                _clients.erase(clientSocket);
            } else {
                closesocket(clientSocket);
            }
        }
    }
}

std::string SimpleSocketServer::computeAcceptKey(const std::string& key) {
    // WebSocket GUID as defined in RFC 6455
    const std::string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    
    // Concatenate the key with the GUID
    std::string concatenated = key + GUID;
    
    // Compute SHA-1 hash
    unsigned char sha1Hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(concatenated.c_str()), concatenated.length(), sha1Hash);
    
    // Base64 encode the hash
    return base64Encode(sha1Hash, SHA_DIGEST_LENGTH);
}

std::string SimpleSocketServer::generateHandshakeResponse(const std::string& key) {
    std::string acceptKey = computeAcceptKey(key);
    
    // Create handshake response
    std::stringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << acceptKey << "\r\n"
             << "\r\n";
    
    return response.str();
}

bool SimpleSocketServer::handleWebSocketHandshake(SOCKET clientSocket) {
    char buffer[4096] = {0};
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    
    if (bytesReceived <= 0) {
        return false;
    }
    
    std::string request(buffer, bytesReceived);
    std::cout << "Received handshake request:\n" << request << std::endl;
    
    // Extract the Sec-WebSocket-Key using regex
    std::regex keyRegex("Sec-WebSocket-Key: ([^\r\n]+)");
    std::smatch matches;
    
    if (std::regex_search(request, matches, keyRegex) && matches.size() > 1) {
        std::string key = matches[1].str();
        // Trim any whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        
        std::string response = generateHandshakeResponse(key);
        
        // Send handshake response
        if (send(clientSocket, response.c_str(), response.length(), 0) == SOCKET_ERROR) {
            std::cerr << "Failed to send handshake response: " << WSAGetLastError() << std::endl;
            return false;
        }
        
        std::cout << "Sent handshake response:\n" << response << std::endl;
        return true;
    }
    
    return false;
}

std::string SimpleSocketServer::decodeWebSocketFrame(const std::vector<uint8_t>& frame) {
    if (frame.empty()) return "";

    // Basic WebSocket frame decoding
    bool fin = (frame[0] & 0x80) != 0;
    int opcode = frame[0] & 0x0F;
    bool masked = (frame[1] & 0x80) != 0;
    
    // Handle different payload lengths
    uint64_t payload_length = frame[1] & 0x7F;
    size_t offset = 2;
    
    // Extended payload length
    if (payload_length == 126) {
        payload_length = (frame[2] << 8) | frame[3];
        offset = 4;
    } else if (payload_length == 127) {
        payload_length = 0;
        for (int i = 0; i < 8; ++i) {
            payload_length = (payload_length << 8) | frame[2 + i];
        }
        offset = 10;
    }
    
    // Masking key
    std::vector<uint8_t> maskingKey;
    if (masked) {
        maskingKey.insert(maskingKey.end(), 
                          frame.begin() + offset, 
                          frame.begin() + offset + 4);
        offset += 4;
    }
    
    // Extract payload
    std::vector<uint8_t> payload(
        frame.begin() + offset, 
        frame.begin() + offset + payload_length
    );
    
    // Unmask payload if necessary
    if (masked && !maskingKey.empty()) {
        for (size_t i = 0; i < payload.size(); ++i) {
            payload[i] ^= maskingKey[i % 4];
        }
    }
    
    // Convert payload to string
    return std::string(payload.begin(), payload.end());
}

std::vector<uint8_t> SimpleSocketServer::encodeWebSocketFrame(const std::string& message) {
    std::vector<uint8_t> frame;
    
    // First byte: FIN bit set, text frame
    frame.push_back(0x81);
    
    // Payload length
    if (message.length() <= 125) {
        frame.push_back(message.length());
    } else if (message.length() <= 65535) {
        frame.push_back(126);
        frame.push_back((message.length() >> 8) & 0xFF);
        frame.push_back(message.length() & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back((message.length() >> (i * 8)) & 0xFF);
        }
    }
    
    // Add payload
    frame.insert(frame.end(), message.begin(), message.end());
    
    return frame;
}