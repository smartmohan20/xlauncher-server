#include "server/server.h"
#include "dotenv.h"
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    try {
        // Load environment variables from .env file
        dotenv::init();
        
        // Get PORT from environment or use default (2354)
        int port = 2354;  // Default port
        
        // Try to get PORT from environment
        auto port_it = dotenv::env.find("PORT");
        if (port_it != dotenv::env.end()) {
            try {
                port = std::stoi(port_it->second);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Invalid PORT value in .env file, using default: " << port << std::endl;
            }
        }
        
        // Create a server instance with the port from environment
        Server server(port);

        std::cout << "Starting server on port " << port << "..." << std::endl;

        // Start server directly in main thread for better error handling
        auto [success, errorMsg] = server.run();
        
        if (!success) {
            std::cerr << "Server failed to start: " << errorMsg << std::endl;
            std::cerr << "Possible solutions:" << std::endl;
            std::cerr << "- Try a different port (current: " << port << ")" << std::endl;
            std::cerr << "- Check if you have any antivirus or firewall blocking socket operations" << std::endl;
            std::cerr << "- Verify no other application is using this port" << std::endl;
            return 1;
        }

        std::cout << "Server started successfully on port " << server.getPort() << std::endl;
        std::cout << "Press Ctrl+C to stop the server..." << std::endl;

        // Keep the main thread alive with a simple loop
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}