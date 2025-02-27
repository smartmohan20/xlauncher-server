#include "websocket_server.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

SimpleSocketServer::SimpleSocketServer(int port) : _port(port) {
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
    }
}

SimpleSocketServer::~SimpleSocketServer() {
    stop();
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
    
    if (_serverThread.joinable()) {
        _serverThread.join();
    }
    
    if (_listenSocket != INVALID_SOCKET) {
        closesocket(_listenSocket);
        _listenSocket = INVALID_SOCKET;
    }
}

void SimpleSocketServer::runServer() {
    while (_running) {
        // Accept a client socket
        SOCKET clientSocket = accept(_listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            if (_running) {
                std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            }
            continue;
        }
        
        std::cout << "Client connected" << std::endl;
        
        // Add client to our map, initially not handshaked
        _clients[clientSocket] = false;
        
        // Handle WebSocket handshake
        if (handleWebSocketHandshake(clientSocket)) {
            std::cout << "WebSocket handshake successful" << std::endl;
            _clients[clientSocket] = true;
            
            // TODO: In a real implementation, you would handle WebSocket frame parsing here
            // and call _messageHandler when a message is received
        } else {
            std::cout << "WebSocket handshake failed" << std::endl;
            closesocket(clientSocket);
            _clients.erase(clientSocket);
        }
    }
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
        std::string response = generateHandshakeResponse(key);
        
        // Send handshake response
        if (send(clientSocket, response.c_str(), response.length(), 0) == SOCKET_ERROR) {
            std::cerr << "Failed to send handshake response: " << WSAGetLastError() << std::endl;
            return false;
        }
        
        return true;
    }
    
    return false;
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

std::string SimpleSocketServer::computeAcceptKey(const std::string& key) {
    // Concatenate key with WebSocket GUID
    std::string concatenated = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    
    // Compute SHA-1 hash
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[20] = {0};
    DWORD hashLen = sizeof(hash);
    std::string base64Hash;
    
    if (CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
            if (CryptHashData(hHash, (BYTE*)concatenated.c_str(), concatenated.length(), 0)) {
                if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
                    // Base64 encode the hash
                    DWORD base64Len = 0;
                    CryptBinaryToStringA(hash, hashLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &base64Len);
                    
                    char* base64Buffer = new char[base64Len];
                    CryptBinaryToStringA(hash, hashLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, base64Buffer, &base64Len);
                    
                    base64Hash = std::string(base64Buffer, base64Len);
                    delete[] base64Buffer;
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    
    return base64Hash;
}