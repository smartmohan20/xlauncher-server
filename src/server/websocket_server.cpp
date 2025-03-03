#include "websocket_server.h"
#include "utils/base64.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <vector>
#include <windows.h>
#include <wincrypt.h>
#include <openssl/sha.h>

// Base64 encoding function for WebSocket handshake
static std::string ws_base64Encode(const unsigned char* data, size_t length) {
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
SimpleSocketServer::SimpleSocketServer(int port, const std::string& host) 
    : _port(port), _host(host), _running(false), _listenSocket(INVALID_SOCKET) {
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

// Helper to disambiguate from winsock send function
int SimpleSocketServer::rawSend(SOCKET client, const char* data, int length, int flags) {
    return ::send(client, data, length, flags);
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
    service.sin_addr.s_addr = inet_addr(_host.c_str());
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
    
    std::cout << "Server listening on " << _host << ":" << _port << std::endl;
    
    _running = true;
    _serverThread = std::thread(&SimpleSocketServer::runServer, this);
    
    return {true, "Server started successfully"};
}

void SimpleSocketServer::stop() {
    if (!_running) return;
    
    _running = false;
    
    // Close all client sockets
    {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        for (const auto& client : _clients) {
            closesocket(client.first);
        }
        _clients.clear();
    }
    
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
        
        // Add client sockets to the set
        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            for (const auto& client : _clients) {
                FD_SET(client.first, &readfds);
            }
        }
        
        // Set timeout
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout
        
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
                std::lock_guard<std::mutex> lock(_clientsMutex);
                _clients[clientSocket] = true;
            } else {
                closesocket(clientSocket);
            }
        }
        
        // Check for activity on client sockets
        std::vector<SOCKET> clientsToRead;
        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            for (const auto& client : _clients) {
                if (FD_ISSET(client.first, &readfds)) {
                    clientsToRead.push_back(client.first);
                }
            }
        }
        
        // Process client messages outside of the lock
        for (SOCKET clientSocket : clientsToRead) {
            char buffer[8192]; // Increased buffer size for binary data
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            
            if (bytesReceived <= 0) {
                // Client disconnected
                std::lock_guard<std::mutex> lock(_clientsMutex);
                closesocket(clientSocket);
                _clients.erase(clientSocket);
                std::cout << "Client disconnected" << std::endl;
                continue;
            }
            
            // Process WebSocket frame
            std::vector<uint8_t> frameData(buffer, buffer + bytesReceived);
            processWebSocketFrame(clientSocket, frameData);
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
    return ws_base64Encode(sha1Hash, SHA_DIGEST_LENGTH);
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
        if (rawSend(clientSocket, response.c_str(), response.length(), 0) == SOCKET_ERROR) {
            std::cerr << "Failed to send handshake response: " << WSAGetLastError() << std::endl;
            return false;
        }
        
        std::cout << "Sent handshake response:\n" << response << std::endl;
        return true;
    }
    
    return false;
}

void SimpleSocketServer::processWebSocketFrame(SOCKET clientSocket, const std::vector<uint8_t>& frame) {
    if (frame.empty()) return;

    // Parse WebSocket frame header
    bool fin = (frame[0] & 0x80) != 0;
    int opcode = frame[0] & 0x0F;
    bool masked = (frame[1] & 0x80) != 0;
    
    // Get payload length
    uint64_t payloadLength = frame[1] & 0x7F;
    size_t headerOffset = 2;
    
    // Handle extended payload length
    if (payloadLength == 126) {
        if (frame.size() < 4) return; // Not enough data
        
        payloadLength = (static_cast<uint16_t>(frame[2]) << 8) | frame[3];
        headerOffset = 4;
    } else if (payloadLength == 127) {
        if (frame.size() < 10) return; // Not enough data
        
        payloadLength = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLength = (payloadLength << 8) | frame[2 + i];
        }
        headerOffset = 10;
    }
    
    // Get masking key
    std::vector<uint8_t> maskingKey;
    if (masked) {
        if (frame.size() < headerOffset + 4) return; // Not enough data
        
        maskingKey.assign(frame.begin() + headerOffset, frame.begin() + headerOffset + 4);
        headerOffset += 4;
    }
    
    // Check if we have enough data
    if (frame.size() < headerOffset + payloadLength) return;
    
    // Extract payload data
    std::vector<uint8_t> payloadData(frame.begin() + headerOffset, frame.begin() + headerOffset + payloadLength);
    
    // Unmask the data if needed
    if (masked) {
        for (size_t i = 0; i < payloadData.size(); ++i) {
            payloadData[i] ^= maskingKey[i % 4];
        }
    }
    
    // Handle different opcodes
    switch (opcode) {
        case 0x1: // Text frame
            if (_messageHandler) {
                std::string textMessage(payloadData.begin(), payloadData.end());
                std::string response = _messageHandler(textMessage);
                
                if (!response.empty()) {
                    sendMessage(clientSocket, response);
                }
            }
            break;
            
        case 0x2: // Binary frame
            if (_binaryMessageHandler) {
                _binaryMessageHandler(clientSocket, payloadData);
            }
            break;
            
        case 0x8: // Close frame
            {
                std::lock_guard<std::mutex> lock(_clientsMutex);
                closesocket(clientSocket);
                _clients.erase(clientSocket);
                std::cout << "Client closed connection" << std::endl;
            }
            break;
            
        case 0x9: // Ping frame
            // Send pong frame
            {
                std::vector<uint8_t> pongFrame;
                pongFrame.push_back(0x8A); // FIN bit set, Pong frame
                
                // Payload length
                if (payloadData.size() <= 125) {
                    pongFrame.push_back(static_cast<uint8_t>(payloadData.size()));
                } else if (payloadData.size() <= 65535) {
                    pongFrame.push_back(126);
                    pongFrame.push_back((payloadData.size() >> 8) & 0xFF);
                    pongFrame.push_back(payloadData.size() & 0xFF);
                } else {
                    pongFrame.push_back(127);
                    for (int i = 7; i >= 0; --i) {
                        pongFrame.push_back((payloadData.size() >> (i * 8)) & 0xFF);
                    }
                }
                
                // Add payload (echo back the ping payload)
                pongFrame.insert(pongFrame.end(), payloadData.begin(), payloadData.end());
                
                // Send pong frame
                rawSend(clientSocket, reinterpret_cast<const char*>(pongFrame.data()), pongFrame.size(), 0);
            }
            break;
            
        case 0xA: // Pong frame
            // Ignore, we don't need to do anything
            break;
            
        default:
            std::cerr << "Unknown WebSocket opcode: " << opcode << std::endl;
            break;
    }
}

std::vector<uint8_t> SimpleSocketServer::encodeWebSocketFrame(const std::string& message) {
    std::vector<uint8_t> frame;
    
    // First byte: FIN bit set, text frame
    frame.push_back(0x81);
    
    // Payload length
    if (message.length() <= 125) {
        frame.push_back(static_cast<uint8_t>(message.length()));
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

std::vector<uint8_t> SimpleSocketServer::encodeBinaryWebSocketFrame(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> frame;
    
    // First byte: FIN bit set, binary frame
    frame.push_back(0x82);
    
    // Payload length
    if (data.size() <= 125) {
        frame.push_back(static_cast<uint8_t>(data.size()));
    } else if (data.size() <= 65535) {
        frame.push_back(126);
        frame.push_back((data.size() >> 8) & 0xFF);
        frame.push_back(data.size() & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back((data.size() >> (i * 8)) & 0xFF);
        }
    }
    
    // Add payload
    frame.insert(frame.end(), data.begin(), data.end());
    
    return frame;
}

bool SimpleSocketServer::sendMessage(SOCKET client, const std::string& message) {
    std::vector<uint8_t> frame = encodeWebSocketFrame(message);
    return rawSend(client, reinterpret_cast<const char*>(frame.data()), frame.size(), 0) != SOCKET_ERROR;
}

bool SimpleSocketServer::broadcastMessage(const std::string& message) {
    std::vector<uint8_t> frame = encodeWebSocketFrame(message);
    
    std::lock_guard<std::mutex> lock(_clientsMutex);
    for (const auto& client : _clients) {
        if (rawSend(client.first, reinterpret_cast<const char*>(frame.data()), frame.size(), 0) == SOCKET_ERROR) {
            std::cerr << "Failed to send message to client: " << WSAGetLastError() << std::endl;
        }
    }
    
    return true;
}

bool SimpleSocketServer::sendBinaryMessage(SOCKET client, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> frame = encodeBinaryWebSocketFrame(data);
    return rawSend(client, reinterpret_cast<const char*>(frame.data()), frame.size(), 0) != SOCKET_ERROR;
}

bool SimpleSocketServer::broadcastBinaryMessage(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> frame = encodeBinaryWebSocketFrame(data);
    
    std::lock_guard<std::mutex> lock(_clientsMutex);
    for (const auto& client : _clients) {
        if (rawSend(client.first, reinterpret_cast<const char*>(frame.data()), frame.size(), 0) == SOCKET_ERROR) {
            std::cerr << "Failed to send binary message to client: " << WSAGetLastError() << std::endl;
        }
    }
    
    return true;
}