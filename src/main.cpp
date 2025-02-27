#include "server/server.h"
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    try {
        // Create a server instance on port 2354
        Server server(2354);

        std::cout << "Starting server on port 2354..." << std::endl;

        // Start server directly in main thread for better error handling
        auto [success, errorMsg] = server.run();
        
        if (!success) {
            std::cerr << "Server failed to start: " << errorMsg << std::endl;
            std::cerr << "Possible solutions:" << std::endl;
            std::cerr << "- Try a different port" << std::endl;
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